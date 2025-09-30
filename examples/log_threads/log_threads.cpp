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
        MIN_LOGGER_RECORD_STRING(MIN_LOGGER_INFO, "T_NAME", msg.c_str());
        MIN_LOGGER_RECORD_VALUE(MIN_LOGGER_INFO, "LOOP_COUNT", MIN_LOGGER_PAYLOAD_U64, i);
        MIN_LOGGER_LOG(MIN_LOGGER_INFO, "task${T_NAME}: ${LOOP_COUNT}");
        MIN_LOGGER_EXIT(MIN_LOGGER_DEBUG, "TASK_LOOP");
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main() {
    min_logger_write_thread_names();

    // Constructs the new thread and runs it. Does not block execution.
    thread t1(task, "1");
    thread t2(task, "2");

    // Makes the main thread wait for the new thread to finish execution, therefore blocks its own
    // execution.
    t1.join();
    t2.join();

    *min_logger_is_verbose() = true;

    // Constructs the new thread and runs it. Does not block execution.
    thread t3(task, "3");
    thread t4(task, "4");

    // Makes the main thread wait for the new thread to finish execution, therefore blocks its own
    // execution.
    t3.join();
    t4.join();
}
