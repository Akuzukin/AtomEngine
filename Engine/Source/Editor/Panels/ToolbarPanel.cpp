#include "../EditorPanelSupport.h"
#include "../ScriptFieldEditor.h"
namespace AtomEngine
{
	void ToolbarPanel::Draw(EditorLayer& editor, Scene& scene)
	{
		auto hint = [](const char* key)
			{
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled))
					ImGui::SetTooltip("%s", EditorLocalization::Text(key));
			};
		if (EditorTheme::IconButton("###SimulationPlay", editor.mPlaying ? EditorTheme::Icon::Stop : EditorTheme::Icon::Play,
			editor.mPlaying, EditorTheme::Color(46, 100, 64),
			editor.mPlaying ? EditorTheme::Gray(230) : EditorTheme::Color(112, 192, 138)))
			editor.Run([&] { editor.TogglePlay(scene); });
		hint(editor.mPlaying ? "Stop simulation and restore the editing scene" : "Run the scene");
		ImGui::SameLine();
		ImGui::BeginDisabled(!editor.mPlaying);
		if (EditorTheme::IconButton("###SimulationPause", editor.mPaused ? EditorTheme::Icon::Play : EditorTheme::Icon::Pause,
			editor.mPaused, EditorTheme::Color(112, 84, 39)))
			editor.TogglePause();
		hint(editor.mPaused ? "Resume" : "Pause");
		ImGui::SameLine();
		ImGui::BeginDisabled(!editor.mPaused);
		if (EditorTheme::IconButton("###SimulationStep", EditorTheme::Icon::Step, false, EditorTheme::Gray(51)))
			editor.StepFrame();
		hint("Step");
		ImGui::EndDisabled();
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(editor.mScriptBuild.valid());
		if (ImGui::Button(EditorLocalization::Label("Build scripts")))
			editor.Run([&] { editor.mScriptBuild = ScriptTools::Build(); });
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::Checkbox(EditorLocalization::Label("Maximize on Play"), &editor.mMaximizeOnPlay);
		ImGui::SameLine();
		bool fullscreen = WindowManager::GetInstance()->IsFullscreen();
		if (ImGui::Checkbox(EditorLocalization::Label("Fullscreen preview [F11]"), &fullscreen))
		{
			WindowManager::GetInstance()->SetFullscreen(fullscreen);
			if (fullscreen) editor.mMaximizeOnPlay = true;
		}
		ImGui::Separator();
	}
}
