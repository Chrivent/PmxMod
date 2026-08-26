#include "Program/TaskExecutor.h"

#include <gtest/gtest.h>

#include <atomic>

namespace Chrivent {
	TEST(TaskExecutorContract, SkipsEmptyWork) {
		TaskExecutor executor;
		std::atomic_size_t callCount = 0;
		executor.Run(0, [&](const std::size_t) {
			callCount.fetch_add(1, std::memory_order_relaxed);
		});
		EXPECT_EQ(callCount.load(std::memory_order_relaxed), 0);
	}

	TEST(TaskExecutorContract, ExecutesSingleWorkExactlyOnce) {
		TaskExecutor executor;
		std::atomic_size_t callCount = 0;
		executor.Run(1, [&](const std::size_t index) {
			EXPECT_EQ(index, 0);
			callCount.fetch_add(1, std::memory_order_relaxed);
		});
		EXPECT_EQ(callCount.load(std::memory_order_relaxed), 1);
	}

	TEST(TaskExecutorContract, ExecutesAndReusesParallelWork) {
		TaskExecutor executor;
		constexpr std::size_t workCount = 64;
		std::atomic_size_t callCount = 0;
		std::atomic_size_t indexSum = 0;
		executor.Run(workCount, [&](const std::size_t index) {
			callCount.fetch_add(1, std::memory_order_relaxed);
			indexSum.fetch_add(index, std::memory_order_relaxed);
		});
		EXPECT_EQ(callCount.load(std::memory_order_relaxed), workCount);
		EXPECT_EQ(indexSum.load(std::memory_order_relaxed), workCount * (workCount - 1) / 2);
		callCount = 0;
		executor.Run(2, [&](const std::size_t) {
			callCount.fetch_add(1, std::memory_order_relaxed);
		});
		EXPECT_EQ(callCount.load(std::memory_order_relaxed), 2);
	}
}
