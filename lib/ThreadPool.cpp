#include "Pcheader.h"

#include <nwork/Queue.h>
#include <nwork/ThreadPool.h>

namespace nwork
{

	ThreadPool::ThreadPool(
		Queue*				aWorkQueue,
		size_t				aNumThreads)
	{		
		if(aNumThreads == 0)
			aNumThreads = GetCPUCount();

		for (size_t i = 0; i < aNumThreads; i++)
		{
			std::unique_ptr<std::thread> t = std::make_unique<std::thread>([&, aWorkQueue]()
			{
				while (!m_stop)
				{
					// Don't spin if the queue is broken
					if(aWorkQueue->WaitAndExecute(100) == Queue::WAIT_RESULT_ERROR)
						std::this_thread::sleep_for(std::chrono::milliseconds(100));
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