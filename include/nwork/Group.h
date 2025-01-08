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

		uint32_t				m_posted;
		std::atomic_uint32_t	m_size;
		std::atomic_uint32_t	m_completed;
		std::binary_semaphore	m_event;
		std::function<void()>	m_completionFunction;

		std::atomic_uint32_t	m_refCount;

		void			_OnAllCompleted();
		void			_OnUnreferenced();
	};

}