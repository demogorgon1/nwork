#include "Pcheader.h"

#include <nwork/Queue.h>
#include <nwork/Work.h>

namespace nwork_test
{

	class WorkerThreads
	{
	public:
		WorkerThreads(
			nwork::Queue*		aWorkQueue,
			size_t				aNumThreads)
		{
			for(size_t i = 0; i < aNumThreads; i++)
			{
				std::unique_ptr<std::thread> t = std::make_unique<std::thread>([&, aWorkQueue]()
				{
					while(!m_stop)
					{
						aWorkQueue->WaitAndExecute(100);

						std::this_thread::yield();
					}
				});

				m_threads.push_back(std::move(t));
			}
		}

		~WorkerThreads()
		{
			m_stop = true;

			for(std::unique_ptr<std::thread>& t : m_threads)
			{
				t->join();
				t.reset();
			}
		}

		// Public data
		std::vector<std::unique_ptr<std::thread>>	m_threads;
		std::atomic_bool							m_stop = false;
	};

	void
	Run()
	{
		{
			nwork::Queue workQueue(8);
			WorkerThreads workerThreads(&workQueue, 8);

			std::vector<uint32_t> test;
			for(size_t i = 0; i < 20; i++)
				test.push_back(i);

			workQueue.BlockingForEachVector<uint32_t>(test, [&](
				const uint32_t& aItem)
			{
				printf("%u\n", aItem);
			});

			for(;;)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			}
		}
	}

}

int
main(
	int		/*aNumArgs*/,
	char**	/*aArgs*/)
{
	nwork_test::Run();

	return EXIT_SUCCESS;
}