#!/usr/bin/env python3
"""
CLI interface for validating type_def JSON with C implementation.
"""

import json
import logging
import subprocess
import sys

from jsonargparse import auto_cli
from jsonargparse.typing import Path_fr

from min_logger.parser import _c_type_to_python_data

_logger = logging.getLogger("min_logger.builder_main")


def command(
    bin_file: Path_fr,  # pyright: ignore[reportInvalidTypeForm]
    type_defs: Path_fr = None,  # pyright: ignore[reportInvalidTypeForm]
    gdb_bin: str = "gdb",
):  # pylint: disable=dangerous-default-value
    """Compare type sizes from type definitions against sizes obtained from binary via gdb.

    Args:
        bin_file: The binary with log macros to check types against. Must be compiled with debug symbols.
        type_defs: The map of C types to their python serialization
        gdb_bin: Path to the gdb binary to debug the binary
    """

    if type_defs is None:
        raise ValueError("--type_defs is required")
    with open(type_defs, "r") as fd:
        type_defs_data: dict[str, str | dict] = json.load(fd)

    for value_type in type_defs_data:
        _, size = _c_type_to_python_data(None, value_type, type_defs_data)

        # Run gdb to get sizeof(int) from the firmware
        output = ""
        try:
            result = subprocess.run(
                [
                    gdb_bin,
                    str(bin_file),
                    "--batch",
                    f"-ex=output sizeof({value_type})",
                ],
                capture_output=True,
                text=True,
                check=True,
            )

            # Parse the output as an int
            output = result.stdout.strip()
            gdb_int_size = int(output)

            # Compare with expected size
            _logger.info(f"Type {value_type}: expected size={size}, gdb sizeof(int)={gdb_int_size}")

            if size != gdb_int_size:
                _logger.warning(
                    f"Size mismatch for {value_type}: expected {size}, but gdb reports {gdb_int_size}"
                )
                sys.exit(1)
        except subprocess.CalledProcessError as e:
            _logger.error(f"gdb-multiarch failed: {e.stderr}")
        except ValueError as e:
            _logger.error(f"Could not parse gdb output as int: {output}, error: {e}")


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
