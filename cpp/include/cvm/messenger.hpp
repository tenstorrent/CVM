#include <boost/signals2.hpp>
#include <iostream>

namespace cvm {

    template <typename T>
        class messenger {

            public:

                typedef std::function<void(const T&)> listener;
                inline static boost::signals2::signal<void(const T&)> signal_;

                static void connect(const listener& l) {
                    signal_.connect(l);
                }

                static void signal(const T& t) {
                    signal_(t);
                }

        };

}
