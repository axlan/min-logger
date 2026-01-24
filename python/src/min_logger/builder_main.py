#!/usr/bin/env python3
"""
CLI interface for building min-logger metadata and header files.
"""

import json
import logging
from pathlib import Path
from typing import Optional

from jsonargparse import auto_cli
from jsonargparse.typing import Path_fc, Path_fr, path_type

from min_logger.builder import get_file_matches, get_metric_entries, json_dump_helper
from min_logger.parser import _c_type_to_python_data, SUBSTITUTE_PATTERN

Path_dr = path_type("dw", docstring="path to a directory that exists and is writeable")

_logger = logging.getLogger("min_logger.builder_main")


def command(
    src_paths: list[Path_dr] | Path_dr,  # pyright: ignore[reportInvalidTypeForm]
    # Set to None to indicate this is a required named arguement
    json_output: Path_fc = None,  # pyright: ignore[reportInvalidTypeForm]
    root_paths: Optional[list[Path_dr] | Path_dr] = None,  # pyright: ignore[reportInvalidTypeForm]
    extensions: list[str] = ["h", "hh", "hpp", "c", "cpp", "cc", "cxx"],
    recursive: bool = True,
    type_defs: Path_fr = None,  # pyright: ignore[reportInvalidTypeForm]
):  # pylint: disable=dangerous-default-value
    """Generate min-logger metadata data files from source files with MIN_LOGGER macros.

    Args:
        src_paths: Directories to scan for source files with MIN_LOGGER macros.
        json_output: File to loghing context data to.
        root_paths: Root paths to strip from source file paths in the output.
        extensions: The extensions for source files with MIN_LOGGER macros.
        recursive: Search src_paths recursively.
        type_defs: A JSON map of C types to their python serialization
    """

    if json_output is None:
        raise ValueError("--json_output is required")
    if not isinstance(src_paths, list):
        src_paths = [src_paths]
    src_paths = [Path(s) for s in src_paths]
    if root_paths is not None:
        if not isinstance(root_paths, list):
            root_paths = [root_paths]
        root_paths = [Path(s) for s in root_paths]
    else:
        root_paths = src_paths
    type_defs_data = {}
    if type_defs:
        with open(type_defs, "r") as fd:
            type_defs_data = json.load(fd)

    candidate_files = get_file_matches(src_paths, extensions, recursive)
    entries = get_metric_entries(candidate_files, root_paths)

    value_names = set(
        e.name for e in entries.values() if e.name is not None and e.value_type is not None
    )
    for entry in entries.values():
        if entry.value_type is not None:
            # TODO: Check sizes here?
            _c_type_to_python_data(None, entry.value_type, type_defs_data)
        if entry.msg is not None:
            m = SUBSTITUTE_PATTERN.search(entry.msg)
            if m:
                if m.group(1) not in value_names:
                    _logger.warning(
                        "In message format '%s', the variable '%s' is not defined as a logged value.",
                        entry.msg,
                        m.group(1),
                    )

    description = {"entries": [e._asdict() for e in entries.values()], "type_defs": type_defs_data}

    with open(json_output, "w") as fd:
        json.dump(description, fd, default=json_dump_helper)


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
