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

_logger = logging.getLogger("min_logger.builder")


class MetricType(StrEnum):
    """Identifies type of metric"""

    @staticmethod
    def _generate_next_value_(name, start, count, last_values) -> str:
        """
        By default StrEnum converts names to lower case. Keep them uppercase.
        """
        return name

    LOG = auto()
    RECORD_STRING = auto()
    RECORD_U64 = auto()
    ENTER = auto()
    EXIT = auto()


class MetricEntryData(NamedTuple):
    """Metric metadata"""

    id: int
    type: MetricType
    source_file: Path
    source_line: int
    level: int
    tags: list[str]
    msg: Optional[str] = None
    name: Optional[str] = None


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


# MIN_LOGGER_LOG(MIN_LOGGER_INFO, "task{T_NAME}: {LOOP_COUNT}");
_LOG_METRIC_RE = re.compile(r"MIN_LOGGER_LOG\((.+)\)")
# MIN_LOGGER_LOG_ID(MIN_LOGGER_INFO, "hello world trunc explicit ID", 0xDEADBEEF);
_LOG_ID_METRIC_RE = re.compile(r"MIN_LOGGER_LOG_ID\((.+)\)")
# MIN_LOGGER_RECORD_STRING(MIN_LOGGER_INFO, "T_NAME", msg.c_str());
# MIN_LOGGER_RECORD_U64(MIN_LOGGER_INFO, "LOOP_COUNT", i);
_RECORD_METRIC_RE = re.compile(r"MIN_LOGGER_RECORD_([A-Z0-9]+)\((.+)\)")
# MIN_LOGGER_ENTER(MIN_LOGGER_DEBUG, "TASK_LOOP");
# MIN_LOGGER_EXIT(MIN_LOGGER_DEBUG, "TASK_LOOP");
_ENTER_METRIC_RE = re.compile(r"MIN_LOGGER_(ENTER|EXIT)\((.+)\)")


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


def get_metric_entries(files: list[Path], root_paths: list[Path]) -> dict[int, MetricEntryData]:
    """Parse metric macros from source files.

    Args:
        files: Source files to scan for MIN_LOGGER macros.
    """
    metrics: dict[int, MetricEntryData] = {}
    name_table: dict[str, list[MetricEntryData]] = defaultdict(list)

    for file in files:
        with open(file) as fd:
            for root in root_paths:
                try:
                    file = file.relative_to(root)
                except ValueError:
                    pass
            for i, line in enumerate(fd):
                line_num = i + 1
                location_str = f"{file}:{line_num}"

                metric_type: Optional[MetricType] = None
                parsed_args = 0
                expected_args = 0

                severity_raw = ""
                msg_raw = None
                name_raw = None
                msg_id_raw = None

                name = None
                msg = None

                m = _LOG_METRIC_RE.search(line)
                if m is not None:
                    metric_type = MetricType.LOG
                    args = _parse_args(m.group(1))
                    expected_args = 2
                    parsed_args = len(args)
                    if expected_args == parsed_args:
                        severity_raw = args[0]
                        msg_raw = args[1]

                m = _LOG_ID_METRIC_RE.search(line)
                if m is not None:
                    metric_type = MetricType.LOG
                    args = _parse_args(m.group(1))
                    expected_args = 3
                    parsed_args = len(args)
                    if expected_args == parsed_args:
                        severity_raw = args[0]
                        msg_raw = args[1]
                        msg_id_raw = args[2]

                m = _RECORD_METRIC_RE.search(line)
                if m is not None:
                    if m.group(1) == "STRING":
                        metric_type = MetricType.RECORD_STRING
                    elif m.group(1) == "U64":
                        metric_type = MetricType.RECORD_U64
                    else:
                        raise ValueError(
                            f'Unsupported record type "{m.group(1)}" in {location_str}.'
                        )
                    args = _parse_args(m.group(2))
                    expected_args = 3
                    parsed_args = len(args)
                    if expected_args == parsed_args:
                        severity_raw = args[0]
                        name_raw = args[1]

                m = _ENTER_METRIC_RE.search(line)
                if m is not None:
                    if m.group(1) == "ENTER":
                        metric_type = MetricType.ENTER
                    else:
                        metric_type = MetricType.EXIT
                    args = _parse_args(m.group(2))
                    expected_args = 2
                    parsed_args = len(args)
                    if expected_args == parsed_args:
                        severity_raw = args[0]
                        name_raw = args[1]

                if metric_type is not None:
                    error_msg = f'Could not parse "{line.strip()}" at {location_str}.'
                    if expected_args != parsed_args:
                        raise ValueError(
                            f"{error_msg} Expected {expected_args} args, parsed {parsed_args}."
                        )

                    severity = _parse_severity(severity_raw)
                    if severity is None:
                        raise ValueError(
                            f'{error_msg} Could not parse severiy level "{severity_raw}".'
                        )

                    if msg_raw is not None:
                        msg = _get_string_literal(msg_raw)
                        if msg is None:
                            raise ValueError(
                                f'{error_msg} Log message "{msg_raw}" not string literal.'
                            )

                    if name_raw is not None:
                        name = _get_string_literal(name_raw)
                        if name is None:
                            raise ValueError(
                                f'{error_msg} Metric name "{name_raw}" not string literal.'
                            )

                        if name in name_table:
                            others = name_table[name]
                            if len(others) != 1 or {metric_type, others[0].type} != {
                                MetricType.ENTER,
                                MetricType.EXIT,
                            }:
                                other_location_str = (
                                    f"{others[0].source_file}:{others[0].source_line}"
                                )
                                _logger.warning(
                                    'Duplicate metric name "%s" in %s and %s.',
                                    name,
                                    other_location_str,
                                    location_str,
                                )

                    metric_id = 0
                    if msg_id_raw is not None:
                        try:
                            metric_id = int(msg_id_raw, 0)
                        except ValueError:
                            raise ValueError(
                                f'{error_msg} Could not parse ID "{msg_id_raw}" as integer.'
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
                        type=metric_type,
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
