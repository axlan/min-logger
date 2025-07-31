


cmake -G Ninja -S . -B build
cmake --build build

note UV dependancy


TODO:
* Set custom levels per file
* Whitelist by ID
* Autogenerate id's - could use cmake to generate unique token for each file to concate, no obvious preprocessor only way
* Set optimizations for no values, 32 bit values, 64 bit values, or mixed size.
* Allow string messages to be compile time, or runtime disabled
* make argcomplete optional
* Add "tags" to categorize metrics to enable.


