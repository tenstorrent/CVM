#include <gtest/gtest.h>
#include "cvm/messenger.hpp"

struct transaction1 { int i = 0; };
struct transaction2 { int i = 0; };

cvm::messenger messenger1;

void long_running1(const transaction1& t) {
  EXPECT_EQ(t.i, 1);
  return;
}

TEST(Messenger, Basic) {
  messenger1.connect<transaction1>(1, long_running1);
  messenger1.fork([] () -> cvm::messenger::task<void> {
      while(1) {
        transaction1 t = co_await messenger1.wait<transaction1>(1);
        long_running1(t);
      }

      co_return;
    }
  );
  messenger1.signal(1, transaction1{1});
}

cvm::messenger messenger2;

cvm::messenger::task<void> long_running2() {
  while (1) {
    transaction1 t = co_await messenger2.wait<transaction1>(2);
    EXPECT_EQ(t.i, 2);
    transaction2 t2 = co_await messenger2.wait<transaction2>(2);
    EXPECT_EQ(t2.i, 2);
  }
  co_return;
}

TEST(Messenger, Suspend) {
  messenger2.fork(long_running2);
  messenger2.signal(2, transaction1{2});
  messenger2.signal(2, transaction2{2});
}

cvm::messenger messenger3;
bool nested = false;

cvm::messenger::task<void> nested_task() {
  transaction2 t2 = co_await messenger3.wait<transaction2>(3);
  EXPECT_EQ(t2.i, 3);
  nested = true;
  co_return;
}

cvm::messenger::task<void> long_running3() {
  while (1) {
    transaction1 t = co_await messenger3.wait<transaction1>(3);
    EXPECT_EQ(t.i, 3);
    co_await nested_task();
    EXPECT_EQ(nested, true);
  }
  co_return;
}

TEST(Messenger, Nested) {
  messenger3.fork(long_running3);
  messenger3.signal(3, transaction1{3});
  messenger3.signal(3, transaction2{3});
  EXPECT_EQ(nested, true);
}

cvm::messenger messenger4;
bool nested2 = false;
int popped = 0;

cvm::messenger::task<void> nested_task2() {
  nested2 = true;
  transaction2 t = co_await messenger4.wait<transaction2>(4);
  EXPECT_EQ(t.i, 4);
  co_return;
}

cvm::messenger::task<void> long_running4() {
  auto channel = messenger4.channel<transaction1>(4);
  while (1) {
    transaction1 t = co_await messenger4.wait<transaction1>(channel);
    popped++;
    EXPECT_EQ(t.i, 4);
    if (popped == 1) {
      co_await nested_task2();
      EXPECT_EQ(nested2, true);
    }
  }
}

TEST(Messenger, Channel) {
  messenger4.fork(long_running4);
  EXPECT_EQ(nested2, false);
  messenger4.signal(4, transaction1{4});
  EXPECT_EQ(nested2, true);
  messenger4.signal(4, transaction1{4});
  messenger4.signal(4, transaction1{4});
  messenger4.signal(4, transaction1{4});
  messenger4.signal(4, transaction2{4});
  EXPECT_EQ(popped, 4);
  EXPECT_EQ(nested2, true);
}

cvm::messenger messenger5;
unsigned hit = 0;

cvm::messenger::task<void> long_running5(cvm::messenger::pool<transaction1>::channel_info channel) {
  while (1) {
    co_await messenger5.wait<transaction1>(channel);
    hit++;
  }
}

TEST(Messenger, Filter) {
  auto channel = messenger5.channel<transaction1>(5, [](const transaction1& t) { return t.i == 1; });
  messenger5.signal(5, transaction1{0});
  messenger5.fork(long_running5, channel);
  messenger5.signal(5, transaction1{0});
  messenger5.signal(5, transaction1{1});
  messenger5.channel_filter<transaction1>(channel, [](const transaction1& t) { return t.i == 2; });
  messenger5.signal(5, transaction1{2});
  EXPECT_EQ(hit, 2);
}
