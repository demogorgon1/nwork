#include "Pcheader.h"

#include <nwork/Queue.h>

namespace nwork
{

	Queue::Queue(
		size_t				aConcurrency)
		: m_concurrency(aConcurrency)
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
		#endif
	}
	
	Queue::~Queue()
	{

	}

	void
	Queue::PostPacket(
		const Packet&		aPacket)
	{
		#if defined(WIN32)
			assert(m_iocpHandle);

			BOOL ok = PostQueuedCompletionStatus(m_iocpHandle, (DWORD)aPacket.m_header, (ULONG_PTR)aPacket.m_pointer1, (LPOVERLAPPED)aPacket.m_pointer2);
			assert(ok != 0);
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

		uint32_t size = packet.m_header & 0xFFFFFF00;
			
		if(size == 0xFFFFFF00)
		{
			Type type = (Type)(packet.m_header & 0x3);

			switch(type)
			{
			case TYPE_FOR_EACH_VECTOR:
				{
					const uint8_t* base = (const uint8_t*)packet.m_pointer1;
					const ForEachContext* forEachContext = (const ForEachContext*)packet.m_pointer2;
					size_t itemCount = (packet.m_header & FOR_EACH_VECTOR_FLAG_LAST) != 0 ? forEachContext->m_itemCountPerWorkLast : forEachContext->m_itemCountPerWork;

					for (size_t i = 0; i < itemCount; i++)
					{
						const DummyClass* p = (const DummyClass*)(base + forEachContext->m_itemSize * i);
						forEachContext->m_function->operator()(*p);
					}
				}
				break;

			default:
				assert(false);
			}

		}
		else
		{
			assert(false);
		}
		
		return WAIT_RESULT_OK;
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
			return WAIT_RESULT_ERROR;
		#endif
	}

}