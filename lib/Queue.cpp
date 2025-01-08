#include "Pcheader.h"

#include <nwork/Group.h>
#include <nwork/Object.h>
#include <nwork/Queue.h>

namespace nwork
{

	namespace
	{

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

			{
				SYSTEM_INFO si;
				GetSystemInfo(&si);
				m_forEachConcurrency = (size_t)si.dwNumberOfProcessors * 2;
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
			}
		#endif
	}
	
	Queue::~Queue()
	{
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
			assert(ok != 0);
		#else
			assert(m_eventFd != 0);

			m_internal->m_concurrentQueue.enqueue(aPacket);
			m_internal->m_queueLength++;

			uint64_t v = 1;
			ssize_t bytes = write(m_eventFd, &v, sizeof(v));
			assert(bytes == sizeof(v));
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

		uint32_t size = packet.m_header & 0x0FFFFFFF;
			
		if(size == 0x0FFFFFFF)
		{
			Type type = (Type)(packet.m_header & 0x30000000);			

			switch(type)
			{
			case TYPE_FOR_EACH_VECTOR:
				{
					const uint8_t* base = (const uint8_t*)packet.m_pointer1;
					const ForEachVectorContext* forEachContext = (const ForEachVectorContext*)packet.m_pointer2;
					size_t itemCount = (packet.m_header & FOR_EACH_VECTOR_FLAG_REMAINDER) != 0 ? forEachContext->m_itemCountPerWorkRemainder : forEachContext->m_itemCountPerWork;

					for (size_t i = 0; i < itemCount; i++)
					{
						const DummyClass* p = (const DummyClass*)(base + forEachContext->m_itemSize * i);
						forEachContext->m_function->operator()(*p);
					}

					forEachContext->m_semaphore->release();
				}
				break;

			case TYPE_FOR_EACH_IN_RANGE:
				{
					uint64_t range = reinterpret_cast<uint64_t>(packet.m_pointer1);
					const ForEachInRangeContext* forEachContext = (const ForEachInRangeContext*)packet.m_pointer2;
					int32_t workMin = _ExtractInt32RangeMin(range);
					int32_t workMax = _ExtractInt32RangeMax(range);

					for(int32_t i = workMin; i <= workMax; i++)
						forEachContext->m_function->operator()(i);

					forEachContext->m_semaphore->release();
				}
				break;

			case TYPE_FUNCTION:
				{
					std::function<void()>* p = (std::function<void()>*)packet.m_pointer1;
					assert(p != NULL);
					p->operator()();

					if(packet.m_header & FUNCTION_FLAG_DELETE)
						delete p;

					if(packet.m_pointer2 != NULL)
					{
						if(packet.m_header & FUNCTION_FLAG_GROUP)
						{
							Group* group = (Group*)packet.m_pointer2;
							group->OnCompletion();

							if(group->IsReferenceCounted())
								group->RemoveReference();
						}
						else
						{
							std::counting_semaphore<>* semaphore = (std::counting_semaphore<>*)packet.m_pointer2;
							semaphore->release();
						}
					}
				}					
				break;

			case TYPE_OBJECT:
				{
					Object* p = (Object*)packet.m_pointer1;
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

			m_ioFunction(size, packet.m_pointer2);
		}
		
		return WAIT_RESULT_OK;
	}

	void					
	Queue::BlockingForEachInRange(
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

		for (int32_t i = 0; i < workCount; i++)
			semaphore.acquire();
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
		packet.m_header = MakeHeader(TYPE_FUNCTION, FUNCTION_FLAG_DELETE);
		packet.m_pointer1 = (void*)aFunction;
		PostPacket(packet);
	}

	void					
	Queue::PostFunctionPointerWithSemaphore(
		std::counting_semaphore<>*				aSemaphore,
		std::function<void()>*					aFunction)
	{
		Packet packet;
		packet.m_header = MakeHeader(TYPE_FUNCTION, FUNCTION_FLAG_DELETE);
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
				DWORD lastError = GetLastError();

				if (lastError == WAIT_TIMEOUT)
					return WAIT_RESULT_TIMED_OUT;
				else if (lastError != ERROR_HANDLE_EOF)
					return WAIT_RESULT_ERROR;
			}

			return WAIT_RESULT_OK;
		#else
			assert(m_epollFd != 0);
			assert(m_eventFd != 0);

			struct epoll_event t;
			
			{
				int result = epoll_wait(m_epollFd, &t, 1, aMaxWaitTime);
				if (result == -1 && errno == EINTR)
					return WAIT_RESULT_TIMED_OUT;

				assert(result >= 0);

				if (result == 0)
					return WAIT_RESULT_TIMED_OUT;
			}

			if (t.data.ptr == NULL)
			{
				uint64_t v = 0;
				ssize_t bytes = read(m_eventFd, &v, sizeof(v));
				if(bytes < 0)
					return WAIT_RESULT_TIMED_OUT;

				while(m_internal->m_queueLength > 0)
				{
					if(m_internal->m_concurrentQueue.try_dequeue(aOut))
						break;
				}

				assert(m_internal->m_queueLength > 0);
				m_internal->m_queueLength--;
			}
			else
			{
				assert(false);
			}

			return WAIT_RESULT_OK;
		#endif
	}

}