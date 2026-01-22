import logging
from pathlib import Path
import re
import struct
from typing import Any, BinaryIO, Optional, List
import csv

from min_logger.builder import MetricEntryData, THREAD_NAME_MSG_ID, SEVERITY_LEVELS, ProfilerType
from min_logger.generate_perfetto import PerfettoBuilder
from min_logger.built_in_types import get_struct_format

_logger = logging.getLogger("min_logger.parser")


SUBSTITUTE_PATTERN = re.compile(r"\$\{(.+?)\}")


def _substitute_vars(text, values):

    def replacer(match):
        key = match.group(1)
        return str(values.get(key, match.group(0)))

    return re.sub(SUBSTITUTE_PATTERN, replacer, text)


def _severity_string(severity):
    if severity <= SEVERITY_LEVELS["DEBUG"]:
        return "DEBUG"
    elif severity <= SEVERITY_LEVELS["INFO"]:
        return "INFO"
    elif severity <= SEVERITY_LEVELS["WARN"]:
        return "WARN"
    elif severity <= SEVERITY_LEVELS["ERROR"]:
        return "ERROR"
    else:
        return "CRITICAL"


FORMAT_CHARS = "bBhHiIlLqQfdspPc?x"
TYPE_RE = re.compile(r"([0-9]*)([^0-9].*)")


def _c_type_to_python_data(
    data: Optional[bytes],
    c_type: str,
    type_defs: dict[str, str | dict],
    seen: Optional[List[str]] = None,
) -> tuple[Any, int]:
    """Parse binary data according to a c_type definition from type_defs.

    Args:
        data: Binary data to parse
        c_type: Type name or struct format string
        type_defs: Type definitions dict

    Returns:
        Scalar value for simple types, dict for composite types
    """
    if seen is None:
        seen = []
    if c_type in seen:
        chain = " -> ".join(seen + [c_type])
        raise ValueError(f"Circular type reference detected: {chain}")

    m = TYPE_RE.fullmatch(c_type)
    if m is None:
        raise ValueError(f"Invalid c_type format: {c_type}")

    count_str = m.group(1)
    count = 1 if len(count_str) == 0 else int(count_str)

    single_type = m.group(2)

    if single_type in FORMAT_CHARS:
        format_str = c_type
    elif single_type not in type_defs:
        format_str = count_str + get_struct_format(single_type)
    else:
        type_def = type_defs[single_type]

        # Check if it's a composite type (dict in type_defs)
        if isinstance(type_def, dict):
            array_values = []
            offset = 0
            for _ in range(count):
                result = {}
                seen.append(single_type)
                for field_name, field_type in type_def.items():
                    # Recursively parse each field, passing along seen list
                    next_field_data = data[offset:] if data is not None else None
                    field_value, field_size = _c_type_to_python_data(
                        next_field_data, field_type, type_defs, seen
                    )
                    if field_value is not None:
                        result[field_name] = field_value
                    offset += field_size

                seen.pop()

                if count == 1:
                    return result, offset

                array_values.append(result)
            return array_values, offset

        return _c_type_to_python_data(data, count_str + type_def, type_defs, seen)

    # Parse as struct format string
    size = struct.calcsize(format_str)
    if data is None or single_type == "x":
        value = None
    else:
        values = struct.unpack("<" + format_str, data[:size])
        value = values[0] if len(values) == 1 else list(values)
        if isinstance(value, bytes):
            try:
                str_val = value.rstrip(b"\x00").decode("utf-8")
                if str_val.isprintable():
                    value = str_val
            except UnicodeDecodeError:
                pass

    return value, size


class MessageHandler:
    def __init__(
        self,
        meta,
        print_messages=True,
        perfetto_path: Optional[Path] = None,
        csv_dir: Optional[Path] = None,
    ) -> None:
        self.log_metrics: dict[int, MetricEntryData] = meta["entries"]
        self.type_defs: dict[str, str | dict] = meta["type_defs"]
        self.print_messages = print_messages
        self.perfetto_path = perfetto_path
        self.csv_dir = csv_dir
        if self.csv_dir is not None:
            self.csv_dir.mkdir(parents=True, exist_ok=True)

        # Map metric_key -> {file: fobj, writer: DictWriter, fieldnames: list}
        self._csv_files: dict[str, dict] = {}

        self.last_values: dict[str, str] = {}
        self.unknown_ids: set[int] = set()
        self.thread_names: dict[int, str] = {}
        self.perfetto_gen = PerfettoBuilder()

    def _sanitize_filename(self, name: str) -> str:
        # Keep only safe characters
        return re.sub(r"[^A-Za-z0-9_.-]", "_", name)

    def _flatten_value(self, v: Any, out: dict, prefix: str = "") -> None:
        if isinstance(v, dict):
            for k, val in v.items():
                new_prefix = f"{prefix}.{k}" if prefix else k
                self._flatten_value(val, out, new_prefix)
        elif isinstance(v, list):
            for i, val in enumerate(v):
                new_prefix = f"{prefix}_{i}" if prefix else f"item_{i}"
                self._flatten_value(val, out, new_prefix)
        else:
            out[prefix or "value"] = v

    def _write_metric_csv(
        self, metric_name: str, timestamp: float, value: Any, index: Optional[int] = None
    ) -> None:
        if metric_name is None:
            return
        if self.csv_dir is None:
            return

        # Prepare flattened row
        flattened: dict = {}
        self._flatten_value(value, flattened)

        # CSV file key by metric_name
        entry = self._csv_files.get(metric_name)

        if entry is None:
            # Create file and writer
            fname = self.csv_dir / f"{self._sanitize_filename(metric_name)}.csv"
            new_file = not fname.exists()
            fobj = open(fname, "a", newline="", encoding="utf-8")
            implicit_fields = ["timestamp"]
            if index is not None:
                implicit_fields.append("index")
            fieldnames = implicit_fields + list(flattened.keys())
            writer = csv.DictWriter(fobj, fieldnames=fieldnames)
            if new_file:
                writer.writeheader()
            self._csv_files[metric_name] = {
                "file": fobj,
                "writer": writer,
                "fieldnames": fieldnames,
            }
            entry = self._csv_files[metric_name]

        # Check for new keys
        current_fields = entry["fieldnames"]
        new_keys = [k for k in flattened.keys() if k not in current_fields]
        if new_keys:
            current_fields = current_fields + new_keys
            entry["fieldnames"] = current_fields
            entry["writer"] = csv.DictWriter(entry["file"], fieldnames=current_fields)
            entry["writer"].writeheader()

        # Build row with all fields
        row = {fn: "" for fn in entry["fieldnames"]}
        row["timestamp"] = f"{timestamp:.6f}"
        if index is not None:
            row["index"] = str(index)
        for k, v in flattened.items():
            row[k] = v

        entry["writer"].writerow(row)

    def process_msg(self, timestamp: float, metric_id: int, thread_id: int, value: bytes):

        if metric_id == THREAD_NAME_MSG_ID:
            thread_name = ""
            if isinstance(value, (bytes, bytearray)):
                thread_name = value.decode()
            else:
                thread_name = str(value)

            self.thread_names[thread_id] = thread_name

            if self.perfetto_path:
                self.perfetto_gen.set_thread_name(timestamp, thread_id, thread_name)

            return

        if metric_id not in self.log_metrics:
            if metric_id not in self.unknown_ids:
                _logger.warning("Metric with unknown ID: 0x%08X", metric_id)
                self.unknown_ids.add(metric_id)
            return

        metric = self.log_metrics[metric_id]

        if metric.name is not None and metric.value_type is not None:
            if metric.is_array:
                _, size = _c_type_to_python_data(None, metric.value_type, self.type_defs)
                if len(value) % size != 0:
                    raise ValueError(
                        f"Payload size {len(value)} is not a multiple of element size {size} for metric ID 0x{metric_id:08X}"
                    )
                else:
                    array_count = len(value) // size
                    full_type = f"{array_count}{metric.value_type}"
            else:
                full_type = metric.value_type
            new_value, size = _c_type_to_python_data(value, full_type, self.type_defs)
            self.last_values[metric.name] = new_value
            # Write CSV if requested and metric has a name
            if self.csv_dir is not None:
                if isinstance(new_value, list):
                    for i, elem in enumerate(new_value):
                        self._write_metric_csv(metric.name, timestamp, elem, i)
                else:
                    self._write_metric_csv(metric.name, timestamp, new_value)
            if size < len(value):
                raise ValueError(
                    f"Parsed size {size} is smaller than payload size {len(value)} for metric ID 0x{metric_id:08X}"
                )

        thread_name = (
            f"thread_id_{thread_id}"
            if thread_id not in self.thread_names
            else self.thread_names[thread_id]
        )

        if metric.profiler_type == ProfilerType.ENTER and metric.name is not None:
            if self.perfetto_path:
                self.perfetto_gen.enter_slice(
                    timestamp, metric.name, thread_id, metric.source_file, metric.source_line
                )
        elif metric.profiler_type == ProfilerType.EXIT and metric.name is not None:
            if self.perfetto_path:
                self.perfetto_gen.exit_slice(
                    timestamp, metric.name, thread_id, metric.source_file, metric.source_line
                )

        if metric.msg is not None:
            msg = _substitute_vars(metric.msg, self.last_values)
            severity_str = _severity_string(metric.level)
            print(
                f"{timestamp:.6f} {severity_str:5} {metric.source_file}:{metric.source_line} {thread_name}] {msg}"
            )
            if self.perfetto_path:
                self.perfetto_gen.add_log(
                    timestamp, msg, thread_id, severity_str, metric.source_file, metric.source_line
                )

    def finish(self):
        if self.perfetto_path:
            self.perfetto_gen.write_to_file(self.perfetto_path)

        # Close any open CSV files
        for entry in list(self._csv_files.values()):
            entry["file"].close()

        if len(self.unknown_ids) > 0:
            _logger.warning("Log contained unknown IDs: %s", str(self.unknown_ids))


# struct BinaryMsgHeader {
#     static constexpr uint16_t SYNC = 0xFAAF;
#     uint16_t sync = SYNC;
#     uint8_t payload_len = 0;
#     uint8_t thread_id = 0;
#     MinLoggerCRC msg_id = 0;
#     uint64_t timestamp = 0;
# };

SYNC_BYTES = b"\xaf\xfa"
# Skip sync
MSG_HEADER = struct.Struct("<BBIQ")
CHUNK_SIZE = 32
HEADER_SIZE = MSG_HEADER.size + len(SYNC_BYTES)


def read_binary(
    fd: BinaryIO, meta: dict[int, MetricEntryData], perfetto_path: Optional[Path] = None
):
    handler = MessageHandler(meta, perfetto_path=perfetto_path, csv_dir=Path("/tmp/min_logger_csv"))
    buffer = b""
    while True:
        chunk = fd.read(CHUNK_SIZE)
        if not chunk:
            break
        buffer += chunk
        while True:
            idx = buffer.find(SYNC_BYTES)
            if idx == -1:
                # Keep last byte in buffer in case sync is split across chunks
                buffer = (
                    buffer[-(len(SYNC_BYTES) - 1) :]
                    if len(buffer) >= len(SYNC_BYTES) - 1
                    else buffer
                )
                break
            buffer = buffer[idx:]

            if len(buffer) < HEADER_SIZE:
                # Not enough data for header, wait for next chunk
                break
            payload_len, thread_id, metric_id, timestamp = MSG_HEADER.unpack_from(
                buffer, len(SYNC_BYTES)
            )
            timestamp *= 1e-9
            if len(buffer) < HEADER_SIZE + payload_len:
                # Not enough data for payload, wait for next chunk
                break
            msg_end = HEADER_SIZE + payload_len
            payload = buffer[HEADER_SIZE:msg_end]
            handler.process_msg(timestamp, metric_id, thread_id, payload)
            buffer = buffer[msg_end:]

    handler.finish()


def _timescale_to_dt(time_scale, time_value) -> float:
    return time_value * (1e-9 * (1000**time_scale))


def read_micro_binary(
    fd: BinaryIO, meta: dict[int, MetricEntryData], perfetto_path: Optional[Path] = None
):
    handler = MessageHandler(meta, perfetto_path=perfetto_path)
    truncated_ids = {v & 0xFFFF: v for v in meta.keys()}
    truncated_ids[THREAD_NAME_MSG_ID & 0xFFFF] = THREAD_NAME_MSG_ID
    timestamp = 0
    """
    Searches a binary file byte by byte for words that match the truncated_ids.
    Then parses the remainder of the MicroMessage structure:
        struct MicroMessage {
            uint16_t truncated_id;
            uint8_t thread_id : 4;
            uint8_t time_scale : 2;
            uint16_t time_value : 10;
        };
    Yields dicts with the parsed fields.
    """

    BUFFER_SIZE = 4096
    buffer = b""
    while True:
        chunk = fd.read(BUFFER_SIZE)
        if not chunk:
            break
        buffer += chunk
        # Minimum message size is 4 bytes
        min_msg_size = 4
        i = 0
        while i <= len(buffer) - min_msg_size:
            truncated_id = int.from_bytes(buffer[i : i + 2], "little")
            if truncated_id not in truncated_ids:
                i += 1
                continue
            # Check if we have enough bytes for the rest of the message
            if i + 4 > len(buffer):
                break  # Wait for more data
            bitfield = int.from_bytes(buffer[i + 2 : i + 4], "little")

            thread_id = (bitfield >> 0) & 0xF
            time_scale = (bitfield >> 4) & 0x3
            time_value = (bitfield >> 6) & 0x3FF
            # print(hex(truncated_id), thread_id, time_scale, time_value)
            dt = _timescale_to_dt(time_scale, time_value)
            timestamp += dt

            # You may want to reconstruct a timestamp from time_scale/time_value here
            # For now, just pass 0 as timestamp and truncated_id as metric_id
            handler.process_msg(timestamp, truncated_ids[truncated_id], thread_id, b"")
            i += 4
        # Keep any leftover bytes for next chunk
        buffer = buffer[i:]
    handler.finish()
