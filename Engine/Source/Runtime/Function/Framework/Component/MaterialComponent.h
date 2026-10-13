#pragma once
#include "Runtime/Resource/Model.h"

namespace AtomEngine
{
	struct MaterialComponent
	{
		MaterialComponent(const std::shared_ptr<Model>model = nullptr);


        using Material = MaterialInstance;
		std::vector<Material> mMaterials;
	};
}
