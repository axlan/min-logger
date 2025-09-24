#!/usr/bin/env python3
"""
CLI interface for parsing min-logger logs.
"""

from enum import Enum, auto
import json
import logging
from pathlib import Path
from typing import Optional
import sys

from jsonargparse import auto_cli
from jsonargparse.typing import Path_fr, path_type

from min_logger.builder import (
    get_file_matches,
    get_metric_entries,
    json_dump_helper,
    MetricEntryData,
)
from min_logger.parser import read_text

Path_dr = path_type("dw", docstring="path to a directory that exists and is writeable")

_logger = logging.getLogger("min_logger.parser_main")


class LogFormat(Enum):
    TEXT = auto()
    BINARY = auto()


def command(
    meta_data: Path_fr,  # pyright: ignore[reportInvalidTypeForm]
    log_format: LogFormat = None,  # pyright: ignore[reportArgumentType]
    log_file: Optional[Path_fr] = None,  # pyright: ignore[reportInvalidTypeForm]
):  # pylint: disable=dangerous-default-value
    """Generate MIN_LOGGER header and/or metadata data files from source files with MIN_LOGGER macros.

    Args:
        meta_data: Meta data json file with log definitions.

    """

    if log_format is None:
        raise ValueError("--log_format is required")

    with open(meta_data, "r") as fd:
        meta_data = json.load(fd)
        meta_data = {e["id"]: MetricEntryData(**e) for e in meta_data}

    is_binary = log_format in {LogFormat.BINARY}

    if log_file is None:
        log_fd = sys.stdin.buffer if is_binary else sys.stdin
    else:
        mode = "rb" if is_binary else "r"
        log_fd = open(log_file, mode)

    if log_format == LogFormat.TEXT:
        read_text(log_fd, meta_data)  # type: ignore


def main():
    """
    Entry point for the application. Invokes the auto_cli function with the specified command.
    """
    logging.basicConfig(
        level=logging.INFO,
        format="%(levelname)s - %(name)s:%(lineno)d - %(message)s",
        stream=sys.stderr,
    )
    auto_cli(command)


if __name__ == "__main__":
    main()
