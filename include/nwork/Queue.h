#pragma once

#include "Work.h"

namespace nwork
{
	
	class Queue
	{
	public:
		enum WaitResult
		{
			WAIT_RESULT_OK,
			WAIT_RESULT_TIMED_OUT,
			WAIT_RESULT_ERROR
		};

		enum Type : uint32_t
		{
			TYPE_FOR_EACH_VECTOR
		};

		enum ForEachVectorFlag : uint32_t
		{
			FOR_EACH_VECTOR_FLAG_LAST = 0x4
		};

		struct Packet
		{
			uint32_t		m_header = 0;
			void*			m_pointer1 = NULL;
			void*			m_pointer2 = NULL;
		};

		static const uint32_t
		MakeHeader(
			Type										aType,
			uint32_t									aFlags,
			uint32_t									aSize = UINT32_MAX)
		{
			if(aSize != UINT32_MAX)
			{
				assert(aSize < 0xFFFFFF00);
				return aSize;
			}
			return 0xFFFFFF00 | (uint32_t)aType | aFlags;
		}

								Queue(
									size_t				aConcurrency);
								~Queue();

		void					PostPacket(
									const Packet&		aPacket);
		WaitResult				WaitAndExecute(
									uint32_t			aMaxWaitTime);

		template <typename _T>
		void
		BlockingForEachVector(
			const std::vector<_T>&				aVector,
			std::function<void(const _T&)>		aFunction)
		{
			if(aVector.empty())
				return;

			size_t itemsPerWork = aVector.size() / m_concurrency;
			if(itemsPerWork == 0)
				itemsPerWork = 1;

			size_t workCount = aVector.size() / itemsPerWork;
			size_t workIndex = 0;

			ForEachContext context;
			context.m_itemSize = sizeof(_T);
			context.m_function = (std::function<void(const DummyClass&)>*)&aFunction;
			context.m_itemCountPerWork = itemsPerWork;
			context.m_itemCountPerWorkLast = (itemsPerWork * workCount) % aVector.size();

			for(size_t i = 0; i < workCount; i++)
			{
				size_t remaining = aVector.size() - workIndex;
				size_t count = remaining < itemsPerWork ? remaining : itemsPerWork;
				uint32_t header = MakeHeader(TYPE_FOR_EACH_VECTOR, i + 1 == workCount ? FOR_EACH_VECTOR_FLAG_LAST : 0);

				PostPacket({ header, (void*)&aVector[workIndex], (void*)&context });

				workIndex += count;
			}

			for(;;)
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));

		}

	private:		

		size_t											m_concurrency;

		#if defined(WIN32)
			Win32Handle									m_iocpHandle;
		#endif

		struct DummyClass
		{

		};

		struct ForEachContext
		{
			size_t									m_itemSize = 0;
			size_t									m_itemCountPerWork = 0;
			size_t									m_itemCountPerWorkLast = 0;
			std::function<void(const DummyClass&)>* m_function = NULL;
		};

		WaitResult	_WaitForPacket(
						uint32_t		aMaxWaitTime,
						Packet&			aOut);


	};

}