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
		, m_posted(0)
		, m_allPosted(false)
		, m_pending((aFlags & FLAG_FIXED_SIZE) != 0 ? aSize : 1)
		, m_event(0)
		, m_refCount(0)
	{
		assert(IsFixedSize() || aSize == 0);
	}

	Group::~Group()
	{

	}

	void
	Group::OnCompletion()
	{
		uint32_t pending = --m_pending;
		assert(pending != UINT32_MAX);

		if(pending == 0)
			_OnAllCompleted();
	}

	void
	Group::OnPost()
	{
		assert(!m_allPosted);

		m_posted++;

		if(IsFixedSize())
			assert(m_posted <= m_size);
		else
			m_pending++;
	}

	void
	Group::OnAllPosted()
	{
		assert(!IsFixedSize());
		assert(!m_allPosted);

		m_allPosted = true;

		if(--m_pending == 0)
			_OnAllCompleted();
	}

	void
	Group::Wait()
	{
		if(IsFixedSize() && m_size == 0)
			return;

		if(!IsFixedSize() && m_posted == 0)
			return;

		if(!IsFixedSize() && !m_allPosted)
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