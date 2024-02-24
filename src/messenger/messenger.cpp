#include "cvm/messenger.hpp"
#include <chrono>

using namespace std::chrono_literals;

DEFINE_bool(signal_async, false, "cvm::messenger signals serviced by another thread. This is for DPI calls that we make non-streaming for low-latency, but which could stall the emulator while being serviced");
DEFINE_uint64(signal_lower_priority_timeout_ns, 1000000, "Time in nanoseconds to spend servicing a lower priority queue before switching back to checking the high priority queue");

void cvm::messenger::flush() {

    std::remove_reference<decltype(signal_queue_  )>::type q;
    std::remove_reference<decltype(signal_storage_)>::type s;
    std::array<decltype(signal_queue_[0].begin()), num_priority> iterators;

    std::atomic_flag switching = ATOMIC_FLAG_INIT;
    std::atomic_flag timing    = ATOMIC_FLAG_INIT;
    std::jthread switching_thread;
    if (FLAGS_signal_async) {
        switching_thread = std::jthread([this,&timing,&switching] () {
            auto sleep = std::chrono::nanoseconds(FLAGS_signal_lower_priority_timeout_ns);
            while (!quit_.test()) {
                timing.wait(false);
                std::this_thread::sleep_for(sleep);
                switching.test_and_set();
                switching.notify_one();
            }
        });
    }

    bool saw_quit = false;

    std::array<time_point, num_priority> signal_swap_time;
    time_point prev_func_start_time  = std::chrono::high_resolution_clock::now();
    time_point prev_func_finish_time = std::chrono::high_resolution_clock::now();
    time_point sleep_time            = std::chrono::high_resolution_clock::now();
    time_point wakeup_time           = std::chrono::high_resolution_clock::now();

    while (1) {
        for (int prio = highest_priority; prio >= lowest_priority; prio--) {
            if (q[prio].empty()) {
                std::unique_lock<std::mutex> lock(signal_mutex_);
                if (!signal_queue_[prio].empty()) {
                    q[prio].swap(signal_queue_  [prio]);
                    s[prio].swap(signal_storage_[prio]);
                    iterators[prio] = q[prio].begin();
                    signal_swap_time[prio] = std::chrono::high_resolution_clock::now();
                }
            }

            if (q[prio].empty()) {
                continue;
            }

            bool switching = false;

            for(; iterators[prio] != q[prio].end(); iterators[prio]++) {
                auto& [idx, f] = *iterators[prio];
                auto func_start_time = std::chrono::high_resolution_clock::now();
                if(f(idx, *this, s[prio], std::make_tuple(prev_func_start_time, prev_func_finish_time, sleep_time, wakeup_time, signal_swap_time[prio]))) {
                    // not necessary all the time - need to use a GC?
                    clean_tasks();
                }
                prev_func_finish_time = std::chrono::high_resolution_clock::now();
                prev_func_start_time = func_start_time;
                if (FLAGS_signal_async && prio != highest_priority && !quit_.test()) {
                    switching = true;
                    break;
                }
                /*
                if (FLAGS_signal_async && prio != highest_priority && switching.test() && !quit_.test()) {
                //  break;
                }
                */
            }
            if (iterators[prio] == q[prio].end()) {
                q[prio].clear();
            }

            if (switching) {
                prio = highest_priority + 1;
            }

            /*
            if (switching.test() && !quit_.test()) {
                prio = highest_priority + 1;
            }

            if (prio == highest_priority) {
                switching.clear();
                timing.test_and_set();
                timing.notify_one();
            }
            */
        }

        if (!FLAGS_signal_async || saw_quit) break;
        saw_quit = quit_.test();
        sleep_time = std::chrono::high_resolution_clock::now();
        if (!saw_quit) signal_queue_updated_.wait(false);
        wakeup_time = std::chrono::high_resolution_clock::now();
    }

    timing.test_and_set();
    timing.notify_one();
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
