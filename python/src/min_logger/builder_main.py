#!/usr/bin/env python3
"""
CLI interface for building min-logger metadata.
"""

from pathlib import Path
from jsonargparse import auto_cli
from jsonargparse.typing import Path_dw, path_type

GEN_HEADER_FILENAME = "_min_logger_gen.h"

Path_dr = path_type("dw", docstring="path to a directory that exists and is writeable")


def command(
    src_paths: list[Path_dr] | Path_dr,  # pyright: ignore[reportInvalidTypeForm]
    out_path: Path_dw,  # pyright: ignore[reportInvalidTypeForm]
    recursive: bool = True,
    extensions: list[str] = ["h", "hh", "hpp", "c", "cpp", "cc", "cxx"],
):  # pylint: disable=dangerous-default-value
    """Prints the prize won by a person.

    Args:
        src_paths: Name of winner.
        out_path: Amount won.
    """
    with open(Path(out_path) / GEN_HEADER_FILENAME, "w", encoding="utf-8") as fd:
        fd.write(
            """\

#if MIN_LOGGER_MIN_LEVEL >= 20

void min_logger_func_0(){
    min_logger_write("hello world");
}

#endif

"""
        )


def main():
    """
    Entry point for the application. Invokes the auto_cli function with the specified command.
    """
    auto_cli(command)


if __name__ == "__main__":
    main()
