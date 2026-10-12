#pragma once
#include <string>

namespace AtomEngine
{
	class IPanel
	{
	public:
		virtual ~IPanel() = default;
		virtual void OnAttach() = 0;
		virtual void Render() = 0;

	protected:
		std::string mTitle;
		bool mIsOpen = true;
	};
}