#pragma once

namespace nwork
{

	class Group;
	class Object;
	
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
			TYPE_FOR_EACH_VECTOR			= 0x00000000,
			TYPE_FOR_EACH_IN_RANGE			= 0x10000000,
			TYPE_FUNCTION					= 0x20000000,
			TYPE_OBJECT						= 0x30000000
		};

		enum ForEachVectorFlag : uint32_t
		{
			FOR_EACH_VECTOR_FLAG_REMAINDER	= 0x40000000
		};

		enum FunctionFlag : uint32_t
		{
			FUNCTION_FLAG_DELETE			= 0x40000000,
			FUNCTION_FLAG_GROUP				= 0x80000000
		};

		struct Packet
		{
			uint32_t		m_header = 0;
			void*			m_pointer1 = NULL;
			void*			m_pointer2 = NULL;
		};
		
		typedef std::function<void(uint32_t, void*)> IOFunction;

		static uint32_t
		MakeHeader(
			Type															aType,
			uint32_t														aFlags,
			uint32_t														aSize = UINT32_MAX)
		{
			if(aSize != UINT32_MAX)
			{
				assert(aSize < 0x0FFFFFFF);
				return aSize;
			}
			return 0x0FFFFFFF | aType | aFlags;
		}


								Queue();
								~Queue();

		void					SetForEachConcurrency(
									size_t									aForEachConcurrency);
		void					SetIOFunction(
									IOFunction								aIOFunction);
		void					PostPacket(
									const Packet&							aPacket);
		WaitResult				WaitAndExecute(
									uint32_t								aMaxWaitTime);
		void					ForEachInRange(
									int32_t									aMin,
									int32_t									aMax,
									std::function<void(int32_t)>			aFunction);
		void					PostFunction(
									std::function<void()>					aFunction);
		void					PostFunctionWithSemaphore(
									std::counting_semaphore<>*				aSemaphore,
									std::function<void()>					aFunction);
		void					PostFunctionWithGroup(
									Group*									aGroup,
									std::function<void()>					aFunction);
		void					PostFunctionPointer(
									std::function<void()>*					aFunction);
		void					PostFunctionPointerWithSemaphore(
									std::counting_semaphore<>*				aSemaphore,
									std::function<void()>*					aFunction);
		void					PostObject(
									Object*									aObject);

		template <typename _T>
		void
		ForEachVector(
			const std::vector<_T>&											aVector,
			std::function<void(const _T&)>									aFunction)
		{
			if(aVector.empty())
				return;

			ForEachVectorContext context;
			context.m_itemSize = sizeof(_T);
			context.m_function = (std::function<void(const DummyClass&)>*)&aFunction;
			_ForEachVector(&context, &aVector[0], aVector.size());
		}

		template <typename _T>
		void
		ForEachVector(
			std::vector<_T>&												aVector,
			std::function<void(_T&)>										aFunction)
		{
			if(aVector.empty())
				return;

			ForEachVectorContext context;
			context.m_itemSize = sizeof(_T);
			context.m_function = (std::function<void(const DummyClass&)>*)&aFunction;
			_ForEachVector(&context, &aVector[0], aVector.size());
		}

		// Data access
		#if defined(WIN32)
			HANDLE	GetIOCPHandle() { return m_iocpHandle; }
		#else
			int		GetEpollFd() { return m_epollFd; }
		#endif

	private:		

		size_t											m_forEachConcurrency = 1;
		IOFunction										m_ioFunction;

		#if defined(WIN32)
			Win32Handle									m_iocpHandle;
		#else	
			int											m_eventFd = 0;
			int											m_epollFd = 0;

			struct Internal;
			Internal*									m_internal = NULL;
		#endif

		struct DummyClass
		{

		};

		struct ForEachVectorContext
		{
			size_t										m_itemSize = 0;
			size_t										m_itemCountPerWork = 0;
			size_t										m_itemCountPerWorkRemainder = 0;
			std::function<void(const DummyClass&)>*		m_function = NULL;
			std::counting_semaphore<>*					m_semaphore = NULL;
		};

		struct ForEachInRangeContext
		{
			std::function<void(int32_t)>*				m_function = NULL;
			std::counting_semaphore<>*					m_semaphore = NULL;
		};

		WaitResult	_WaitForPacket(
						uint32_t				aMaxWaitTime,
						Packet&					aOut);
		void		_ForEachVector(
						ForEachVectorContext*	aContext,
						void*					aVectorBase,
						size_t					aVectorSize);


	};

}