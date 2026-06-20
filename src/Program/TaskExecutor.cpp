#include "TaskExecutor.h"

#include <algorithm>

namespace Chrivent {
	void TaskExecutor::WorkerLoop() {
		std::size_t observedGeneration = 0;
		while (true) {
			std::shared_ptr<TaskBatch> batch;
			{
				std::unique_lock lock(mutex);
				workCondition.wait(lock, [&] {
					return stopping || generation != observedGeneration;
				});
				if (stopping)
					return;
				observedGeneration = generation;
				batch = currentBatch;
			}
			ExecuteWork(batch);
		}
	}

	void TaskExecutor::ExecuteWork(const std::shared_ptr<TaskBatch>& batch) {
		while (true) {
			const std::size_t index = batch->nextIndex.fetch_add(1, std::memory_order_relaxed);
			if (index >= batch->workCount)
				return;
			batch->work(index);
			if (batch->remainingCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
				batch->completionCondition.notify_one();
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

	void TaskExecutor::Run(const std::size_t count, std::function<void(std::size_t)> task) {
		if (count == 0)
			return;
		const auto batch = std::make_shared<TaskBatch>(count, std::move(task));
		{
			const std::lock_guard lock(mutex);
			currentBatch = batch;
			generation++;
		}
		workCondition.notify_all();
		ExecuteWork(batch);
		std::unique_lock lock(batch->completionMutex);
		batch->completionCondition.wait(lock, [&] {
			return batch->remainingCount.load(std::memory_order_acquire) == 0;
		});
	}
}
