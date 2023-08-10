#include "cvm/messenger.hpp"
#include <chrono>

using namespace std::chrono_literals;

DEFINE_bool(signal_async, false, "cvm::messenger signals serviced by another thread. This is for DPI calls that we make non-streaming for low-latency, but which could stall the emulator while being serviced");

void cvm::messenger::flush() {

    while (1) {

        std::function<void(void)> f;

        {
            std::unique_lock<std::mutex> lock(signal_mutex_);
            while (signal_queue_.empty()) {
                if (!FLAGS_signal_async || quit_) return;
                signal_condition_.wait_for(lock, 100ms);
            }
            f = std::move(signal_queue_.front());
            signal_queue_.erase(signal_queue_.begin());
        }

        f();
    }
}

void cvm::messenger::build() {
    quit_ = false;
    if (FLAGS_signal_async) {
        signal_thread_ = std::thread(std::bind(&cvm::messenger::flush, this));
    }
}

void cvm::messenger::clear() {
    quit_ = true;
    {
        std::lock_guard<std::mutex> tasks_guard(tasks_mutex_);
        tasks_.clear();
    }
    {
        std::lock_guard<std::mutex> pools_guard(pools_mutex_);
        pools_.clear();
    }
    if (signal_thread_.joinable()) {
        signal_thread_.join();
    }
    {
        std::lock_guard<std::mutex> signal_queue_guarg(signal_mutex_);
        signal_queue_.clear();
    }
}
