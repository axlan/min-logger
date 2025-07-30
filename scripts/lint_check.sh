#!/usr/bin/env bash
set -e

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PYTHON_DIR="${SCRIPT_DIR}/../python"

uv --project "${PYTHON_DIR}" run black "${PYTHON_DIR}/src" "${PYTHON_DIR}/tests"

uv --project "${PYTHON_DIR}" run pylint --rcfile="${PYTHON_DIR}/.pylintrc" --recursive=y "${PYTHON_DIR}/src" "${PYTHON_DIR}/tests"
