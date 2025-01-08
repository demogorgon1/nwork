#pragma once

namespace nwork
{

	template <typename _T>
	class Reference
	{
	public:
		Reference(
			_T*							aP = NULL)
			: m_p(aP)
		{
			if(m_p != NULL)
				m_p->AddReference();	
		}

		Reference(
			const Reference<_T>&		aCopy)
			: m_p(aCopy.m_p)
		{
			if(m_p != NULL)
				m_p->AddReference();				
		}

		Reference(
			Reference<_T>&&				aMove)
			: m_p(aMove.m_p)
		{
			aMove.m_p = NULL;
		}

		~Reference()
		{
			Release();
		}

		_T*
		operator ->()
		{
			assert(m_p != NULL);

			return m_p;
		}

		const _T*
		operator ->() const
		{
			assert(m_p != NULL);

			return (const _T*)m_p;
		}

		operator _T*()
		{
			return m_p;
		}

		operator const _T*() const
		{
			return m_p;
		}

		operator bool() const
		{
			return IsSet();
		}

		Reference<_T>&
		operator =(
			_T*		aP)
		{
			Set(aP);
			return *this;
		}

		Reference<_T>&
		operator =(
			const Reference<_T>&		aCopy)
		{
			Set(aCopy.m_p);
			return *this;
		}

		bool
		operator ==(
			const Reference<_T>&		aRHS) const
		{
			return aRHS.m_p == m_p;
		}

		bool
		operator ==(
			_T*							aRHS) const
		{
			return aRHS == m_p;
		}

		bool
		operator ==(
			int							aRHS) const
		{
			// This operator overload is supposed to catch (x == NULL) only, in case NULL is #define NULL 0.
			assert(aRHS == 0);
			return m_p == NULL;
		}

		bool
		operator !=(
			const Reference<_T>&		aRHS) const
		{
			return aRHS.m_p != m_p;
		}

		bool
		operator !=(
			_T*							aRHS) const
		{
			return aRHS != m_p;
		}

		bool
		operator !=(
			int							aRHS) const
		{
			// This operator overload is supposed to catch (x != NULL) only, in case NULL is #define NULL 0.
			assert(aRHS == 0);
			return m_p != NULL;
		}

		void
		Set(
			_T*							aP)
		{
			if(m_p == aP)
				return;

			Release();

			if(aP != NULL)
			{
				m_p = aP;

				m_p->AddReference();
			}
		}

		void
		Release()
		{
			if(m_p != NULL)
				m_p->RemoveReference();

			m_p = NULL;
		}

		bool
		IsNull() const
		{
			return m_p == NULL;
		}

		bool
		IsSet() const
		{
			return m_p != NULL;
		}

		_T*
		GetPointer()
		{
			return m_p;
		}

		const _T*
		GetPointer() const
		{
			return m_p;
		}

	private:

		_T*		m_p;
	};

}