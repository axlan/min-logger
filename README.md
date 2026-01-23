


`cmake -G Ninja -S . -B build -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=RelWithDebInfo`
`cmake --build build`

`stdbuf -oL ./build/examples/log_threads/log_threads | uv --project python run min-logger-parser build/examples/log_threads/log_threads_min_logger.json --log_format=TEXT`

`uv --project python run min-logger-validate-types build/examples/custom_type/custom_type --type_defs examples/custom_type/custom_type_type_defs.json`

note UV dependancy

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

TODO:
* Add way to validate type sizes. Could use objdump of symbols, or GDB? GDB with seperated debug symbols? gdb-multiarch seems to work even for ESP32 (`/usr/bin/gdb-multiarch .pio/build/esp32dev/firmware.elf --batch -ex="output sizeof(int)"`).
* Make parsing robust to transmission errors, add advice for fixing format specification errors
* Making adding custom framing parsers easier. Maybe use <https://github.com/MightyPork/TinyFrame>?
* Make parser more efficient. Both in the framing and the value parsing. Value parsing should at least cache struct format and map to dict. Maybe check out [Kaitai](https://kaitai.io/)?
* Set custom levels per file
* Whitelist by ID
* make argcomplete optional
* Add "tags" to categorize metrics to enable. (tags or logger names?)
* Have generated code look for metric, file, and category allow lists to include metric
* Add functions to log scheduler using cores to run tasks and idle (need special hooks into scheduler)
* Figure out how to handle logging from interrupts. Special functions?


# Design Decisions

* Include function name in source info? - This is possible from the https://gcc.gnu.org/onlinedocs/gcc/Function-Names.html macros in GCC, but is not trivial from the Python builder. Leaving it out since it's not super useful.
* How to specify categorizization for log statements? I seems like the typical way is to use named loggers. I think for this I'm going to go with tags since there's the preprocessing step if you really want to filter.

