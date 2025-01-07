#pragma once

#if defined(WIN32)
	#include <windows.h>
#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <chrono>
#include <functional>
#include <semaphore>
#include <thread>

#if defined(WIN32)

	namespace nwork
	{

		class Win32Handle
		{
		public:
			Win32Handle(
				HANDLE				aHandle = NULL)
				: m_handle(aHandle)
			{

			}

			~Win32Handle()
			{
				if (m_handle != NULL)
					CloseHandle(m_handle);
			}

			Win32Handle&
			operator=(
				HANDLE				aHandle)
			{
				m_handle = aHandle;
				return *this;
			}

			bool
			operator==(
				HANDLE				aHandle) const
			{
				return m_handle == aHandle;
			}

			operator HANDLE() const
			{
				return m_handle;
			}

			operator bool() const
			{
				return m_handle != NULL && m_handle != INVALID_HANDLE_VALUE;
			}

		private:

			HANDLE	m_handle;
		};

	}


#endif