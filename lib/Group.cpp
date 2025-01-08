#include "Pcheader.h"

#include <nwork/Group.h>

namespace nwork
{

	Group* 
	Group::NewReferenceCounted(
		uint32_t	aFlags,
		uint32_t	aSize)
	{
		return new Group(aFlags | FLAG_REFERENCE_COUNTED, aSize);
	}

	//-------------------------------------------------------------------------------------------

	Group::Group(
		uint32_t	aFlags,
		uint32_t	aSize)
		: m_flags(aFlags)
		, m_size(aSize)
		, m_completed(0)
		, m_refCount(0)
		, m_posted(0)
		, m_event(0)
	{
		
	}
	
	Group::~Group()
	{

	}

	void	
	Group::OnCompletion()
	{
		m_completed++;

		if(m_size == m_completed)
			_OnAllCompleted();
	}

	void
	Group::OnPost()
	{
		if(!IsFixedSize())
		{
			assert(m_size == 0);
			m_posted++;
		}
	}

	void	
	Group::OnAllPosted()
	{
		assert(m_size == 0);

		m_size = m_posted;

		if(m_size == m_completed)
			_OnAllCompleted();
	}

	void	
	Group::Wait()
	{
		if(IsFixedSize() && m_size == 0)
			return;

		if(!IsFixedSize() && m_posted == 0)
			return;

		if(m_posted > 0 && m_size == 0)
			OnAllPosted();

		m_event.acquire();
	}

	void
	Group::AddReference()
	{
		assert(IsReferenceCounted());

		m_refCount++;
	}

	void
	Group::RemoveReference()
	{
		assert(IsReferenceCounted());

		if (--m_refCount == 0)
			_OnUnreferenced();
	}

	void			
	Group::SetCompletionFunction(
		std::function<void()>	aFunction)
	{
		m_completionFunction = aFunction;
	}

	//-------------------------------------------------------------------------------------------

	void
	Group::_OnAllCompleted()
	{
		if(m_completionFunction)
			m_completionFunction();

		m_event.release();
	}

	void			
	Group::_OnUnreferenced()
	{
		assert(IsReferenceCounted());

		delete this;
	}

}