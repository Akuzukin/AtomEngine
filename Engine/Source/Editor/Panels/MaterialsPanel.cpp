#include "../EditorPanelSupport.h"
#include "../ScriptFieldEditor.h"
namespace AtomEngine
{
	void MaterialsPanel::Draw(EditorLayer& editor, Scene& scene)
	{
		ImGui::Begin(EditorLocalization::Label("Materials"), &editor.mShowMaterials);
		if (!editor.mMaterialPreview)
			editor.Run([&]
				{
					auto preview = std::make_unique<MaterialPreviewRenderer>();
					preview->Initialize();
					editor.mMaterialPreview = std::move(preview);
				});
		ImGui::TextDisabled(EditorLocalization::Text("MATERIAL PREVIEW"));
		ImGui::Checkbox(EditorLocalization::Label("Preview selected object"), &editor.mPreviewFromSelection);
		ImGui::Separator();
		auto component = scene.GetWorld().TryGet<MaterialComponent>(editor.mSelected);
		if (editor.mPreviewFromSelection && component && component->mMaterials.size() > 1)
		{
			ImGui::SliderInt(EditorLocalization::Label("Slot"), &editor.mPreviewSlot, 0, int(component->mMaterials.size()) - 1);
		}
		if (editor.mMaterialPreview)
		{
			auto width = std::min(256.f, ImGui::GetContentRegionAvail().x);
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - width) * .5f);
			ImGuiCommon::ImageDisplayEncoded((ImTextureID)editor.mMaterialPreview->Texture(), ImVec2(width, width));
			EditorTheme::Tooltip(EditorLocalization::Text("Drag to orbit. Drop a .mat asset here to preview it."));
			if (ImGui::IsItemHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
			{
				editor.mPreviewYaw += ImGui::GetIO().MouseDelta.x * .01f;
				editor.mPreviewPitch = std::clamp(editor.mPreviewPitch - ImGui::GetIO().MouseDelta.y * .01f, -1.3f, 1.3f);
			}
			if (ImGui::BeginDragDropTarget())
			{
				if (auto payload = ImGui::AcceptDragDropPayload("ATOM_ASSET"))
				{
					auto path = std::filesystem::path(UTF8ToWString(static_cast<const char*>(payload->Data)));
					if (path.extension() == L".mat")
						editor.Run([&] { editor.SelectMaterial(path); });
				}
				ImGui::EndDragDropTarget();
			}
		}
		ImGui::TextDisabled(EditorLocalization::Text("Drag to orbit | Drop .mat to preview"));
		ImGui::Separator();
		ImGui::TextUnformatted(EditorLocalization::Text("Preset path"));
		ImGui::SetNextItemWidth(-1);
		ImGui::InputText("##Material path", editor.mMaterialPath, sizeof(editor.mMaterialPath));
		if (ImGui::Button(EditorLocalization::Label("Load preset")))
			editor.Run([&] { editor.SelectMaterial(UTF8ToWString(editor.mMaterialPath)); });
		ImGui::SameLine();
		if (ImGui::Button(EditorLocalization::Label("Browse presets")))
		{
			editor.mShowAssets = true;
			editor.mAssetDirectory = "Asset/Materials";
			editor.mRefreshAssets = true;
		}
		ImGui::BeginDisabled(editor.mPlaying || !component || component->mMaterials.empty());
		if (ImGui::Button(EditorLocalization::Label("Save selected slot as preset")))
			editor.Run([&]
				{
					auto preset = MaterialPreset::FromObject(
						scene.GetWorld(), editor.mSelected, std::clamp(editor.mPreviewSlot, 0, int(component->mMaterials.size()) - 1));
					preset.name = WStringToUTF8(std::filesystem::path(UTF8ToWString(editor.mMaterialPath)).stem().wstring());
					preset.Save(UTF8ToWString(editor.mMaterialPath));
					editor.SelectMaterial(UTF8ToWString(editor.mMaterialPath));
					editor.mRefreshAssets = true;
				});
		if (ImGui::Button(EditorLocalization::Label("Apply preset to selected slot")))
			editor.Run([&]
				{
					editor.mPreset.Apply(scene.GetWorld(), editor.mSelected,
						std::clamp(editor.mPreviewSlot, 0, int(component->mMaterials.size()) - 1));
					editor.mHistoryPending = true;
				});
		if (ImGui::Button(EditorLocalization::Label("Apply preset to all slots")))
			editor.Run([&]
				{
					editor.mPreset.Apply(scene.GetWorld(), editor.mSelected);
					editor.mHistoryPending = true;
				});
		ImGui::EndDisabled();
		ImGui::TextDisabled(EditorLocalization::Text("Preset: %s"), editor.mPreset.name.c_str());
		ImGui::End();
	}
}
