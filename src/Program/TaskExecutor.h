#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace Chrivent {
	class TaskExecutor {
		struct TaskBatch {
			explicit TaskBatch(
				const std::size_t count,
				std::function<void(std::size_t)> task)
				: work(std::move(task)),
				  workCount(count),
				  remainingCount(count) {}

			std::function<void(std::size_t)> work;
			std::size_t workCount;
			std::atomic_size_t nextIndex = 0;
			std::atomic_size_t remainingCount;
			std::mutex completionMutex;
			std::condition_variable completionCondition;
		};

		std::vector<std::thread> workers;
		std::mutex mutex;
		std::condition_variable workCondition;
		std::shared_ptr<TaskBatch> currentBatch;
		std::size_t generation = 0;
		bool stopping = false;

		// 새 작업 세대를 기다린 뒤 할당된 작업 인덱스를 처리한다.
		void WorkerLoop();
		// 전달된 작업 묶음에서 아직 처리되지 않은 인덱스를 실행한다.
		static void ExecuteWork(const std::shared_ptr<TaskBatch>& batch);

	public:
		TaskExecutor();
		~TaskExecutor();

		TaskExecutor(const TaskExecutor&) = delete;
		TaskExecutor& operator=(const TaskExecutor&) = delete;
		TaskExecutor(TaskExecutor&&) = delete;
		TaskExecutor& operator=(TaskExecutor&&) = delete;

		// 지정한 개수의 독립 작업을 고정 워커와 호출 스레드에 분배하고 완료를 기다린다.
		void Run(std::size_t count, std::function<void(std::size_t)> task);
	};
}
