


cmake -G Ninja -S . -B build -DBUILD_SHARED_LIBS=OFF
cmake --build build

stdbuf -oL ./build/examples/log_threads/log_threads | uv --project python run min-logger-parser build/examples/log_threads/log_threads_min_logger.json --log_format=TEXT

note UV dependancy


TODO:
* Add way to validate type sizes. Could use objdump of symbols, or GDB? GDB with seperated debug symbols?
* Set custom levels per file
* Whitelist by ID
* Autogenerate id's - could use cmake to generate unique token for each file to concate, no obvious preprocessor only way
* Set optimizations for no values, 32 bit values, 64 bit values, or mixed size.
* Allow string messages to be compile time, or runtime disabled
* make argcomplete optional
* Add "tags" to categorize metrics to enable. (tags or logger names?)
* Have generated code look for metric, file, and category allow lists to include metric
* Add timestamp callback
* Switch weak link functions with registering callback function pointers
* Allow custimizing timestamp size/precision for basic text implementation
* Add functions to log scheduler using cores to run tasks and idle (need special hooks into scheduler)
* Figure out how to handle logging from interrupts. Special functions?
* Add option to replace ID's written into C code with incrementing value hex
* Add ease of use helper to allow log function names to be generated from MACRO defining file name and line number


# Design Decisions

* Include function name in source info? - This is possible from the https://gcc.gnu.org/onlinedocs/gcc/Function-Names.html macros in GCC, but is not trivial from the Python builder. Leaving it out since it's not super useful.
* How to specify categorizization for log statements? I seems like the typical way is to use named loggers. I think for this I'm going to go with tags since there's the preprocessing step if you really want to filter.

