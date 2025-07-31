#!/usr/bin/env python3
"""
CLI interface for building min-logger metadata and header files.
"""

import logging
from pathlib import Path
from typing import Optional

from jsonargparse import auto_cli
from jsonargparse.typing import Path_fc, path_type

from min_logger.builder import get_file_matches, get_metric_entries, write_header

Path_dr = path_type("dw", docstring="path to a directory that exists and is writeable")

_logger = logging.getLogger("min_logger.builder_main")


def command(
    src_paths: list[Path_dr] | Path_dr,  # pyright: ignore[reportInvalidTypeForm]
    extensions: list[str] = ["h", "hh", "hpp", "c", "cpp", "cc", "cxx"],
    recursive: bool = True,
    header_output: Optional[Path_fc] = None,  # pyright: ignore[reportInvalidTypeForm]
    json_output: Optional[Path_fc] = None,  # pyright: ignore[reportInvalidTypeForm]
):  # pylint: disable=dangerous-default-value
    """Generate MIN_LOGGER header and/or metadata data files from source files with MIN_LOGGER macros.

    Args:
        src_paths: Directories to scan for source files with MIN_LOGGER macros.
        extensions: The extensions for source files with MIN_LOGGER macros.
        recursive: Search src_paths recursively.
        header_output: File to write _min_logger_gen.h helper functions to (required for cmake builds).
        json_output: File to loghing context data to.
    """

    if not isinstance(src_paths, list):
        src_paths = [src_paths]
    src_paths = [Path(s) for s in src_paths]

    candidate_files = get_file_matches(src_paths, extensions, recursive)
    entries = get_metric_entries(candidate_files)

    if header_output:
        write_header(entries, header_output)


def main():
    """
    Entry point for the application. Invokes the auto_cli function with the specified command.
    """
    logging.basicConfig(
        level=logging.INFO, format="%(levelname)s - %(name)s:%(lineno)d - %(message)s"
    )
    auto_cli(command)


if __name__ == "__main__":
    main()
