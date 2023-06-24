#include "blocking_queue.h"

#include <gtest/gtest.h>
#include <chrono>
#include <future>
#include <thread>

using namespace std::chrono_literals;

TEST(blocking_queue, test_basic) {
    pedro::BlockingQueue<int> queue(100);
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(queue.Offer(i));
    }
    ASSERT_FALSE(queue.Offer(100));
    
    for (int i = 0; i < 100; ++i) {
        ASSERT_EQ(queue.Take(), i);
    }
    
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(queue.Offer(i));
    }
    
    for (int i = 0; i < 100; ++i) {
        ASSERT_EQ(queue.Take(), i);
    }
}

TEST(blocking_queue, test_put) {
    pedro::BlockingQueue<int> queue(100);
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(queue.Offer(i));
    }
    
    auto fut = std::async(std::launch::async, [&] {
        std::this_thread::sleep_for(500ms);
        queue.Take();
    });
    
    auto st = std::chrono::steady_clock::now();
    ASSERT_TRUE(queue.Put(100));
    auto et = std::chrono::steady_clock::now();
    ASSERT_TRUE(et - st > 100ms);
    
    fut.wait();
}

TEST(blocking_queue, test_take) {
    pedro::BlockingQueue<int> queue(100);

    ASSERT_EQ(queue.Poll(), std::nullopt);
    
    auto fut = std::async(std::launch::async, [&] {
        std::this_thread::sleep_for(500ms);
        ASSERT_TRUE(queue.Put(1024));
    });

    auto st = std::chrono::steady_clock::now();
    ASSERT_EQ(queue.Take(), 1024);
    auto et = std::chrono::steady_clock::now();
    ASSERT_TRUE(et - st > 100ms);

    fut.wait();
}

TEST(blocking_queue, test_close_take) {
    pedro::BlockingQueue<int> queue(100);

    auto fut = std::async(std::launch::async, [&] {
        std::this_thread::sleep_for(500ms);
        queue.Close();
    });

    auto st = std::chrono::steady_clock::now();
    ASSERT_EQ(queue.Take(), std::nullopt);
    auto et = std::chrono::steady_clock::now();
    ASSERT_TRUE(et - st > 100ms);

    ASSERT_TRUE(queue.Closed());

    fut.wait();
}

TEST(blocking_queue, test_close_put) {
    pedro::BlockingQueue<int> queue(1);

    ASSERT_TRUE(queue.Offer(1024));
    
    auto fut = std::async(std::launch::async, [&] {
        std::this_thread::sleep_for(500ms);
        queue.Close();
    });

    auto st = std::chrono::steady_clock::now();
    ASSERT_FALSE(queue.Put(1));
    auto et = std::chrono::steady_clock::now();
    ASSERT_TRUE(et - st > 100ms);
    
    ASSERT_TRUE(queue.Closed());
    
    ASSERT_EQ(queue.Take(), 1024);
    ASSERT_FALSE(queue.Offer(1));
    ASSERT_FALSE(queue.Put(1));

    fut.wait();
}