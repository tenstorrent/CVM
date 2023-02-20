#include <vector>
#include <functional>

std::vector<std::function<void()>> messenger_disconnects_;

extern "C" {

    void cvm_messenger_reset() {
        for (const auto& discon : messenger_disconnects_) {
            discon();
        }
        messenger_disconnects_.clear();
    }
}
