#pragma once

namespace nwork
{

	class Group
	{
	public:
		enum Flag : uint32_t
		{
			FLAG_REFERENCE_COUNTED	= 0x00000001,
			FLAG_FIXED_SIZE			= 0x00000002
		};

		static Group*	NewReferenceCounted(
							uint32_t				aFlags = 0,
							uint32_t				aSize = 0);

						Group(
							uint32_t				aFlags = 0,
							uint32_t				aSize = 0);
						~Group();

						Group(const Group&) = delete;
		Group&			operator=(const Group&) = delete;

		void			OnCompletion();
		void			OnPost();
		void			OnAllPosted();
		void			Wait();
		void			AddReference();
		void			RemoveReference();
		void			SetCompletionFunction(
							std::function<void()>	aFunction);

		// Data access
		bool			IsReferenceCounted() const { return m_flags & FLAG_REFERENCE_COUNTED; }
		bool			IsFixedSize() const { return m_flags & FLAG_FIXED_SIZE; }

	private:

		uint32_t				m_flags;
		uint32_t				m_size;

		// Only touched by the posting thread
		uint32_t				m_posted;
		bool					m_allPosted;

		// Work that hasn't completed yet. If not fixed size, it also includes one for not having called OnAllPosted() yet,
		// so it can't reach zero before everything has been posted. Whoever brings it to zero completes the group.
		std::atomic_uint32_t	m_pending;

		std::binary_semaphore	m_event;
		std::function<void()>	m_completionFunction;

		std::atomic_uint32_t	m_refCount;

		void			_OnAllCompleted();
		void			_OnUnreferenced();
	};

}
