#include "Pcheader.h"

#include <nwork/Queue.h>
#include <nwork/ThreadPool.h>

namespace nwork
{

	ThreadPool::ThreadPool(
		Queue*				aWorkQueue,
		size_t				aNumThreads)
	{
		for (size_t i = 0; i < aNumThreads; i++)
		{
			std::unique_ptr<std::thread> t = std::make_unique<std::thread>([&, aWorkQueue]()
			{
				while (!m_stop)
				{
					aWorkQueue->WaitAndExecute(100);

					std::this_thread::yield();
				}
			});

			m_threads.push_back(std::move(t));
		}
	}

	ThreadPool::~ThreadPool()
	{
		m_stop = true;

		for (std::unique_ptr<std::thread>& t : m_threads)
		{
			t->join();
			t.reset();
		}
	}

}