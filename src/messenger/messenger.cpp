#include "cvm/messenger.hpp"
#include <chrono>

using namespace std::chrono_literals;

DEFINE_bool(signal_async, false, "cvm::messenger signals serviced by another thread. This is for DPI calls that we make non-streaming for low-latency, but which could stall the emulator while being serviced");

void cvm::messenger::flush() {

    decltype(signal_queue_  ) q;
    decltype(signal_storage_) s;

    while (1) {
        {
            std::unique_lock<std::mutex> lock(signal_mutex_);
            while (signal_queue_.empty()) {
                if (!FLAGS_signal_async || quit_) return;
                signal_condition_.wait_for(lock, 100ms);
            }
            q.swap(signal_queue_  );
            s.swap(signal_storage_);
        }

        for(auto& f : q) {
            if(f(*this, s)) {
                // not necessary all the time - need to use a GC?
                clean_tasks();
            }
        }

        q.clear();
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
    if (signal_thread_.joinable()) {
        signal_thread_.join();
    }
    {
        std::lock_guard<std::mutex> tasks_guard(tasks_mutex_);
        tasks_.clear();
    }
    {
        std::lock_guard<std::mutex> pools_guard(pools_mutex_);
        pools_.clear();
    }
    {
        std::lock_guard<std::mutex> signal_queue_guard(signal_mutex_);
        signal_queue_.clear();
        signal_storage_.clear();
    }
}
