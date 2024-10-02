#include "../main.cpp"
#include <atomic>
#include <gtest/gtest.h>
#include <thread>

class LockFreeCircularBufferTest : public ::testing::Test {
protected:
  void SetUp() override {
    buffer = std::make_unique<lock_free_circular_buffer<int>>(5);
  }

  std::unique_ptr<lock_free_circular_buffer<int>> buffer;
};

TEST_F(LockFreeCircularBufferTest, PushAndPop) {
  buffer->push(1);
  buffer->push(2);
  buffer->push(3);

  int value;
  ASSERT_TRUE(buffer->pop(value));
  EXPECT_EQ(value, 1);
  ASSERT_TRUE(buffer->pop(value));
  EXPECT_EQ(value, 2);
  ASSERT_TRUE(buffer->pop(value));
  EXPECT_EQ(value, 3);
  ASSERT_FALSE(buffer->pop(value));
}

TEST_F(LockFreeCircularBufferTest, OverwriteOldestData) {
  for (int i = 0; i < 6; ++i) {
    buffer->push(i);
  }

  int value;
  ASSERT_TRUE(buffer->pop(value));
  EXPECT_EQ(value, 1);
  ASSERT_TRUE(buffer->pop(value));
  EXPECT_EQ(value, 2);
}

TEST_F(LockFreeCircularBufferTest, Peek) {
  buffer->push(10);
  buffer->push(20);
  buffer->push(30);

  int value;
  ASSERT_TRUE(buffer->peek(0, value));
  EXPECT_EQ(value, 10);
  ASSERT_TRUE(buffer->peek(1, value));
  EXPECT_EQ(value, 20);
  ASSERT_TRUE(buffer->peek(2, value));
  EXPECT_EQ(value, 30);
  ASSERT_FALSE(buffer->peek(3, value));
}

TEST_F(LockFreeCircularBufferTest, Size) {
  EXPECT_EQ(buffer->size(), 0);
  buffer->push(1);
  EXPECT_EQ(buffer->size(), 1);
  buffer->push(2);
  EXPECT_EQ(buffer->size(), 2);
  int value;
  buffer->pop(value);
  EXPECT_EQ(buffer->size(), 1);
}

TEST_F(LockFreeCircularBufferTest, GetSpan) {
  buffer->push(1);
  buffer->push(2);
  buffer->push(3);

  auto span = buffer->get_span();
  EXPECT_EQ(span.size(), 3);
  EXPECT_EQ(span[0], 1);
  EXPECT_EQ(span[1], 2);
  EXPECT_EQ(span[2], 3);
}

TEST_F(LockFreeCircularBufferTest, ConcurrentPushPop) {
  std::atomic<int> sum(0);
  std::thread producer([this]() {
    for (int i = 1; i <= 1000; ++i) {
      buffer->push(i);
    }
  });

  std::thread consumer([this, &sum]() {
    int value;
    for (int i = 0; i < 1000; ++i) {
      while (!buffer->pop(value)) {
        std::this_thread::yield();
      }
      sum += value;
    }
  });

  producer.join();
  consumer.join();

  EXPECT_EQ(sum, 500500); // Sum of numbers from 1 to 1000
}
