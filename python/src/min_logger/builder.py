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


class MetricEntryData(NamedTuple):
    """Metric metadata"""

    id: str
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


def _parse_severity(level_str: str) -> Optional[int]:
    for k, v in SEVERITY_LEVELS.items():
        if k in level_str:
            return v

    try:
        return int(level_str)
    except ValueError:
        return None


_LOG_METRIC_RE = re.compile(r"MIN_LOGGER_LOG\( *([a-zA-Z0-9_]+) *,(.+), *([a-zA-Z0-9_]+) *\)")


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


def get_metric_entries(files: list[Path], root_paths: list[Path]) -> list[MetricEntryData]:
    """Parse metric macros from source files.

    Args:
        files: Source files to scan for MIN_LOGGER macros.
    """
    metrics = []
    for file in files:
        with open(file) as fd:
            for root in root_paths:
                try:
                    file = file.relative_to(root)
                except ValueError:
                    pass
            for i, line in enumerate(fd):
                m = _LOG_METRIC_RE.search(line)
                if m is not None:
                    line_num = i + 1
                    location_str = f"{file}:{line_num}"
                    severity = _parse_severity(m.group(1))
                    if severity is None:
                        raise ValueError(
                            f'Could not parse severiy level "{m.group(1)}" in {location_str}.'
                        )
                    msg = m.group(2).strip()
                    if msg[0] != '"' or msg[-1] != '"':
                        raise ValueError(
                            f'Log message "{msg}" not string literal in {location_str}.'
                        )
                    msg = msg[1:-1]
                    metric_id = m.group(3)
                    metrics.append(
                        MetricEntryData(
                            id=metric_id,
                            type=MetricType.LOG,
                            source_file=file,
                            source_line=line_num,
                            level=severity,
                            tags=[],
                            msg=msg,
                        )
                    )

    return metrics


def write_header(entries: list[MetricEntryData], out_path: Path):
    """Generate header file for entries"""
    with open(out_path, "w") as fd:
        for entry in entries:
            fd.write(
                f"""\
#if MIN_LOGGER_MIN_LEVEL >= {entry.level}

void min_logger_func_{entry.id}(){{
    min_logger_format_and_write(MIN_LOGGER_NO_TAGS,
                                "{entry.source_file}",
                                {entry.source_line},
                                "{entry.msg}",
                                {entry.level},
                                0);
}}

#else

void min_logger_func_0(){{}}

#endif

"""
            )
