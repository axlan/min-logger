#!/usr/bin/env python3
"""
Build min-logger metadata and header files from source files with MIN_LOGGER macros.
"""

from enum import Enum, StrEnum, auto
import logging
from pathlib import Path
import re
from typing import NamedTuple, Optional

_logger = logging.getLogger("min_logger.builder")


class MetricType(StrEnum):
    """Identifies type of metric"""

    LOG = auto()
    RECORD_STRING = auto()
    RECORD_U64 = auto()
    ENTER = auto()
    EXIT = auto()


class MetricEntryData(NamedTuple):
    """Metric metadata"""

    bin_id: int
    type: MetricType
    source_file: Path
    source_line: int
    level: int
    tags: list[str]
    msg: str


def json_dump_helper(obj):
    """Can be passed to JSON serializer to handle unsuported types."""

    if isinstance(obj, Enum):
        return obj.name
    else:
        return str(obj)


SEVERITY_LEVELS = {"DEBUG": 10, "INFO": 20, "WARN": 30, "ERROR": 40, "CRITICAL": 50}

RESERVED_IDS = {"TID"}


def _parse_severity(level_str: str) -> Optional[int]:
    for k, v in SEVERITY_LEVELS.items():
        if k in level_str:
            return v

    try:
        return int(level_str)
    except ValueError:
        return None


_LOG_METRIC_RE = re.compile(r"MIN_LOGGER_LOG\( *([a-zA-Z0-9_]+) *,(.+), *([a-zA-Z0-9_]+) *\)")

_RECORD_METRIC_RE = re.compile(
    r"MIN_LOGGER_RECORD_([A-Z0-9]+)\( *([a-zA-Z0-9_]+) *,.+, *([a-zA-Z0-9_]+) *\)"
)

_ENTER_METRIC_RE = re.compile(r"MIN_LOGGER_ENTER\( *([a-zA-Z0-9_]+) *, *([a-zA-Z0-9_]+) *\)")

_EXIT_METRIC_RE = re.compile(r"MIN_LOGGER_EXIT\( *([a-zA-Z0-9_]+) *, *([a-zA-Z0-9_]+) *\)")


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


def get_metric_entries(files: list[Path], root_paths: list[Path]) -> dict[str, MetricEntryData]:
    """Parse metric macros from source files.

    Args:
        files: Source files to scan for MIN_LOGGER macros.
    """
    metrics: dict[str, MetricEntryData] = {}
    bin_id_index = 0
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
                severity_str = ""
                msg = ""
                metric_id = ""

                m = _LOG_METRIC_RE.search(line)
                if m is not None:
                    metric_type = MetricType.LOG
                    severity_str = m.group(1)
                    msg = m.group(2)
                    metric_id = m.group(3)

                if metric_type is None:
                    m = _RECORD_METRIC_RE.search(line)
                    if m is not None:
                        if m.group(1) == "STRING":
                            metric_type = MetricType.RECORD_STRING
                        elif m.group(1) == "U64":
                            metric_type = MetricType.RECORD_U64
                        else:
                            raise ValueError(
                                f'No min logging type "{m.group(1)}" in {location_str}.'
                            )
                        severity_str = m.group(2)
                        metric_id = m.group(3)

                if metric_type is None:
                    m = _ENTER_METRIC_RE.search(line)
                    if m is not None:
                        metric_type = MetricType.ENTER
                        severity_str = m.group(1)
                        metric_id = m.group(2)

                if metric_type is None:
                    m = _EXIT_METRIC_RE.search(line)
                    if m is not None:
                        metric_type = MetricType.EXIT
                        severity_str = m.group(1)
                        metric_id = m.group(2)

                if metric_type is not None:
                    severity = _parse_severity(severity_str)
                    if severity is None:
                        raise ValueError(
                            f'Could not parse severiy level "{severity_str}" in {location_str}.'
                        )

                    if len(msg) != 0:
                        msg = msg.strip()
                        if len(msg) == 0 or msg[0] != '"' or msg[-1] != '"':
                            raise ValueError(
                                f'Log message "{msg}" not string literal in {location_str}.'
                            )
                        msg = msg[1:-1]

                    if metric_id in RESERVED_IDS:
                        raise ValueError(f'Can\'t use reserved ID "{metric_id}" in {location_str}.')

                    if metric_id in metrics:
                        other = metrics[metric_id]
                        other_location_str = f"{other.source_file}:{other.source_line}"
                        raise ValueError(
                            f'Duplicate ID "{msg}" in {other_location_str} and {location_str}.'
                        )

                    metrics[metric_id] = MetricEntryData(
                        bin_id=bin_id_index,
                        type=metric_type,
                        source_file=file,
                        source_line=line_num,
                        level=severity,
                        tags=[],
                        msg=msg,
                    )
                    bin_id_index += 1
                    continue

    return metrics


def write_header(entries: dict[str, MetricEntryData], out_path: Path):
    """Generate header file for entries"""
    with open(out_path, "w") as fd:
        fd.write(
            """\
#pragma once
#if MIN_LOGGER_ENABLED
#include <string.h>

extern const char** MIN_LOGGER_NO_TAGS;


"""
        )

        for metric_id, entry in entries.items():
            if entry.type in (MetricType.ENTER, MetricType.EXIT, MetricType.LOG):
                type_string = entry.type.name.lower()
                fd.write(
                    f"""\
static inline void min_logger_{type_string}_func_{metric_id}(){{
#if !MIN_LOGGER_DISABLE_VERBOSE_LOGGING
    if (*min_logging_is_verbose()){{
        min_logger_format_and_write_log(MIN_LOGGER_NO_TAGS,
                                    "{entry.source_file}",
                                    {entry.source_line},
                                    "{entry.msg}",
                                    {entry.level});
    }}
    else {{
#endif
        min_logger_write_msg_from_id("{metric_id}", "", 0);
#if !MIN_LOGGER_DISABLE_VERBOSE_LOGGING
    }}
#endif
}}

"""
                )
            elif entry.type == MetricType.RECORD_STRING:
                fd.write(
                    f"""\
void min_logger_record_string_func_{metric_id}(const char* value){{
    min_logger_write_msg_from_id("{metric_id}", value, strlen(value));
}}

"""
                )
            elif entry.type == MetricType.RECORD_U64:
                fd.write(
                    f"""\
void min_logger_record_u64_func_{metric_id}(uint64_t value){{
    if (*min_logging_is_binary()) {{
        min_logger_write_msg_from_id("{metric_id}", &value, sizeof(value));
    }}
    else {{
        const size_t MAX_LEN = 33;
        char buffer [MAX_LEN];
        snprintf(buffer, MAX_LEN, "%lu", value);
        min_logger_write_msg_from_id("{metric_id}", buffer, strlen(buffer));
    }}
}}

"""
                )

        fd.write(
            """\
#endif // MIN_LOGGER_ENABLED

"""
        )
