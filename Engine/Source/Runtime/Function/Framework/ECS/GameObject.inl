#pragma once
#include "World.h"

namespace AtomEngine
{
	template<typename T, typename... Args>
	inline void GameObject::AddComponent(Args&&... args)
	{
		assert(IsValid());
		mWorld->AddComponent<T>(mHandle, std::forward<Args>(args)...);
	}

	template<typename T>
	inline bool GameObject::HasComponent() const
	{
		return IsValid() && mWorld->HasComponent<T>(mHandle);
	}

	template<typename T>
	inline T& GameObject::GetComponent()
	{
		return mWorld->GetComponent<T>(mHandle);
	}

	template<typename T>
	inline void GameObject::RemoveComponent()
	{
		mWorld->RemoveComponent<T>(mHandle);
	}
}