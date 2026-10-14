#include "EditorLayer.h"
#include "ImGuiCommon.h"
#include "EditorTheme.h"
#include "EditorLocalization.h"
#include "Runtime/Function/Framework/Component/MaterialComponent.h"
#include "Runtime/Function/Framework/Component/MeshComponent.h"
#include "imgui.h"
namespace AtomEngine
{
	void EditorLayer::SelectMaterial(const std::filesystem::path& path)
	{
		auto preset = MaterialPreset::Load(path);
		preset.Preload();
		mPreset = std::move(preset);
		strncpy_s(mMaterialPath, WStringToUTF8(path.generic_wstring()).c_str(), _TRUNCATE);
		mPreviewFromSelection = false;
		mShowMaterials = true;
	}
	void EditorLayer::ApplyMaterial(World& world, Entity entity, const std::filesystem::path& path, int slot)
	{
		MaterialPreset::Load(path).Apply(world, entity, slot);
		mHistoryPending = true;
	}
	void EditorLayer::RenderMaterialPreview(Scene& scene)
	{
		if (!mMaterialPreview || !mShowMaterials)
			return;
		Run([&]
			{
				auto material = mPreset.material;
				if (mPreviewFromSelection)
				{
					auto component = scene.GetWorld().TryGet<MaterialComponent>(mSelected);
					if (component && !component->mMaterials.empty())
						material = component->mMaterials.at(std::clamp(mPreviewSlot, 0, int(component->mMaterials.size()) - 1));
				}
				auto mesh = mPreviewFromSelection ? scene.GetWorld().TryGet<MeshComponent>(mSelected) : nullptr;
				auto source = mesh ? mesh->GetModel() : nullptr;
				int slot = source ? std::clamp(mPreviewSlot, 0, int(source->mMaterials.size()) - 1) : 0;
				mMaterialPreview->Render(material, mPreviewYaw, mPreviewPitch, source, slot);
			});
	}

} // namespace AtomEngine
