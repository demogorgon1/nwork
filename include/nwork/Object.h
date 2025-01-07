#pragma once

namespace nwork
{	

	class Object
	{
	public:
		virtual			~Object() {}

		// Virtual methods
		virtual void	ExecuteWork() {}
		virtual void	BeforePost() {}
		virtual void	AfterExecute() {}		
	};

}