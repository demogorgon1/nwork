#pragma once

#if defined(WIN32)
	#include <windows.h>
#else
	#include <unistd.h>
#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <chrono>
#include <functional>
#include <semaphore>
#include <thread>

namespace nwork
{

	#if defined(WIN32)

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
				_Close();
			}

			Win32Handle(const Win32Handle&) = delete;
			Win32Handle& operator=(const Win32Handle&) = delete;

			Win32Handle&
			operator=(
				HANDLE				aHandle)
			{
				if (aHandle != m_handle)
				{
					_Close();
					m_handle = aHandle;
				}
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

			void
			_Close()
			{
				if (m_handle != NULL && m_handle != INVALID_HANDLE_VALUE)
					CloseHandle(m_handle);
			}
		};

		inline size_t 
		GetCPUCount()
		{
			SYSTEM_INFO si;
			GetSystemInfo(&si);
			return (size_t)si.dwNumberOfProcessors;
		}

	#else
	
		inline size_t 
		GetCPUCount()
		{
			return (size_t)sysconf(_SC_NPROCESSORS_ONLN);
		}

	#endif

}