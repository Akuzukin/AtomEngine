#include "MaterialComponent.h"

AtomEngine::MaterialComponent::MaterialComponent(const std::shared_ptr<Model> model)
{
	if (model)
	{
		mMaterials.resize(model->mMaterials.size());
		for (uint32_t i = 0; i < model->mMaterials.size(); i++)
		{
			auto& material = model->mMaterials[i];
			mMaterials[i].mBaseColor = material.baseColorFactor;
			mMaterials[i].mMetallic = material.metallicFactor;
            mMaterials[i].mRoughness = material.roughnessFactor;
			mMaterials[i].mEmissive = material.emissiveFactor;
			mMaterials[i].mNormalScale = material.normalScale;
            mMaterials[i].mAO=material.ao;
		}
	}
	else
	{
		mMaterials.resize(1,Material());
	}
}
