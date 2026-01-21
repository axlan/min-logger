#!/usr/bin/env python3
"""
Build min-logger metadata from source files with MIN_LOGGER macros.
"""

from binascii import crc32
from collections import defaultdict
from enum import Enum, StrEnum, auto
import logging
from pathlib import Path
import re
from typing import NamedTuple, Optional

from .built_in_types import get_struct_format

_logger = logging.getLogger("min_logger.builder")


class ProfilerType(StrEnum):
    """Identifies type of metric"""

    @staticmethod
    def _generate_next_value_(name, start, count, last_values) -> str:
        """
        By default StrEnum converts names to lower case. Keep them uppercase.
        """
        return name

    ENTER = auto()
    EXIT = auto()


class MetricEntryData(NamedTuple):
    """Metric metadata"""

    id: int
    source_file: Path
    source_line: int
    level: int
    tags: list[str]
    value_type: Optional[str] = None
    is_array: bool = False
    msg: Optional[str] = None
    name: Optional[str] = None
    profiler_type: Optional[ProfilerType] = None


def json_dump_helper(obj):
    """Can be passed to JSON serializer to handle unsuported types."""

    if isinstance(obj, Enum):
        return obj.name
    else:
        return str(obj)


SEVERITY_LEVELS = {"DEBUG": 10, "INFO": 20, "WARN": 30, "ERROR": 40, "CRITICAL": 50}

THREAD_NAME_MSG_ID = 0xFFFFFF00

RESERVED_IDS = {THREAD_NAME_MSG_ID}


def _parse_severity(level_str: str) -> Optional[int]:
    for k, v in SEVERITY_LEVELS.items():
        if k in level_str:
            return v

    try:
        return int(level_str)
    except ValueError:
        return None


def _get_string_literal(value: str) -> Optional[str]:
    if len(value) < 2 or value[0] != '"' or value[-1] != '"':
        return None
    else:
        return value[1:-1]


def _parse_args(raw_contents: str) -> list[str]:
    args = [""]
    quote_open = False
    last_char = ""
    for c in raw_contents:
        if not quote_open and c == ",":
            args.append("")
        else:
            args[-1] += c
            if last_char != "\\" and c == '"':
                quote_open = not quote_open
        last_char = c
    return [a.strip() for a in args]


_METRIC_RE = re.compile(r"MIN_LOGGER_[A-Z_]+\((.+?)\);", flags=re.DOTALL)

# MIN_LOGGER_LOG(MIN_LOGGER_INFO, "task{T_NAME}: {LOOP_COUNT}");
# MIN_LOGGER_LOG_ID(0xDEADBEEF, MIN_LOS(_ID)?\((.GGER_INFO, "hello world trunc explicit ID");
_LOG_METRIC_RE = re.compile(r"MIN_LOGGER_LOG(_ID)?\((.+?)\);", flags=re.DOTALL)
# define MIN_LOGGER_RECORD_VALUE_ID(id, level, name, type, value)
# define MIN_LOGGER_RECORD_AND_LOG_VALUE_ID(id, level, name, type, value, msg)
# define MIN_LOGGER_RECORD_VALUE_ARRAY_ID(id, level, name, type, values, num_values)
# define MIN_LOGGER_RECORD_AND_LOG_VALUE_ARRAY_ID(id, level, name, type, values, num_values, msg)
_RECORD_VALUE_METRIC_RE = re.compile(
    r"MIN_LOGGER_RECORD(_AND_LOG)?_VALUE(_ARRAY)?(_ID)?\((.+?)\);", flags=re.DOTALL
)
# MIN_LOGGER_ENTER(MIN_LOGGER_DEBUG, "TASK_LOOP");
# MIN_LOGGER_EXIT(MIN_LOGGER_DEBUG, "TASK_LOOP");
_ENTER_METRIC_RE = re.compile(r"MIN_LOGGER_(ENTER|EXIT)(_ID)?\((.+?)\);", flags=re.DOTALL)


def get_file_matches(src_paths: list[Path], extensions: list[str], recursive: bool) -> list[Path]:
    """Get list of qualifying files.

    Args:
        src_paths: Directories to scan for source files with MIN_LOGGER macros.
        extensions: The extensions for source files with MIN_LOGGER macros.
        recursive: Search src_paths recursively.
    """
    matches = []
    for src_path in src_paths:
        if not src_path.exists():
            continue
        if src_path.is_file():
            if src_path.suffix in extensions:
                matches.append(src_path)
        else:
            pattern = "**/*" if recursive else "*"
            for ext in extensions:
                for file in src_path.glob(f"{pattern}{ext}"):
                    if file.is_file():
                        matches.append(file)
    return matches


def get_metric_entries(
    files: list[Path], root_paths: list[Path], type_defs
) -> dict[int, MetricEntryData]:
    """Parse metric macros from source files.

    Args:
        files: Source files to scan for MIN_LOGGER macros.
    """
    metrics: dict[int, MetricEntryData] = {}
    name_table: dict[str, list[MetricEntryData]] = defaultdict(list)

    for file in files:
        with open(file) as fd:
            content = fd.read()
            for root in root_paths:
                try:
                    file = file.relative_to(root)
                except ValueError:
                    pass

            current_line = 1
            last_pos = 0
            for m in _METRIC_RE.finditer(content):
                current_line += content.count("\n", last_pos, m.start())
                last_pos = m.start()
                line_num = current_line
                location_str = f"{file}:{line_num}"
                lines = m.group(0)

                profiler_type: Optional[ProfilerType] = None
                parsed_args: list[str] = []
                has_id = False
                is_array = False

                raw_strings = {
                    "severity": "",
                    "msg": None,
                    "name": None,
                    "msg_id": None,
                    "value_type": None,
                }

                param_positions: dict[str, int] = {}

                name = None
                msg = None

                m = _LOG_METRIC_RE.search(lines)
                if m is not None:
                    has_id = bool(m.group(1))
                    parsed_args = _parse_args(m.group(2))
                    param_positions["severity"] = 0
                    param_positions["msg"] = 1

                m = _RECORD_VALUE_METRIC_RE.search(lines)
                if m is not None:
                    has_msg = bool(m.group(1))
                    is_array = bool(m.group(2))
                    has_id = bool(m.group(3))
                    parsed_args = _parse_args(m.group(4))
                    param_positions["severity"] = 0
                    param_positions["name"] = 1
                    param_positions["value_type"] = 2
                    param_positions["value"] = 3
                    if is_array:
                        param_positions["num_values"] = len(param_positions)
                    if has_msg:
                        param_positions["msg"] = len(param_positions)

                m = _ENTER_METRIC_RE.search(lines)
                if m is not None:
                    if m.group(1) == "ENTER":
                        profiler_type = ProfilerType.ENTER
                    else:
                        profiler_type = ProfilerType.EXIT
                    has_id = bool(m.group(2))
                    parsed_args = _parse_args(m.group(3))
                    param_positions["severity"] = 0
                    param_positions["name"] = 1

                if len(param_positions) > 0:
                    error_msg = f'Could not parse "{lines.strip()}" at {location_str}.'

                    if has_id:
                        for key in param_positions:
                            param_positions[key] += 1
                        param_positions["msg_id"] = 0

                    if len(param_positions) != len(parsed_args):
                        raise ValueError(
                            f"{error_msg} Expected {len(param_positions)} args, parsed {len(parsed_args)}."
                        )

                    for param, idx in param_positions.items():
                        raw_strings[param] = parsed_args[idx]

                    severity = _parse_severity(raw_strings["severity"])
                    if severity is None:
                        raise ValueError(
                            f'{error_msg} Could not parse severiy level "{raw_strings["severity"]}".'
                        )

                    if raw_strings["msg"] is not None:
                        msg = _get_string_literal(raw_strings["msg"])
                        if msg is None:
                            raise ValueError(
                                f'{error_msg} Log message "{raw_strings["msg"]}" not string literal.'
                            )

                    if raw_strings["name"] is not None:
                        name = _get_string_literal(raw_strings["name"])
                        if name is None:
                            raise ValueError(
                                f'{error_msg} Metric name "{raw_strings["name"]}" not string literal.'
                            )

                        if (
                            profiler_type not in {ProfilerType.ENTER, ProfilerType.EXIT}
                            and name in name_table
                        ):
                            others = name_table[name]
                            other_location_str = f"{others[0].source_file}:{others[0].source_line}"
                            _logger.warning(
                                'Duplicate metric name "%s" in %s and %s.',
                                name,
                                other_location_str,
                                location_str,
                            )

                    if raw_strings["value_type"] is not None:
                        if raw_strings["value_type"] not in type_defs:
                            get_struct_format(raw_strings["value_type"])

                    metric_id = 0
                    if raw_strings["msg_id"] is not None:
                        try:
                            metric_id = int(raw_strings["msg_id"], 0)
                        except ValueError:
                            raise ValueError(
                                f'{error_msg} Could not parse ID "{raw_strings["msg_id"]}" as integer.'
                            )
                    else:
                        metric_id = crc32(location_str.encode())

                    if metric_id in RESERVED_IDS:
                        raise ValueError(f'Can\'t use reserved ID "{metric_id}" in {location_str}.')

                    if metric_id in metrics:
                        other = metrics[metric_id]
                        other_location_str = f"{other.source_file}:{other.source_line}"
                        raise ValueError(
                            f'Duplicate ID "{metric_id}" in {other_location_str} and {location_str}.'
                        )

                    metrics[metric_id] = MetricEntryData(
                        id=metric_id,
                        value_type=raw_strings["value_type"],
                        is_array=is_array,
                        profiler_type=profiler_type,
                        source_file=file,
                        source_line=line_num,
                        level=severity,
                        tags=[],
                        msg=msg,
                        name=name,
                    )

                    if name is not None:
                        name_table[name].append(metrics[metric_id])

    return metrics
