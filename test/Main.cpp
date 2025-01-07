#include "Pcheader.h"

#include <random>
#include <unordered_set>

#include <nwork/Queue.h>
#include <nwork/Object.h>

namespace nwork_test
{

	namespace 
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
		_MakeRandomRange(
			std::mt19937&				aRandom,
			int32_t&					aOutMin,
			int32_t&					aOutMax)
		{
			aOutMin = -100 + (int32_t)(aRandom() % 200);
			aOutMax = aOutMin + (int32_t)(aRandom() % 200);
		}

		void
		_TestForEachInRange(
			nwork::Queue*				aWorkQueue,
			int32_t						aMin,
			int32_t						aMax)
		{
			std::mutex valuesLock;
			std::unordered_set<int32_t> values;

			aWorkQueue->BlockingForEachInRange(aMin, aMax, [&](
				int32_t aValue)
			{
				std::lock_guard lock(valuesLock);
				assert(!values.contains(aValue));
				values.insert(aValue);
			});

			for(int32_t i = aMin; i <= aMax; i++)
				assert(values.contains(i));

			for(int32_t value : values)
				assert(value >= aMin && value <= aMax);
		}

		void
		_TestForEachVector(
			nwork::Queue*				aWorkQueue,
			size_t						aSize)
		{
			std::vector<uint32_t> testVector;
			for(size_t i = 0; i < aSize; i++)
				testVector.push_back(i);

			std::mutex valuesLock;
			std::unordered_set<uint32_t> values;

			aWorkQueue->BlockingForEachVector<uint32_t>(testVector, [&](
				const uint32_t& aItem)
			{
				std::lock_guard lock(valuesLock);
				assert(!values.contains(aItem));
				values.insert(aItem);
			});

			for(uint32_t i : testVector)
				assert(values.contains(i));

			assert(values.size() == testVector.size());
		}
		
		void
		_TestFunctions(
			nwork::Queue*				aWorkQueue)
		{
			// Post a bunch of functions with any completion events
			{
				std::mutex valuesLock;
				std::unordered_set<uint32_t> values;

				for (size_t i = 0; i < 10; i++)
				{
					aWorkQueue->PostFunction([&, i]()
					{
						std::lock_guard lock(valuesLock);
						assert(!values.contains(i));
						values.insert(i);
					});
				}

				for(;;)
				{
					std::lock_guard lock(valuesLock);
					if(values.size() == 10)
						break;

					std::this_thread::sleep_for(std::chrono::milliseconds(10));
				}

				for(size_t i = 0; i < 10; i++)
					assert(values.contains(i));
			}

			// Post a function with a semaphore
			{
				std::counting_semaphore<> semaphore(0);
				uint32_t x = 0;
				aWorkQueue->PostFunctionWithSemaphore(&semaphore, [&]()
				{
					x = 1234;
				});
				semaphore.acquire();
				assert(x == 1234);
			}
		}

		void
		_TestObjects(
			nwork::Queue*				aWorkQueue)
		{
			struct TestObject
				: public nwork::Object
			{
				TestObject()
				{

				}

				virtual
				~TestObject()
				{

				}

				// nwork::Object implementation
				void	
				ExecuteWork() override
				{
					assert(m_beforePostCalled);
					assert(!m_executeWorkCalled);
					assert(!m_afterExecuteCalled);

					m_executeWorkCalled = true;
				}

				void	
				BeforePost() override
				{
					assert(!m_beforePostCalled);
					assert(!m_executeWorkCalled);
					assert(!m_afterExecuteCalled);

					m_beforePostCalled = true;
				}

				void	
				AfterExecute() override
				{
					assert(m_beforePostCalled);
					assert(m_executeWorkCalled);
					assert(!m_afterExecuteCalled);

					m_afterExecuteCalled = true;
				}

				// Public data
				std::atomic_bool	m_executeWorkCalled = false;
				std::atomic_bool	m_beforePostCalled = false;
				std::atomic_bool	m_afterExecuteCalled = false;
			};

			TestObject testObject;
			aWorkQueue->PostObject(&testObject);

			while(!testObject.m_afterExecuteCalled)
				std::this_thread::sleep_for(std::chrono::milliseconds(100));

			assert(testObject.m_beforePostCalled);
			assert(testObject.m_executeWorkCalled);
		}
	}

	void
	Run()
	{
		{
			nwork::Queue workQueue;
			WorkerThreads workerThreads(&workQueue, 8);
			std::mt19937 random(1234);

			// Test ForEachInRange and ForEachVector
			for(size_t i = 1; i <= 16; i++)
			{
				workQueue.SetForEachConcurrency(i);

				{
					_TestForEachInRange(&workQueue, 0, 0);

					for (size_t j = 0; j < 100; j++)
					{
						int32_t rangeMin = 0;
						int32_t rangeMax = 0;
						_MakeRandomRange(random, rangeMin, rangeMax);
						_TestForEachInRange(&workQueue, rangeMin, rangeMax);
					}
				}

				for(size_t j = 0; j < 100; j++)
					_TestForEachVector(&workQueue, j);
			}

			// Other tests
			_TestFunctions(&workQueue);
			_TestObjects(&workQueue);
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