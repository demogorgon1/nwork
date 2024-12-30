#pragma once

namespace nwork
{	

	struct Work
	{
		typedef void (*FunctionType)();

		FunctionType	m_function = NULL;
	};

}