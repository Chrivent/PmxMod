#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace Chrivent {
	class TaskExecutor {
		std::vector<std::thread> workers;
		std::mutex mutex;
		std::condition_variable workCondition;
		std::condition_variable completionCondition;
		std::function<void(std::size_t)> work;
		std::atomic_size_t nextIndex = 0;
		std::atomic_size_t remainingCount = 0;
		std::size_t workCount = 0;
		std::size_t generation = 0;
		bool stopping = false;

		// 새 작업 세대를 기다린 뒤 할당된 작업 인덱스를 처리한다.
		void WorkerLoop();
		// 현재 작업 세대에서 아직 처리되지 않은 인덱스를 실행한다.
		void ExecuteWork();

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
