#pragma once

namespace nwork
{
	
	class Queue;

	class ThreadPool
	{
	public:
		ThreadPool(
			Queue*				aWorkQueue,
			size_t				aNumThreads);
		~ThreadPool();

	private:

		std::vector<std::unique_ptr<std::thread>>	m_threads;
		std::atomic_bool							m_stop = false;
	};

}