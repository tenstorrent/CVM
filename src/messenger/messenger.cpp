#include <vector>
#include <functional>

std::vector<std::function<void()>> messenger_disconnects_;

namespace cvm {
    namespace messenger_reset {
        void reset() {
            for (const auto& discon : messenger_disconnects_) {
                discon();
            }
            messenger_disconnects_.clear();
        };
    }
}
