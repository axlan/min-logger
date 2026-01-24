#!/usr/bin/env bash

set -e

rm -rf build

cmake -S . -B build -G Ninja -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=RelWithDebInfo 
cmake --build build -j"$(nproc)"

uv --project python run min-logger-validate-types build/examples/custom_type/custom_type --type_defs examples/custom_type/custom_type_type_defs.json

for example in hello_c hello_cpp custom_type log_threads; do
    "./build/examples/$example/$example" > build/${example}_out.bin
     uv --project python run min-logger-parser build/examples/$example/${example}_min_logger.json --log_format=BINARY --log_file=build/${example}_out.bin | tee build/${example}_out.txt
done

./build/examples/log_threads/log_threads micro > build/log_threads_micro_out.bin
uv --project python run min-logger-parser build/examples/log_threads/log_threads_min_logger.json --log_format=MICRO_BINARY --log_file=build/log_threads_micro_out.bin | tee build/log_threads_micro_out.txt

set -x

grep -q "hello world binary" build/hello_c_out.txt

grep -q "hello world binary" build/hello_cpp_out.txt

grep -q "An integer value: 100" build/custom_type_out.txt
grep -q "An integer value: hello" build/custom_type_out.txt
IFS='' read -r -d '' var <<'EOF' || true
rectangle: {'pos': {'x': 0.0, 'y': 0.0, 'dummy': [1, 2]}, 'size': {'x': 5.0, 'y': 5.0, 'dummy': [0, 0]}, 'str': 'ccat', 'bytes': b'\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00'}
EOF
grep -q -F "$var"  build/custom_type_out.txt
IFS='' read -r -d '' var <<'EOF' || true
rectangle: [{'pos': {'x': 0.0, 'y': 0.0, 'dummy': [1, 2]}, 'size': {'x': 5.0, 'y': 5.0, 'dummy': [0, 0]}, 'str': 'ccat', 'bytes': b'\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00'}, {'pos': {'x': 1.0, 'y': 1.0, 'dummy': [0, 0]}, 'size': {'x': 10.0, 'y': 10.0, 'dummy': [0, 0]}, 'str': '', 'bytes': ''}]
EOF
grep -q -F "$var"  build/custom_type_out.txt

grep -q -F "task2] task: 4" build/log_threads_out.txt

grep -q -F "task2] task: 4" build/log_threads_micro_out.txt
