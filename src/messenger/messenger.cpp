#include "cvm/messenger.hpp"
#include <chrono>

using namespace std::chrono_literals;

DEFINE_bool(signal_async, false, "cvm::messenger signals serviced by another thread. This is for DPI calls that we make non-streaming for low-latency, but which could stall the emulator while being serviced");
DEFINE_bool(signal_flush_switch_enable, false, "cvm::messenger switch immediately from low priority queue to high priority queue. Causes starvation");
DEFINE_uint64(signal_lower_priority_timeout_ns, 10000000, "Time in nanoseconds to spend servicing a lower priority queue before switching back to checking the high priority queue");

void cvm::messenger::flush() {

    std::remove_reference<decltype(signal_queue_  )>::type q;
    std::remove_reference<decltype(signal_storage_)>::type s;
    std::array<decltype(signal_queue_[0].begin()), num_priority> iterators;

    bool saw_quit = false;

    const std::chrono::nanoseconds signal_lower_priority_timeout{FLAGS_signal_lower_priority_timeout_ns};

    while (1) {
        for (int prio = highest_priority; prio >= lowest_priority; prio--) {
            if (q[prio].empty()) {
                std::unique_lock<std::mutex> lock(signal_mutex_);
                if (!signal_queue_[prio].empty()) {
                    q[prio].swap(signal_queue_  [prio]);
                    s[prio].swap(signal_storage_[prio]);
                    iterators[prio] = q[prio].begin();
                }
            }

            if (q[prio].empty()) {
                continue;
            }

            bool switching = false;


            auto start = std::chrono::steady_clock::now();

            for(; iterators[prio] != q[prio].end(); iterators[prio]++) {
                auto& [idx, f] = *iterators[prio];
                if(f(idx, *this, s[prio])) {
                    // not necessary all the time - need to use a GC?
                    clean_tasks();
                }
                if (FLAGS_signal_flush_switch_enable && FLAGS_signal_async && prio != highest_priority && !quit_.test()) {
                    auto now = std::chrono::steady_clock::now();
                    if ((now - start) > signal_lower_priority_timeout) {
                        switching = true;
                        iterators[prio]++;
                        break;
                    }
                }
            }
            if (iterators[prio] == q[prio].end()) {
                q[prio].clear();
            }

            if (switching) {
                prio = highest_priority + 1;
            }

        }

        if (!FLAGS_signal_async || saw_quit) break;
        saw_quit = quit_.test();
        if (!saw_quit) signal_queue_updated_.wait(false);
    }
}

void cvm::messenger::build() {
    quit_.clear();
    if (FLAGS_signal_async) {
        signal_thread_ = std::thread([this] () {this->flush();});
    }
}

void cvm::messenger::clear() {
    quit_.test_and_set();
    signal_queue_updated_.test_and_set();
    signal_queue_updated_.notify_one();
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
        for (int i = 0; i < num_priority; i++) {
            std::lock_guard<std::mutex> signal_queue_guard(signal_mutex_);
            signal_queue_[i].clear();
            signal_storage_[i].clear();
        }
    }
}
