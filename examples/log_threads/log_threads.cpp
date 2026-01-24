#include <min_logger/min_logger.h>
#include <pthread.h>

#include <string>
#include <thread>

using namespace std;

// The function we want to execute on the new thread.
void task(string msg) {
    auto handle = pthread_self();
    msg = string("task") + msg;
    pthread_setname_np(handle, (msg).c_str());

    for (uint64_t i = 0; i < 5; i++) {
        MIN_LOGGER_ENTER(MIN_LOGGER_DEBUG, "TASK_LOOP");
        MIN_LOGGER_RECORD_AND_LOG_VALUE(MIN_LOGGER_INFO, "LOOP_COUNT", uint64_t, i,
                                        "task: ${LOOP_COUNT}");
        MIN_LOGGER_EXIT(MIN_LOGGER_DEBUG, "TASK_LOOP");
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main(const int argc, const char** argv) {
    if (argc > 1) {
        min_logger_set_serialize_format(MIN_LOGGER_MICRO_BINARY_SERIALIZED_FORMAT);
    } else {
        min_logger_set_serialize_format(MIN_LOGGER_DEFAULT_BINARY_SERIALIZED_FORMAT);
    }

    min_logger_write_thread_names();

    // Constructs the new thread and runs it. Does not block execution.
    thread t1(task, "1");
    thread t2(task, "2");

    // Makes the main thread wait for the new thread to finish execution, therefore blocks its own
    // execution.
    t1.join();
    t2.join();
}
