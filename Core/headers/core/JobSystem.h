#pragma once
#include "util/TsQueue.h"
#include "util/util.h"

namespace SNAKE {
	struct Job {
		// _waited_on should be true if JobSystem::WaitOn(this) will be called
		Job(bool _waited_on) : is_waited_on(_waited_on) {};
		std::function<void(Job const*)> func = nullptr;
		std::atomic_uint32_t unfinished_jobs = 1;

		const bool is_waited_on;

		Job* p_parent = nullptr;
	};


	class JobSystem {
	public:
		static JobSystem& Get() {
			static JobSystem instance;
			return instance;
		}

		static void Init() {
			Get().InitImpl();
		}

		static bool IsBusy() {
			return Get().m_finished_jobs.load() < Get().m_assigned_jobs.load();
		}

		static void Execute(Job* job) {
			Get().ExecuteImpl(job);
		}

		static void WaitAll() {
			Get().WaitImpl();
		}

		static void Shutdown() {
			Get().ShutdownImpl();
		}

		static void WaitOn(Job* job) {
			SNK_DBG_ASSERT(job->is_waited_on);

			while (job->unfinished_jobs.load() != 0) {
				std::this_thread::yield();
			}

			if (job->p_parent)
				job->p_parent->unfinished_jobs.fetch_sub(1);

			Get().m_finished_jobs.fetch_add(1);

			delete job;
		}
		 
		static std::vector<std::thread::id> GetThreadIDs() {
			std::vector<std::thread::id> ret;
			ret.push_back(std::this_thread::get_id());

			for (auto& thread : Get().m_threads) {
				SNK_ASSERT(thread.joinable());
				ret.push_back(thread.get_id());
			}

			return ret;
		}
		tsQueue<Job*> m_job_queue;

		[[nodiscard]] static Job* CreateJob() {
			auto job = new Job(false);
			return job;
		}

		[[nodiscard]] static Job* CreateWaitedOnJob() {
			auto job = new Job(true);
			return job;
		}

	private:
		void ShutdownImpl() {
			m_is_running = false;
			m_wake_condition.notify_all();
			for (auto& thread : m_threads) {
				while (!thread.joinable()) {

				}
				thread.join();
			}
			m_thread_jobs.clear();
		}

		void WaitImpl() {
			while (IsBusy()) {
				if (!m_job_queue.empty()) {
					if (auto job = m_job_queue.pop_front()) {
						ProcessJob(job, std::this_thread::get_id());
					}
					m_wake_condition.notify_one();
				}

				std::this_thread::yield();
			}
		}

		void ExecuteImpl(Job* job) {
			m_assigned_jobs.fetch_add(1);
			job->p_parent = m_thread_jobs[std::this_thread::get_id()];

			if (job->p_parent)
				job->p_parent->unfinished_jobs.fetch_add(1);

			m_job_queue.push_back(job);
			m_wake_condition.notify_one();
		}

		void ProcessJob(Job* job, std::thread::id id) {
			m_thread_jobs[id] = job;

			job->func(job);

			while (job->unfinished_jobs.load() != 1) {
				std::this_thread::yield();
			}

			job->unfinished_jobs.fetch_sub(1);

			m_thread_jobs[id] = nullptr;

			if (!job->is_waited_on) {
				if (job->p_parent)
					job->p_parent->unfinished_jobs.fetch_sub(1);

				m_finished_jobs.fetch_add(1);
				delete job;
			}
		}

		void InitImpl() {
			auto num_threads_to_create = std::thread::hardware_concurrency() - 1;
			m_thread_jobs.reserve(num_threads_to_create + 1);

			for (unsigned i = 0; i < num_threads_to_create; i++) {
				m_threads.push_back(std::thread([this] {
					auto id = std::this_thread::get_id();

					while (m_is_running) {
						if (auto job = m_job_queue.pop_front()) {
							ProcessJob(job, id);
						}
						else {
							std::unique_lock<std::mutex> lock(m_wake_mutex);
							m_wake_condition.wait(lock);
						}
					}

					}));

			}
		}

		bool m_is_running = true;

		std::atomic_uint64_t m_finished_jobs = 0;
		std::atomic_uint64_t m_assigned_jobs = 0;

		std::condition_variable m_wake_condition;
		std::mutex m_wake_mutex;

		std::vector<std::thread> m_threads;

		// Thread-safe, each element only accessed by worker thread with id = key
		std::unordered_map<std::thread::id, Job*> m_thread_jobs;

	};
}