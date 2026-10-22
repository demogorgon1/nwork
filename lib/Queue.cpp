#include "Pcheader.h"

#include <nwork/Group.h>
#include <nwork/Object.h>
#include <nwork/Queue.h>

namespace nwork
{

	namespace
	{

		// Queue whose work is being executed by this thread
		thread_local Queue* t_executingQueue = NULL;

		struct ExecutingQueueScope
		{
			ExecutingQueueScope(
				Queue*															aQueue)
				: m_previous(t_executingQueue)
			{
				t_executingQueue = aQueue;
			}

			~ExecutingQueueScope()
			{
				t_executingQueue = m_previous;
			}

			Queue*	m_previous;
		};

		uint64_t
		_MakeInt32Range(
			int32_t															aMin,
			int32_t															aMax)
		{
			return static_cast<uint64_t>(static_cast<uint32_t>(aMin)) | (static_cast<uint64_t>(static_cast<uint32_t>(aMax)) << 32ULL);
		}

		int32_t
		_ExtractInt32RangeMin(
			uint64_t														aInt32Range)
		{
			return static_cast<int32_t>(aInt32Range & 0xFFFFFFFFULL);
		}

		int32_t
		_ExtractInt32RangeMax(
			uint64_t														aInt32Range)
		{
			return static_cast<int32_t>(aInt32Range >> 32ULL);
		}

	}

	//------------------------------------------------------------------------------------------------

	#if !defined(WIN32)
		struct Queue::Internal
		{
			moodycamel::ConcurrentQueue<Packet>		m_concurrentQueue;
			std::atomic_size_t						m_queueLength = 0;
		};
	#endif

	//------------------------------------------------------------------------------------------------

	Queue::Queue()
		: m_forEachConcurrency(GetCPUCount() * 2)
	{
		#if defined(WIN32)
			{
				m_iocpHandle = CreateIoCompletionPort(
					INVALID_HANDLE_VALUE,
					NULL,
					NULL,
					0);
				assert(m_iocpHandle);
			}
		#else
			m_internal = new Internal();

			{
				m_epollFd = epoll_create1(0);
				assert(m_epollFd >= 0);

				m_eventFd = eventfd(0, EFD_SEMAPHORE | EFD_NONBLOCK);
				assert(m_eventFd >= 0);
			}

			{
				struct epoll_event t;
				memset(&t, 0, sizeof(epoll_event));
				t.events = EPOLLIN | EPOLLEXCLUSIVE;
				t.data.ptr = NULL;

				int result = epoll_ctl(m_epollFd, EPOLL_CTL_ADD, m_eventFd, &t);
				assert(result == 0);
				(void)result;
			}
		#endif
	}

	Queue::~Queue()
	{
		// Get rid of work that never got executed
		{
			Packet packet;
			while(_WaitForPacket(0, packet) == WAIT_RESULT_OK)
			{
				_DiscardPacket(packet);
				packet = Packet();
			}
		}

		#if !defined(WIN32)
			delete m_internal;

			if (m_epollFd >= 0)
				close(m_epollFd);

			if (m_eventFd >= 0)
				close(m_eventFd);
		#endif
	}

	void
	Queue::SetForEachConcurrency(
		size_t				aForEachConcurrency)
	{
		assert(aForEachConcurrency > 0);
		m_forEachConcurrency = aForEachConcurrency;
	}

	void
	Queue::SetIOFunction(
		IOFunction			aIOFunction)
	{
		m_ioFunction = aIOFunction;
	}

	void
	Queue::PostPacket(
		const Packet&		aPacket)
	{
		#if defined(WIN32)
			assert(m_iocpHandle);

			BOOL ok = PostQueuedCompletionStatus(m_iocpHandle, (DWORD)aPacket.m_header, (ULONG_PTR)aPacket.m_pointer1, (LPOVERLAPPED)aPacket.m_pointer2);
			(void)ok;
			assert(ok != 0);
		#else
			assert(m_eventFd >= 0);

			m_internal->m_concurrentQueue.enqueue(aPacket);
			m_internal->m_queueLength++;

			uint64_t v = 1;
			ssize_t bytes = write(m_eventFd, &v, sizeof(v));
			assert(bytes == sizeof(v));
			(void)bytes;
		#endif
	}

	Queue::WaitResult
	Queue::WaitAndExecute(
		uint32_t			aMaxWaitTime)
	{
		Packet packet;
		WaitResult result = _WaitForPacket(aMaxWaitTime, packet);
		if(result != WAIT_RESULT_OK)
			return result;

		ExecutingQueueScope executingQueueScope(this);
		_ExecutePacket(packet);

		return WAIT_RESULT_OK;
	}

	void
	Queue::ForEachInRange(
		int32_t									aMin,
		int32_t									aMax,
		std::function<void(int32_t)>			aFunction)
	{
		assert(aMax >= aMin);

		std::counting_semaphore<> semaphore(0);

		ForEachInRangeContext context;
		context.m_function = &aFunction;
		context.m_semaphore = &semaphore;

		int32_t count = aMax - aMin + 1;
		int32_t step = count / (int32_t)m_forEachConcurrency;
		if(step == 0)
			step = 1;

		int32_t workCount = count / step;
		int32_t workMin = aMin;

		for (int32_t i = 0; i < workCount; i++)
		{
			uint32_t header = MakeHeader(TYPE_FOR_EACH_IN_RANGE, 0);
			uint64_t range = _MakeInt32Range(workMin, workMin + step - 1);
			PostPacket({ header, reinterpret_cast<void*>(range), (void*)&context });
			workMin += step;
		}

		if (count % step != 0)
		{
			uint32_t header = MakeHeader(TYPE_FOR_EACH_IN_RANGE, 0);
			uint64_t range = _MakeInt32Range(workMin, aMax);
			PostPacket({ header, reinterpret_cast<void*>(range), (void*)&context });
			workCount++;
		}

		_WaitForWork(semaphore, (size_t)workCount);
	}

	void
	Queue::PostFunction(
		std::function<void()>					aFunction)
	{
		std::function<void()>* f = new std::function<void()>();
		*f = std::move(aFunction);

		Packet packet;
		packet.m_header = MakeHeader(TYPE_FUNCTION, FUNCTION_FLAG_DELETE);
		packet.m_pointer1 = (void*)f;
		PostPacket(packet);
	}

	void
	Queue::PostFunctionWithSemaphore(
		std::counting_semaphore<>*				aSemaphore,
		std::function<void()>					aFunction)
	{
		std::function<void()>* f = new std::function<void()>();
		*f = std::move(aFunction);

		Packet packet;
		packet.m_header = MakeHeader(TYPE_FUNCTION, FUNCTION_FLAG_DELETE);
		packet.m_pointer1 = (void*)f;
		packet.m_pointer2 = (void*)aSemaphore;
		PostPacket(packet);
	}

	void
	Queue::PostFunctionWithGroup(
		Group*									aGroup,
		std::function<void()>					aFunction)
	{
		if(aGroup->IsReferenceCounted())
			aGroup->AddReference();

		aGroup->OnPost();

		std::function<void()>* f = new std::function<void()>();
		*f = std::move(aFunction);

		Packet packet;
		packet.m_header = MakeHeader(TYPE_FUNCTION, FUNCTION_FLAG_DELETE | FUNCTION_FLAG_GROUP);
		packet.m_pointer1 = (void*)f;
		packet.m_pointer2 = (void*)aGroup;
		PostPacket(packet);
	}

	void
	Queue::PostFunctionPointer(
		std::function<void()>*					aFunction)
	{
		Packet packet;
		packet.m_header = MakeHeader(TYPE_FUNCTION, 0);
		packet.m_pointer1 = (void*)aFunction;
		PostPacket(packet);
	}

	void
	Queue::PostFunctionPointerWithSemaphore(
		std::counting_semaphore<>*				aSemaphore,
		std::function<void()>*					aFunction)
	{
		Packet packet;
		packet.m_header = MakeHeader(TYPE_FUNCTION, 0);
		packet.m_pointer1 = (void*)aFunction;
		packet.m_pointer2 = (void*)aSemaphore;
		PostPacket(packet);
	}

	void
	Queue::PostObject(
		Object*									aObject)
	{
		aObject->BeforePost();

		Packet packet;
		packet.m_header = MakeHeader(TYPE_OBJECT, 0);
		packet.m_pointer1 = (void*)aObject;
		PostPacket(packet);
	}

	//------------------------------------------------------------------------------------------------

	Queue::WaitResult
	Queue::_WaitForPacket(
		uint32_t		aMaxWaitTime,
		Packet&			aOut)
	{
		#if defined(WIN32)
			assert(m_iocpHandle);

			BOOL ok = GetQueuedCompletionStatus(
				m_iocpHandle,
				(LPDWORD)&aOut.m_header,
				(PULONG_PTR)&aOut.m_pointer1,
				(LPOVERLAPPED*)&aOut.m_pointer2,
				(DWORD)aMaxWaitTime);

			if (ok == 0)
			{
				// Failed I/O operation, which the I/O function still needs to know about
				if (aOut.m_pointer2 != NULL)
					return WAIT_RESULT_OK;

				if (GetLastError() == WAIT_TIMEOUT)
					return WAIT_RESULT_TIMED_OUT;

				return WAIT_RESULT_ERROR;
			}

			return WAIT_RESULT_OK;
		#else
			assert(m_epollFd >= 0);
			assert(m_eventFd >= 0);

			struct epoll_event t;

			{
				int result = epoll_wait(m_epollFd, &t, 1, aMaxWaitTime);
				if (result == -1 && errno == EINTR)
					return WAIT_RESULT_TIMED_OUT;

				if (result < 0)
					return WAIT_RESULT_ERROR;

				if (result == 0)
					return WAIT_RESULT_TIMED_OUT;
			}

			if (t.data.ptr != NULL)
			{
				assert(false);
				return WAIT_RESULT_ERROR;
			}

			uint64_t v = 0;
			ssize_t bytes = read(m_eventFd, &v, sizeof(v));
			if(bytes < 0)
				return WAIT_RESULT_TIMED_OUT;

			// Got a token, so the packet has been enqueued (but try_dequeue() can still fail while other threads are using the queue)
			while(!m_internal->m_concurrentQueue.try_dequeue(aOut))
				;

			assert(m_internal->m_queueLength > 0);
			m_internal->m_queueLength--;

			return WAIT_RESULT_OK;
		#endif
	}

	void
	Queue::_ExecutePacket(
		const Packet&	aPacket)
	{
		uint32_t size = aPacket.m_header & 0x0FFFFFFF;

		if(size == 0x0FFFFFFF)
		{
			Type type = (Type)(aPacket.m_header & 0x30000000);

			switch(type)
			{
			case TYPE_FOR_EACH_VECTOR:
				{
					uint8_t* base = (uint8_t*)aPacket.m_pointer1;
					const ForEachVectorContext* forEachContext = (const ForEachVectorContext*)aPacket.m_pointer2;
					size_t itemCount = (aPacket.m_header & FOR_EACH_VECTOR_FLAG_REMAINDER) != 0 ? forEachContext->m_itemCountPerWorkRemainder : forEachContext->m_itemCountPerWork;

					for (size_t i = 0; i < itemCount; i++)
						forEachContext->m_invoke(forEachContext->m_function, base + forEachContext->m_itemSize * i);

					forEachContext->m_semaphore->release();
				}
				break;

			case TYPE_FOR_EACH_IN_RANGE:
				{
					uint64_t range = reinterpret_cast<uint64_t>(aPacket.m_pointer1);
					const ForEachInRangeContext* forEachContext = (const ForEachInRangeContext*)aPacket.m_pointer2;
					int32_t workMin = _ExtractInt32RangeMin(range);
					int32_t workMax = _ExtractInt32RangeMax(range);

					for(int32_t i = workMin; i <= workMax; i++)
						forEachContext->m_function->operator()(i);

					forEachContext->m_semaphore->release();
				}
				break;

			case TYPE_FUNCTION:
				{
					std::function<void()>* p = (std::function<void()>*)aPacket.m_pointer1;
					assert(p != NULL);
					p->operator()();

					if(aPacket.m_header & FUNCTION_FLAG_DELETE)
						delete p;

					if(aPacket.m_pointer2 != NULL)
					{
						if(aPacket.m_header & FUNCTION_FLAG_GROUP)
						{
							Group* group = (Group*)aPacket.m_pointer2;

							// Must be checked first: if not reference counted, the group can be gone as soon as it's completed
							bool isReferenceCounted = group->IsReferenceCounted();

							group->OnCompletion();

							if(isReferenceCounted)
								group->RemoveReference();
						}
						else
						{
							std::counting_semaphore<>* semaphore = (std::counting_semaphore<>*)aPacket.m_pointer2;
							semaphore->release();
						}
					}
				}
				break;

			case TYPE_OBJECT:
				{
					Object* p = (Object*)aPacket.m_pointer1;
					assert(p != NULL);
					p->ExecuteWork();
					p->AfterExecute();
				}
				break;

			default:
				assert(false);
			}

		}
		else
		{
			assert(m_ioFunction);

			m_ioFunction(size, aPacket.m_pointer2);
		}
	}

	void
	Queue::_DiscardPacket(
		const Packet&	aPacket)
	{
		uint32_t size = aPacket.m_header & 0x0FFFFFFF;

		if(size == 0x0FFFFFFF && (Type)(aPacket.m_header & 0x30000000) == TYPE_FUNCTION)
		{
			if(aPacket.m_header & FUNCTION_FLAG_DELETE)
				delete (std::function<void()>*)aPacket.m_pointer1;

			if(aPacket.m_pointer2 != NULL && (aPacket.m_header & FUNCTION_FLAG_GROUP) != 0)
			{
				Group* group = (Group*)aPacket.m_pointer2;
				if(group->IsReferenceCounted())
					group->RemoveReference();
			}
		}
	}

	void
	Queue::_ForEachVector(
		ForEachVectorContext*	aContext,
		uint8_t*				aVectorBase,
		size_t					aVectorSize)
	{
		assert(aVectorSize > 0);

		std::counting_semaphore<> semaphore(0);

		aContext->m_semaphore = &semaphore;
		aContext->m_itemCountPerWork = aVectorSize / m_forEachConcurrency;

		if (aContext->m_itemCountPerWork == 0)
			aContext->m_itemCountPerWork = 1;

		size_t workCount = aVectorSize / aContext->m_itemCountPerWork;

		if (workCount * aContext->m_itemCountPerWork < aVectorSize)
			aContext->m_itemCountPerWorkRemainder = aVectorSize - workCount * aContext->m_itemCountPerWork;

		uint8_t* p = aVectorBase;

		for (size_t i = 0; i < workCount; i++)
		{
			uint32_t header = MakeHeader(TYPE_FOR_EACH_VECTOR, 0);
			PostPacket({ header, (void*)p, (void*)aContext });
			p += aContext->m_itemSize * aContext->m_itemCountPerWork;
		}

		if (aContext->m_itemCountPerWorkRemainder > 0)
		{
			uint32_t header = MakeHeader(TYPE_FOR_EACH_VECTOR, FOR_EACH_VECTOR_FLAG_REMAINDER);
			PostPacket({ header, (void*)p, (void*)aContext });
			workCount++;
		}

		_WaitForWork(semaphore, workCount);
	}

	void
	Queue::_WaitForWork(
		std::counting_semaphore<>&	aSemaphore,
		size_t						aCount)
	{
		if(t_executingQueue != this)
		{
			for (size_t i = 0; i < aCount; i++)
				aSemaphore.acquire();
			return;
		}

		// Called from work executed by this queue. Blocking would take a worker thread out of the pool and could deadlock
		// if all of them did it, so execute other work while waiting.
		while(aCount > 0)
		{
			if(aSemaphore.try_acquire())
			{
				aCount--;
				continue;
			}

			Packet packet;
			if(_WaitForPacket(0, packet) == WAIT_RESULT_OK)
				_ExecutePacket(packet);
			else if(aSemaphore.try_acquire_for(std::chrono::milliseconds(1)))
				aCount--;
		}
	}

}
