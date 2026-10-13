#include "EditorLayer.h"
#include "Runtime/Function/Framework/Component/MaterialComponent.h"
#include "Runtime/Function/Framework/Component/MeshComponent.h"
#include "imgui.h"
namespace AtomEngine
{
void EditorLayer::SelectMaterial(const std::filesystem::path &path)
{
    auto preset = MaterialPreset::Load(path);
    preset.Preload();
    mPreset = std::move(preset);
    strncpy_s(mMaterialPath, WStringToUTF8(path.generic_wstring()).c_str(), _TRUNCATE);
    mPreviewFromSelection = false;
    mShowMaterials = true;
}
void EditorLayer::ApplyMaterial(World &world, Entity entity, const std::filesystem::path &path, int slot)
{
    MaterialPreset::Load(path).Apply(world, entity, slot);
    mHistoryPending = true;
}
void EditorLayer::RenderMaterialPreview(Scene &scene)
{
    if (!mMaterialPreview || !mShowMaterials)
        return;
    Run([&] {
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
void EditorLayer::Materials(Scene &scene)
{
    ImGui::Begin("Materials", &mShowMaterials);
    if (!mMaterialPreview)
        Run([&] {
            auto preview = std::make_unique<MaterialPreviewRenderer>();
            preview->Initialize();
            mMaterialPreview = std::move(preview);
        });
    ImGui::Checkbox("Preview selected object", &mPreviewFromSelection);
    auto component = scene.GetWorld().TryGet<MaterialComponent>(mSelected);
    if (mPreviewFromSelection && component && component->mMaterials.size() > 1)
    {
        ImGui::SliderInt("Slot", &mPreviewSlot, 0, int(component->mMaterials.size()) - 1);
    }
    if (mMaterialPreview)
    {
        auto width = std::min(256.f, ImGui::GetContentRegionAvail().x);
        ImGui::Image((ImTextureID)mMaterialPreview->Texture(), ImVec2(width, width));
        if (ImGui::IsItemHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            mPreviewYaw += ImGui::GetIO().MouseDelta.x * .01f;
            mPreviewPitch = std::clamp(mPreviewPitch - ImGui::GetIO().MouseDelta.y * .01f, -1.3f, 1.3f);
        }
        if (ImGui::BeginDragDropTarget())
        {
            if (auto payload = ImGui::AcceptDragDropPayload("ATOM_ASSET"))
            {
                auto path = std::filesystem::path(UTF8ToWString(static_cast<const char *>(payload->Data)));
                if (path.extension() == L".mat")
                    Run([&] { SelectMaterial(path); });
            }
            ImGui::EndDragDropTarget();
        }
    }
    ImGui::TextWrapped("Drag preview to orbit. Drag .mat onto a mesh or a material slot.");
    ImGui::TextUnformatted("Preset path");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##Material path", mMaterialPath, sizeof(mMaterialPath));
    if (ImGui::Button("Load preset"))
        Run([&] { SelectMaterial(UTF8ToWString(mMaterialPath)); });
    ImGui::SameLine();
    if (ImGui::Button("Browse presets"))
    {
        mShowAssets = true;
        mAssetDirectory = "Asset/Materials";
        mRefreshAssets = true;
    }
    ImGui::BeginDisabled(mPlaying || !component || component->mMaterials.empty());
    if (ImGui::Button("Save selected slot as preset"))
        Run([&] {
            auto preset = MaterialPreset::FromObject(
                scene.GetWorld(), mSelected, std::clamp(mPreviewSlot, 0, int(component->mMaterials.size()) - 1));
            preset.name = WStringToUTF8(std::filesystem::path(UTF8ToWString(mMaterialPath)).stem().wstring());
            preset.Save(UTF8ToWString(mMaterialPath));
            SelectMaterial(UTF8ToWString(mMaterialPath));
            mRefreshAssets = true;
        });
    if (ImGui::Button("Apply preset to selected slot"))
        Run([&] {
            mPreset.Apply(scene.GetWorld(), mSelected,
                          std::clamp(mPreviewSlot, 0, int(component->mMaterials.size()) - 1));
            mHistoryPending = true;
        });
    if (ImGui::Button("Apply preset to all slots"))
        Run([&] {
            mPreset.Apply(scene.GetWorld(), mSelected);
            mHistoryPending = true;
        });
    ImGui::EndDisabled();
    ImGui::Text("Preset: %s", mPreset.name.c_str());
    ImGui::End();
}
} // namespace AtomEngine
