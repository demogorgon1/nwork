#include "Pcheader.h"

#include <random>
#include <unordered_set>

#include <nwork/API.h>

namespace nwork_test
{

	namespace 
	{	

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

			aWorkQueue->ForEachInRange(aMin, aMax, [&](
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

			aWorkQueue->ForEachVector<uint32_t>(testVector, [&](
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

				for (size_t i = 0; i < 1000; i++)
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
					{
						std::lock_guard lock(valuesLock);
						if (values.size() == 1000)
							break;
					}

					std::this_thread::sleep_for(std::chrono::milliseconds(10));
				}

				for(size_t i = 0; i < 1000; i++)
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

		void
		_TestForEach(
			nwork::Queue*				aWorkQueue)
		{
			std::mt19937 random(1234);

			for (size_t i = 1; i <= 16; i++)
			{
				aWorkQueue->SetForEachConcurrency(i);

				{
					_TestForEachInRange(aWorkQueue, 0, 0);

					for (size_t j = 0; j < 100; j++)
					{
						int32_t rangeMin = 0;
						int32_t rangeMax = 0;
						_MakeRandomRange(random, rangeMin, rangeMax);
						_TestForEachInRange(aWorkQueue, rangeMin, rangeMax);
					}
				}

				for (size_t j = 0; j < 100; j++)
					_TestForEachVector(aWorkQueue, j);
			}
		}

		void
		_TestGroups(
			nwork::Queue*				aWorkQueue)
		{
			// Stack, non-fixed size
			{
				std::atomic_bool fA = false;
				std::atomic_bool fB = false;
				std::atomic_bool fC = false;

				nwork::Group group;
				
				aWorkQueue->PostFunctionWithGroup(&group, [&]()
				{
					assert(!fA);
					fA = true;
				});

				aWorkQueue->PostFunctionWithGroup(&group, [&]()
				{
					assert(!fB);
					fB = true;
				});

				aWorkQueue->PostFunctionWithGroup(&group, [&]()
				{
					assert(!fC);
					fC = true;
				});

				group.Wait();

				assert(fA && fB && fC);
			}

			// Ref-counted, non-fixed size
			{
				std::atomic_bool fA = false;
				std::atomic_bool fB = false;
				std::atomic_bool fC = false;

				nwork::Reference<nwork::Group> group(nwork::Group::NewReferenceCounted());
				
				aWorkQueue->PostFunctionWithGroup(group, [&]()
				{
					assert(!fA);
					fA = true;
				});

				aWorkQueue->PostFunctionWithGroup(group, [&]()
				{
					assert(!fB);
					fB = true;
				});

				aWorkQueue->PostFunctionWithGroup(group, [&]()
				{
					assert(!fC);
					fC = true;
				});

				group->Wait();

				assert(fA && fB && fC);
			}

			// Ref-counted, fixed size, with completion function
			{
				std::atomic_bool f = false;
				std::binary_semaphore allCompleted(0);

				{
					nwork::Reference<nwork::Group> group(nwork::Group::NewReferenceCounted(nwork::Group::FLAG_FIXED_SIZE, 1));

					group->SetCompletionFunction([&]()
					{
						allCompleted.release();
					});

					aWorkQueue->PostFunctionWithGroup(group, [&]()
					{
						assert(!f);
						f = true;
					});
				}

				allCompleted.acquire();

				assert(f);
			}
		}

		void
		_TestReferences()
		{
			struct TestObject
			{
				void
				AddReference()
				{
					m_refCount++;
				}

				void
				RemoveReference()
				{
					if(--m_refCount == 0)
					{
						assert(!m_deleted);
						m_deleted = true;
					}
				}

				uint32_t				m_refCount = 0;
				bool					m_deleted = false;
			};

			{
				TestObject x;

				{
					nwork::Reference<TestObject> p(&x);
					assert(!x.m_deleted);
					assert(x.m_refCount == 1);
				}

				assert(x.m_deleted);
			}

			{
				TestObject x;

				{
					nwork::Reference<TestObject> p(&x);
					assert(!x.m_deleted);
					assert(x.m_refCount == 1);

					nwork::Reference<TestObject> a = p;
					assert(!x.m_deleted);
					assert(x.m_refCount == 2);

					assert(a == p);
					a.Release();

					assert(!x.m_deleted);
					assert(x.m_refCount == 1);

					assert(a.IsNull());
					assert(!p.IsNull());

					nwork::Reference<TestObject> b = std::move(p);
					assert(!x.m_deleted);
					assert(x.m_refCount == 1);

					assert(!b.IsNull());
					assert(p.IsNull());
				}

				assert(x.m_deleted);
			}

		}
	}

	void
	Run()
	{
		{
			nwork::Queue workQueue;
			nwork::ThreadPool threadPool(&workQueue, 8);

			_TestFunctions(&workQueue);
			_TestObjects(&workQueue);
			_TestForEach(&workQueue);
			_TestGroups(&workQueue);
			_TestReferences();
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