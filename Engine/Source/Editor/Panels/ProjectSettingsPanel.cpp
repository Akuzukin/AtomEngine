#include "../EditorPanelSupport.h"
namespace AtomEngine
{
	void ProjectSettingsPanel::Draw(EditorLayer& editor, Scene& scene)
	{
		if (!visible)return;
		if (!loaded) { try { draft = ActiveProject.project ? ActiveProject.project->settings : ProjectSettings::Load(); loaded = true; } catch (const std::exception& e) { error = e.what(); loaded = true; } }
		ImGui::SetNextWindowSize(ImVec2(780, 580), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin(EditorLocalization::Label("Project settings"), &visible)) { ImGui::End(); return; }
		ImGui::BeginChild("Categories", ImVec2(170, -100), true);
		const char* categories[] = { "General","Rendering","Editor" }; for (int i = 0; i < 3; ++i)if (ImGui::Selectable(EditorLocalization::Text(categories[i]), category == i))category = i;
		ImGui::EndChild(); ImGui::SameLine(); ImGui::BeginChild("Settings", ImVec2(0, -100), true);
		auto& s = draft.defaults;
		if (category == 0) { if (ActiveProject.project) { ImGui::TextUnformatted(ActiveProject.project->settings.name.c_str()); ImGui::TextUnformatted(ActiveProject.project->settings.startupScene.c_str()); } else { Text("Project name", draft.name); AssetPath("Startup scene", draft.startupScene, "|.scene|"); } ImGui::TextWrapped("%s", EditorLocalization::Text("Startup scene changes take effect after restarting the editor")); }
		if (category == 1)
		{
			ImGui::TextWrapped("%s", EditorLocalization::Text("Project defaults apply to new scenes; saved scenes retain their overrides")); ImGui::Checkbox(EditorLocalization::Label("FXAA"), &s.fxaa); ImGui::Checkbox(EditorLocalization::Label("Auto exposure"), &s.autoExposure); ImGui::BeginDisabled(s.autoExposure); Float("Exposure", &s.exposure, .01f, .01f, 20, "%.2f", ImGuiSliderFlags_AlwaysClamp); ImGui::EndDisabled(); Float("IBL intensity", &s.iblIntensity, .05f, 0, 20, "%.2f", ImGuiSliderFlags_AlwaysClamp);
			EditorLocalization::Combo("Sky mode", &s.skyMode, "Procedural sky\0Texture sky\0"); AssetPath("Sky texture", s.environment, "|.dds|.hdr|.png|.jpg|.jpeg|"); Color3("Sky zenith", s.skyZenith.ptr()); Color3("Sky horizon", s.skyHorizon.ptr()); Color3("Sky ground", s.skyGround.ptr()); Float("Sky brightness", &s.skyIntensity, .05f, 0, 20, "%.2f", ImGuiSliderFlags_AlwaysClamp);
		}
		if (category == 2) { ImGui::Checkbox(EditorLocalization::Label("Maximize on Play"), &draft.maximizeOnPlay); ImGui::TextWrapped("%s", EditorLocalization::Text("Language and panel visibility are local workspace preferences")); }
		ImGui::EndChild(); ImGui::Separator();
		if (ImGui::Button(EditorLocalization::Label("Save project defaults"))) { try { if (ActiveProject.project) { auto saved = *ActiveProject.project; saved.settings.defaults = draft.defaults; saved.settings.maximizeOnPlay = draft.maximizeOnPlay; saved.Save(); ActiveProject.project = saved; ActiveProject.dirty = false; } else draft.Save(); ProjectSceneDefaults = draft.defaults; editor.SetMaximizeOnPlay(draft.maximizeOnPlay); error.clear(); } catch (const std::exception& e) { error = e.what(); } }
		ImGui::SameLine(); if (ImGui::Button(EditorLocalization::Label("Discard changes"))) { loaded = false; error.clear(); }
		ImGui::SameLine(); if (ImGui::Button(EditorLocalization::Label("Restore defaults")))draft = ProjectSettings{};
		ImGui::BeginDisabled(editor.IsPlaying()); if (ImGui::Button(EditorLocalization::Label("Apply defaults to current scene")))
		{
			editor.Capture(scene, "Apply project defaults"); 
			auto previous = scene.GetWorld().settings;
			auto applied = draft.defaults;
			applied.hasEditorCamera = previous.hasEditorCamera;
			applied.editorPosition = previous.editorPosition;
			applied.editorRotation = previous.editorRotation;
			applied.editorFov = previous.editorFov;
			applied.editorNear = previous.editorNear;
			applied.editorFar = previous.editorFar;
			scene.GetWorld().settings = applied; editor.Capture(scene, "Apply project defaults");
		}ImGui::EndDisabled();
		if (!error.empty())ImGui::TextWrapped("%s", error.c_str()); ImGui::End();
	}
}
