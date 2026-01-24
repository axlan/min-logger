


`cmake -G Ninja -S . -B build -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=RelWithDebInfo`
`cmake --build build`

`stdbuf -oL ./build/examples/log_threads/log_threads | uv --project python run min-logger-parser build/examples/log_threads/log_threads_min_logger.json --log_format=TEXT`

`uv --project python run min-logger-validate-types build/examples/custom_type/custom_type --type_defs examples/custom_type/custom_type_type_defs.json`

hello_c Katai example:

```
meta:
  id: hello_c
  file-extension: hello_c.ksy
  endian: le
  encoding: ASCII
seq:
  - id: frames
    type: frame
    repeat: eos
types:
  frame:
    seq:
      - id: sync
        contents: [0xaf, 0xfa]
      - id: payload_len
        type: u1
      - id: thread_id
        type: u1
      - id: msg_id
        type: u4
      - id: timestamp
        type: u8
      - id: payload
        type:
          switch-on: msg_id
          cases:
            4294967040: str_value
            _: bytes_value
  str_value:
    seq:
      - id: value
        type: str
        size: _parent.payload_len
  bytes_value:
    seq:
      - id: value
        size: _parent.payload_len
enums:
  ip_protocol:
    0: log_0
    4294967040: thread_name
```
