#include "../EditorPanelSupport.h"
#include "../ScriptFieldEditor.h"
namespace AtomEngine
{
	void SceneSettingsPanel::Draw(EditorLayer& editor, Scene& s)
	{
		auto& w = s.GetWorld(); ImGui::Begin(EditorLocalization::Label("Scene"));
		ImGui::Text("%s%s", s.GetName().c_str(), editor.mDocument.Dirty() ? " *" : "");
		ImGui::Text("%s", editor.mScenePath);
		ImGui::Text(EditorLocalization::Text("Controller: %s"), Input::GetInstance()->GetGamepadBackendName());
		ImGui::Text(EditorLocalization::Text("%.1f FPS | %.2f ms | %zu history states"), ImGui::GetIO().Framerate,
			1000.f / std::max(1.f, ImGui::GetIO().Framerate), editor.mDocument.Size());
		ImGui::BeginDisabled(editor.mPlaying);
		std::string sceneName = s.GetName();
		if (Text("Scene name", sceneName) && !sceneName.empty())
			s.SetName(sceneName);
		ImGui::TextUnformatted(EditorLocalization::Text("Prefab path"));
		ImGui::SetNextItemWidth(-1);
		ImGui::InputText("##Prefab path", editor.mPrefabPath, sizeof(editor.mPrefabPath));
		if (ImGui::Button(EditorLocalization::Label("Save selected prefab")))
			editor.Run([&] { SceneSerializer::SavePrefab(w, editor.mSelected, UTF8ToWString(editor.mPrefabPath)); });
		ImGui::SameLine();
		if (ImGui::Button(EditorLocalization::Label("Instantiate prefab")))
			editor.Run([&] { editor.Select(w, SceneSerializer::InstantiatePrefab(w, UTF8ToWString(editor.mPrefabPath))); });
		if (ImGui::Button(EditorLocalization::Label("Refresh prefab instances")))
			editor.Run([&]
				{
					auto id = EditorActions::Id(w, editor.mSelected);
					SceneSerializer::RefreshPrefabs(w);
					editor.Select(w, EditorActions::Find(w, id));
				});
		if (w.IsValid(editor.mSelected) && w.HasComponent<PrefabInstanceComponent>(editor.mSelected))
		{
			auto overrides = SceneSerializer::PrefabOverrides(w, editor.mSelected);
			ImGui::Text(EditorLocalization::Text("Overrides on selected object: %zu"), overrides.size());
			if (!overrides.empty() && ImGui::TreeNode(EditorLocalization::Label("Override differences")))
			{
				for (const auto& change : overrides)
					ImGui::TextWrapped("%s", change.c_str());
				ImGui::TreePop();
			}
			if (ImGui::Button(EditorLocalization::Label("Apply instance to prefab")))
				editor.Run([&]
					{
						auto id = EditorActions::Id(w, editor.mSelected);
						SceneSerializer::ApplyPrefab(w, editor.mSelected);
						editor.Select(w, EditorActions::Find(w, id));
					});
			ImGui::SameLine();
			if (ImGui::Button(EditorLocalization::Label("Revert prefab instance")))
				editor.Run([&]
					{
						auto id = EditorActions::Id(w, editor.mSelected);
						SceneSerializer::RevertPrefab(w, editor.mSelected);
						editor.Select(w, EditorActions::Find(w, id));
					});
		}
		if (ImGui::CollapsingHeader(EditorLocalization::Label("Environment")))
		{
			auto& settings = w.settings;
			ImGui::Checkbox(EditorLocalization::Label("Draw sky"), &settings.skyEnabled);
			int mode = settings.skyMode;
			if (ImGui::BeginCombo(EditorLocalization::Label("Sky mode"), EditorLocalization::Text(mode == 0 ? "Procedural sky" : "Texture sky")))
			{
				for (int value = 0; value < 2; ++value)
					if (ImGui::Selectable(EditorLocalization::Text(value == 0 ? "Procedural sky" : "Texture sky"), value == mode)) settings.skyMode = value;
				ImGui::EndCombo();
			}
			if (settings.skyMode == 0)
			{
				Color3("Sky zenith", settings.skyZenith.ptr(), ImGuiColorEditFlags_Float);
				Color3("Sky horizon", settings.skyHorizon.ptr(), ImGuiColorEditFlags_Float);
				Color3("Sky ground", settings.skyGround.ptr(), ImGuiColorEditFlags_Float);
				Float("Sun angular size", &settings.skySunSize, .05f, .05f, 20, "%.2f", ImGuiSliderFlags_AlwaysClamp);
				Float("Sky sun intensity", &settings.skySunIntensity, .05f, 0, 20, "%.2f", ImGuiSliderFlags_AlwaysClamp);
				ImGui::TextWrapped("%s", EditorLocalization::Text("Sky sun follows the scene directional light"));
				ImGui::TextWrapped("%s", EditorLocalization::Text("Procedural sky supplies diffuse and specular environment lighting"));
			}
			else
			{
				AssetPath("Sky texture", settings.environment, "|.dds|.hdr|.png|.jpg|.jpeg|.tga|.bmp|.tif|.tiff|");
				ImGui::TextWrapped("%s", EditorLocalization::Text("Use a DDS cubemap or an equirectangular panorama"));
				Float("Sky rotation", &settings.skyRotation, 1, -360, 360, "%.1f", ImGuiSliderFlags_AlwaysClamp);
				if (ImGui::SmallButton(EditorLocalization::Label("Clear sky texture"))) settings.environment.clear();
				if (!w.skyTextureError.empty())
					ImGui::TextWrapped("%s", EditorLocalization::Text(w.skyTextureError.c_str()));
			}
			Float("Sky brightness", &settings.skyIntensity, .05f, 0, 20, "%.2f", ImGuiSliderFlags_AlwaysClamp);
			AssetPath("Diffuse IBL", settings.diffuseIBL, "|.dds|");
			AssetPath("Specular IBL", settings.specularIBL, "|.dds|");
			AssetPath("BRDF LUT", settings.brdfLut, "|.dds|");
			ImGui::DragFloat(EditorLocalization::Label("IBL intensity"), &settings.iblIntensity, .01f, 0, 20, "%.2f",
				ImGuiSliderFlags_AlwaysClamp);
			ImGui::DragFloat(EditorLocalization::Label("IBL mip bias"), &settings.iblBias, .05f, 0, 10, "%.2f",
				ImGuiSliderFlags_AlwaysClamp);
			ImGui::Checkbox(EditorLocalization::Label("Auto exposure"), &settings.autoExposure);
			ImGui::BeginDisabled(settings.autoExposure);
			ImGui::DragFloat(EditorLocalization::Label("Exposure"), &settings.exposure, .01f, .01f, 20, "%.2f",
				ImGuiSliderFlags_AlwaysClamp);
			ImGui::EndDisabled();
			ImGui::Checkbox(EditorLocalization::Label("FXAA"), &settings.fxaa);
		}
		ImGui::EndDisabled();
		if (!editor.mRuntimeError.empty())
			ImGui::TextWrapped("%s", editor.mRuntimeError.c_str());
		if (!editor.mError.empty())
		{
			ImGui::TextColored(EditorTheme::Color(231, 111, 111), "%s", editor.mError.c_str());
			if (ImGui::SmallButton(EditorLocalization::Label("Dismiss")))
				editor.mError.clear();
		}
		ImGui::End();
	}
}
