#include "TaskExecutor.h"

#include <algorithm>

namespace Chrivent {
	void TaskExecutor::WorkerLoop() {
		std::size_t observedGeneration = 0;
		while (true) {
			{
				std::unique_lock lock(mutex);
				workCondition.wait(lock, [&] {
					return stopping || generation != observedGeneration;
				});
				if (stopping)
					return;
				observedGeneration = generation;
			}
			ExecuteWork();
		}
	}

	void TaskExecutor::ExecuteWork() {
		while (true) {
			const std::size_t index = nextIndex.fetch_add(1, std::memory_order_relaxed);
			if (index >= workCount)
				return;
			work(index);
			if (remainingCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
				completionCondition.notify_one();
		}
	}

	TaskExecutor::TaskExecutor() {
		const unsigned int hardwareThreads = (std::max)(1u, std::thread::hardware_concurrency());
		workers.reserve(hardwareThreads - 1);
		for (unsigned int index = 1; index < hardwareThreads; index++)
			workers.emplace_back([this] { WorkerLoop(); });
	}

	TaskExecutor::~TaskExecutor() {
		{
			const std::lock_guard lock(mutex);
			stopping = true;
		}
		workCondition.notify_all();
		for (auto& worker : workers)
			worker.join();
	}

	void TaskExecutor::Run(
		const std::size_t count,
		std::function<void(std::size_t)> task) {
		if (count == 0)
			return;
		{
			const std::lock_guard lock(mutex);
			work = std::move(task);
			workCount = count;
			nextIndex.store(0, std::memory_order_relaxed);
			remainingCount.store(count, std::memory_order_release);
			generation++;
		}
		workCondition.notify_all();
		ExecuteWork();
		std::unique_lock lock(mutex);
		completionCondition.wait(lock, [&] {
			return remainingCount.load(std::memory_order_acquire) == 0;
		});
		work = {};
	}
}
