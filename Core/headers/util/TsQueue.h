#pragma once

namespace SNAKE {
	template<typename T>
	requires(std::is_pointer<T>::value)
	class tsQueue {
	public:
		void push_back(const T& data) {
			std::scoped_lock l(m_queue_mux);
			m_queue.push_back(data);
		}

		void push_front(const T& data) {
			std::scoped_lock l(m_queue_mux);
			m_queue.push_front(data);
		}

		T pop_front() {
			std::scoped_lock l(m_queue_mux);

			if (m_queue.empty())
				return nullptr;

			T val = m_queue.front();
			m_queue.pop_front();
			return val;
		}

		T pop_back() {
			std::scoped_lock l(m_queue_mux);

			if (m_queue.empty())
				return nullptr;

			T val = m_queue.back();
			m_queue.pop_back();
			return val;
		}

		size_t size() {
			std::scoped_lock l(m_queue_mux);
			return m_queue.size();
		}

		T front() {
			std::scoped_lock l(m_queue_mux);
			return m_queue.front();
		}

		T back() {
			std::scoped_lock l(m_queue_mux);
			return m_queue.back();
		}

		bool empty() {
			std::scoped_lock l(m_queue_mux);
			return m_queue.empty();
		}

		std::deque<T>& GetQueue() {
			return m_queue;
		}

	private:
		std::mutex m_queue_mux;
		std::deque<T> m_queue;
	};
}