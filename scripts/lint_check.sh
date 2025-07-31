#!/usr/bin/env bash
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
REPO_DIR="${SCRIPT_DIR}/.."
PYTHON_DIR="${REPO_DIR}/python"
BUILD_DIR="${REPO_DIR}/build"

echo "************* black *************"
uv --project "${PYTHON_DIR}" run black "${PYTHON_DIR}/src" "${PYTHON_DIR}/tests"
RET=$?

echo "************* pylint *************"
uv --project "${PYTHON_DIR}" run pylint --rcfile="${PYTHON_DIR}/.pylintrc" --recursive=y "${PYTHON_DIR}/src" "${PYTHON_DIR}/tests"
if [ $? -ne 0 ]; then
    RET=1
fi

echo "************* pyright *************"
uv --project "${PYTHON_DIR}" run pyright
if [ $? -ne 0 ]; then
    RET=1
fi

if ! command -v clang-tidy >/dev/null 2>&1
then
    echo "clang-tidy not found."
else
    echo "************* clang-tidy *************"
    cmake -G Ninja -S $REPO_DIR -B $BUILD_DIR -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    # need _min_logger_gen.h from examples/hello_c
    cmake --build $BUILD_DIR
    clang-tidy --config-file="${REPO_DIR}/.clang-tidy" -p $BUILD_DIR ${REPO_DIR}/src/min_logger/* -- -I$BUILD_DIR/examples/hello_c/
    if [ $? -ne 0 ]; then
        RET=1
    fi
fi

exit $RET
