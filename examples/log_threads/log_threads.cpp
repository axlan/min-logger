#include <string>
#include <thread>

#include <pthread.h>

#include <min_logger/min_logger.h>

using namespace std;

// The function we want to execute on the new thread.
void task(string msg)
{
    auto handle = pthread_self();
    pthread_setname_np(handle, (string("task") + msg).c_str());

    for (int i=0; i < 5; i++) {
        MIN_LOGGER_ENTER(MIN_LOGGER_DEBUG, T_IN);
        MIN_LOGGER_RECORD_STRING(MIN_LOGGER_INFO, msg.c_str(), T_NAME);
        MIN_LOGGER_RECORD_U64(MIN_LOGGER_INFO, i, T_VAL);
        MIN_LOGGER_LOG(MIN_LOGGER_INFO, "task{T_NAME}: {T_VAL}", T_LOG);
        MIN_LOGGER_EXIT(MIN_LOGGER_DEBUG, T_OUT);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main()
{
    // Constructs the new thread and runs it. Does not block execution.
    thread t1(task, "1");
    thread t2(task, "2");

    min_logger_write_thread_names();

    // Makes the main thread wait for the new thread to finish execution, therefore blocks its own execution.
    t1.join();
    t2.join();
}
