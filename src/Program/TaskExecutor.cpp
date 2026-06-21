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
			if (batch->remainingOperationCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
				batch->remainingOperationCount.notify_one();
		}
	}

	void TaskExecutor::ExecuteWork(const std::shared_ptr<TaskBatch>& batch) {
		while (true) {
			const std::size_t index = batch->nextIndex.fetch_add(1, std::memory_order_relaxed);
			if (index >= batch->workCount)
				return;
			batch->work(index);
			if (batch->remainingOperationCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
				batch->remainingOperationCount.notify_one();
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
		const auto batch = std::make_shared<TaskBatch>(count, workers.size(), std::move(task));
		{
			const std::lock_guard lock(mutex);
			currentBatch = batch;
			generation++;
		}
		workCondition.notify_all();
		ExecuteWork(batch);
		std::size_t remainingOperationCount = batch->remainingOperationCount.load(std::memory_order_acquire);
		while (remainingOperationCount != 0) {
			batch->remainingOperationCount.wait(remainingOperationCount, std::memory_order_acquire);
			remainingOperationCount = batch->remainingOperationCount.load(std::memory_order_acquire);
		}
	}
}
