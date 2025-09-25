from dataclasses import dataclass, field
import logging
from pathlib import Path
import re
import struct
from typing import BinaryIO, Optional, TextIO

from min_logger.builder import MetricEntryData, MetricType, THREAD_NAME_MSG_ID, SEVERITY_LEVELS
from min_logger.generate_perfetto import PerfettoBuilder


_logger = logging.getLogger("min_logger.parser")


def _substitute_vars(text, values):
    pattern = r"\$\{(.+?)\}"

    def replacer(match):
        key = match.group(1)
        return str(values.get(key, match.group(0)))

    return re.sub(pattern, replacer, text)


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


class MessageHandler:
    def __init__(
        self,
        meta: dict[int, MetricEntryData],
        print_messages=True,
        perfetto_path: Optional[Path] = None,
    ) -> None:
        self.meta = meta
        self.print_messages = print_messages
        self.perfetto_path = perfetto_path

        self.last_values: dict[str, str] = {}
        self.unknown_ids: set[int] = set()
        self.thread_names: dict[int, str] = {}
        self.perfetto_gen = PerfettoBuilder()

    def process_msg(self, timestamp: float, metric_id: int, thread_id: int, value: str | bytes):

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

        if metric_id not in self.meta:
            if metric_id not in self.unknown_ids:
                _logger.warning("Metric with unknown ID: 0x%08X", metric_id)
                self.unknown_ids.add(metric_id)
            return

        metric = self.meta[metric_id]

        if (
            metric.type in (MetricType.RECORD_STRING, MetricType.RECORD_U64)
            and metric.name is not None
        ):
            if isinstance(value, str):
                self.last_values[metric.name] = value
            elif isinstance(value, (bytes, bytearray)):
                if metric.type == MetricType.RECORD_U64:
                    self.last_values[metric.name] = str(struct.unpack("<Q", value)[0])
                elif metric.type == MetricType.RECORD_STRING:
                    self.last_values[metric.name] = value.decode()
                return
            return

        thread_name = (
            f"thread_id_{thread_id}"
            if thread_id not in self.thread_names
            else self.thread_names[thread_id]
        )

        if metric.type == MetricType.LOG and metric.msg is not None:
            msg = _substitute_vars(metric.msg, self.last_values)
            severity_str = _severity_string(metric.level)
            print(
                f"{timestamp:.6f} {severity_str:5} {metric.source_file}:{metric.source_line} {thread_name}] {msg}"
            )
            if self.perfetto_path:
                self.perfetto_gen.add_log(
                    timestamp, msg, thread_id, severity_str, metric.source_file, metric.source_line
                )
        elif metric.type == MetricType.ENTER and metric.name is not None:
            if self.perfetto_path:
                self.perfetto_gen.enter_slice(
                    timestamp, metric.name, thread_id, metric.source_file, metric.source_line
                )
        elif metric.type == MetricType.EXIT and metric.name is not None:
            if self.perfetto_path:
                self.perfetto_gen.exit_slice(
                    timestamp, metric.name, thread_id, metric.source_file, metric.source_line
                )

    def finish(self):
        if self.perfetto_path:
            self.perfetto_gen.write_to_file(self.perfetto_path)

        if len(self.unknown_ids) > 0:
            _logger.warning("Log contained unknown IDs: %s", str(self.unknown_ids))


def read_text(fd: TextIO, meta: dict[int, MetricEntryData], perfetto_path: Optional[Path] = None):
    handler = MessageHandler(meta, perfetto_path=perfetto_path)
    for line in fd:
        if line.startswith("$"):
            values = line.strip().split(",")
            if len(values) < 3:
                continue
            try:
                timestamp = float(values[0][1:])
                metric_id = int(values[1], 16)
                thread_id = int(values[2], 16)
                value = ",".join(values[3:])
                handler.process_msg(timestamp, metric_id, thread_id, value)
                continue
            except ValueError:
                _logger.warning("Error parsing line: ", exc_info=True)
        print(line, end="")
    handler.finish()


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
    handler = MessageHandler(meta, perfetto_path=perfetto_path)
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
            if len(buffer) < HEADER_SIZE + payload_len:
                # Not enough data for payload, wait for next chunk
                break
            msg_end = HEADER_SIZE + payload_len
            payload = buffer[HEADER_SIZE:msg_end]
            handler.process_msg(timestamp, metric_id, thread_id, payload)
            buffer = buffer[msg_end:]

    handler.finish()
