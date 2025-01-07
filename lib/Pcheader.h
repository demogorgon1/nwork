#pragma once

#include <nwork/Base.h>

#if !defined(WIN32)
	#include <sys/epoll.h>
	#include <sys/eventfd.h>

	#include <concurrentqueue.h>
#endif
