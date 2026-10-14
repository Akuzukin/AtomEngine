#include "../EditorPanelSupport.h"
#include "../ScriptFieldEditor.h"
namespace AtomEngine
{
	namespace
	{
		void DockLayout(ImGuiID id)
		{
			auto viewport = ImGui::GetMainViewport();
			ImGui::DockBuilderRemoveNode(id);
			ImGui::DockBuilderAddNode(id, ImGuiDockNodeFlags_DockSpace);
			ImGui::DockBuilderSetNodeSize(id, viewport->WorkSize);
			auto middle = id;
			auto right = ImGui::DockBuilderSplitNode(middle, ImGuiDir_Right, .24f, nullptr, &middle);
			auto bottom = ImGui::DockBuilderSplitNode(middle, ImGuiDir_Down, .25f, nullptr, &middle);
			auto hierarchy = ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, .38f, nullptr, &right);
			ImGui::DockBuilderDockWindow(EditorLocalization::Label("Hierarchy"), hierarchy);
			ImGui::DockBuilderDockWindow(EditorLocalization::Label("Inspector"), right);
			ImGui::DockBuilderDockWindow(EditorLocalization::Label("Materials"), right);
			ImGui::DockBuilderDockWindow(EditorLocalization::Label("Scene"), right);
			ImGui::DockBuilderDockWindow(EditorLocalization::Label("Viewport"), middle);
			for (auto name : { "Assets", "Console", "Shaders", "Editor help" })
				ImGui::DockBuilderDockWindow(EditorLocalization::Label(name), bottom);
			ImGui::DockBuilderFinish(id);
		}
	}
	void WorkspacePanel::Draw(EditorLayer& editor, Scene& s)
	{
		if (editor.mShowPool)
		{
			ImGui::Begin(EditorLocalization::Label("Batch object pool"), &editor.mShowPool);
			ImGui::TextWrapped("%s", EditorLocalization::Text("Duplicate the selected subtree or instantiate a prefab. The entire batch is one undo step."));
			ImGui::BeginDisabled(editor.mPlaying); ImGui::Checkbox(EditorLocalization::Label("Use prefab"), &editor.mPoolUsePrefab);
			if (editor.mPoolUsePrefab)ImGui::InputText(EditorLocalization::Label("Prefab path"), editor.mPoolPrefab, sizeof(editor.mPoolPrefab)); else ImGui::TextUnformatted(EditorLocalization::Text("Source: selected object"));
			ImGui::SliderInt(EditorLocalization::Label("Count"), &editor.mPoolCount, 1, 256); ImGui::SliderInt(EditorLocalization::Label("Columns"), &editor.mPoolColumns, 1, 256); ImGui::DragFloat3("Grid spacing XYZ", editor.mPoolSpacing.ptr(), .1f); ImGui::Checkbox(EditorLocalization::Label("Start inactive"), &editor.mPoolInactive);
			if (ImGui::Button(EditorLocalization::Label("Create pool")))editor.Run([&] { editor.Capture(s); DX12Core::gCommandManager.IdleGPU(); auto objects = SceneAuthoring::CreatePool(s.GetWorld(), editor.mSelected, editor.mPoolCount, editor.mPoolColumns, editor.mPoolSpacing, editor.mPoolInactive, editor.mPoolUsePrefab ? std::filesystem::path(editor.mPoolPrefab) : std::filesystem::path{}); editor.Select(s.GetWorld(), objects.front()); editor.mDocument.Capture(s.GetWorld(), s.GetName(), EditorActions::Id(s.GetWorld(), editor.mSelected), "Create object pool"); editor.mShowPool = false; });
			ImGui::EndDisabled(); ImGui::End();
		}
		editor.Shortcuts(s);
		if (ImGui::BeginMainMenuBar())
		{
			ImGui::TextColored(EditorTheme::Accent(), "ATOM");
			ImGui::Separator();
			if (EditorTheme::IconButton("##SaveDocument", EditorTheme::Icon::Save, false, EditorTheme::Gray(45)))editor.SaveDocument(s);
			EditorTheme::Tooltip(EditorLocalization::Text("Save")); ImGui::SameLine();
			ImGui::BeginDisabled(editor.mPlaying);
			if (EditorTheme::IconButton("##UndoDocument", EditorTheme::Icon::Undo, false, EditorTheme::Gray(45)))editor.Undo(s);
			EditorTheme::Tooltip(EditorLocalization::Text("Undo")); ImGui::SameLine();
			if (EditorTheme::IconButton("##RedoDocument", EditorTheme::Icon::Redo, false, EditorTheme::Gray(45)))editor.Undo(s, true);
			EditorTheme::Tooltip(EditorLocalization::Text("Redo")); ImGui::EndDisabled(); ImGui::SameLine();
			if (EditorTheme::IconButton("##ProjectSettings", EditorTheme::Icon::Settings, false, EditorTheme::Gray(45)))editor.mProjectSettingsPanel.visible = true;
			EditorTheme::Tooltip(EditorLocalization::Text("Project settings")); ImGui::SameLine();
			if (ImGui::BeginMenu(EditorLocalization::Label("File")))
			{
                ImGui::BeginDisabled(editor.mPlaying || editor.mProjectPanel.Busy());
                if (ImGui::MenuItem(EditorLocalization::Label("New project..."))) { auto path = ProjectPanel::Dialog(true); if (!path.empty()) { editor.mPending = EditorLayer::Pending::NewProject; editor.mPendingPath = path; } }
                if (ImGui::MenuItem(EditorLocalization::Label("Open project..."))) { auto path = ProjectPanel::Dialog(false); if (!path.empty()) { editor.mPending = EditorLayer::Pending::OpenProject; editor.mPendingPath = path; } }
                if (ImGui::MenuItem(EditorLocalization::Label("Save project"), "Ctrl+Alt+S")) editor.mProjectPanel.Save(editor, s);
                ImGui::EndDisabled(); ImGui::Separator();
				if (ImGui::MenuItem(EditorLocalization::Label("New scene"), "Ctrl+N", false, !editor.mPlaying))
					editor.mPending = EditorLayer::Pending::New;
				if (ImGui::BeginMenu(EditorLocalization::Label("New from template"), !editor.mPlaying))
				{
					std::error_code ec; auto directory = std::filesystem::path("Asset/Templates");
					for (auto& entry : std::filesystem::directory_iterator(directory, ec))if (entry.path().extension() == ".scene")if (ImGui::MenuItem(PathText(entry.path().stem()).c_str())) { editor.mPending = EditorLayer::Pending::Template; editor.mPendingPath = entry.path(); }
					ImGui::EndMenu();
				}
				if (ImGui::MenuItem(EditorLocalization::Label("Open scene..."), "Ctrl+O", false, !editor.mPlaying))
				{
					auto path = SceneDialog(false, editor.mScenePath);
					if (!path.empty())
						editor.OpenScene(path);
				}
				if (ImGui::BeginMenu(EditorLocalization::Label("Recent scenes"), !editor.mPlaying))
				{
					for (auto& p : editor.mRecentScenes)
						if (ImGui::MenuItem(PathText(p).c_str()))
							editor.OpenScene(p);
					ImGui::EndMenu();
				}
				if (ImGui::MenuItem(EditorLocalization::Label("Save"), "Ctrl+S", false, !editor.mPlaying))
					editor.SaveDocument(s);
				if (ImGui::MenuItem(EditorLocalization::Label("Save As..."), "Ctrl+Shift+S", false, !editor.mPlaying))
					editor.SaveDocument(s, true);
				ImGui::Separator();
				if (ImGui::MenuItem(EditorLocalization::Label("Exit")))
					editor.RequestClose();
				ImGui::EndMenu();
			}
            if (ImGui::BeginMenu(EditorLocalization::Label("Project"))) {
                ImGui::MenuItem(EditorLocalization::Label("Project scenes"), nullptr, &editor.mProjectPanel.visible);
                ImGui::BeginDisabled(editor.mPlaying || editor.mProjectPanel.Busy());
                if (ImGui::MenuItem(EditorLocalization::Label("Build game"))) editor.mProjectPanel.StartBuild(editor, s, false);
                if (ImGui::MenuItem(EditorLocalization::Label("Build and publish"))) editor.mProjectPanel.StartBuild(editor, s, true);
                ImGui::EndDisabled(); ImGui::EndMenu();
            }
			if (ImGui::BeginMenu(EditorLocalization::Label("Edit")))
			{
				if (ImGui::MenuItem(EditorLocalization::Label("Undo"), "Ctrl+Z", false, !editor.mPlaying && editor.mDocument.CanUndo()))
					editor.Undo(s);
				if (ImGui::MenuItem(EditorLocalization::Label("Redo"), "Ctrl+Y", false, !editor.mPlaying && editor.mDocument.CanRedo()))
					editor.Undo(s, true);
				if (ImGui::MenuItem("Batch object pool...", nullptr, false, !editor.mPlaying))editor.mShowPool = true;
				if (ImGui::MenuItem(EditorLocalization::Label("Duplicate subtree"), "Ctrl+D", false,
					!editor.mPlaying && s.GetWorld().IsValid(editor.mSelected)))
					editor.Run([&] { editor.Duplicate(s.GetWorld()); });
				if (ImGui::MenuItem(EditorLocalization::Label("Frame selected"), "F", false,
					s.GetWorld().IsValid(editor.mSelected)))
					editor.Focus(s);
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu(EditorLocalization::Label("View")))
			{
				ImGui::MenuItem(EditorLocalization::Label("Assets"), nullptr, &editor.mShowAssets);
				ImGui::MenuItem(EditorLocalization::Label("Materials"), nullptr, &editor.mShowMaterials);
				ImGui::MenuItem(EditorLocalization::Label("Console"), nullptr, &editor.mShowConsole);
				ImGui::MenuItem(EditorLocalization::Label("Shaders"), nullptr, &editor.mShowShaders);
				ImGui::MenuItem(EditorLocalization::Label("Camera frustums"), nullptr, &editor.mShowCameraGizmos);
				ImGui::MenuItem(EditorLocalization::Label("Grid"), nullptr, &gContext.showGrid);
				ImGui::MenuItem(EditorLocalization::Label("Colliders"), nullptr, &gContext.showColliders);
				ImGui::MenuItem(EditorLocalization::Label("Render diagnostics"), nullptr, &gContext.showRenderDiagnostics);
				if (ImGui::MenuItem(EditorLocalization::Label("Reset layout")))
					editor.mResetLayout = true;
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu(EditorLocalization::Label("Settings")))
			{
				ImGui::MenuItem(EditorLocalization::Label("Project settings"), nullptr, &editor.mProjectSettingsPanel.visible);
				ImGui::EndMenu();
			}
			if (ImGui::MenuItem(EditorLocalization::Label("Help")))
				editor.mShowHelp = !editor.mShowHelp;
			if (ImGui::BeginMenu(EditorLocalization::Label("Language")))
			{
				for (const auto& language : EditorLocalization::Languages())
					if (ImGui::MenuItem(language.name.c_str(), nullptr,
						language.id == EditorLocalization::CurrentLanguage()))
					{
						EditorLocalization::SetLanguage(language.id);
						editor.SavePreferences();
					}
				ImGui::EndMenu();
			}
			ImGui::Separator();
			ImGui::TextDisabled("%s%s", s.GetName().c_str(), editor.mDocument.Dirty() ? " *" : "");
			ImGui::EndMainMenuBar();
		}
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 3));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, EditorTheme::Gray(26));
		if (ImGui::BeginViewportSideBar("##Editor status", ImGui::GetMainViewport(), ImGuiDir_Down, ImGui::GetFrameHeight(),
			ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoScrollWithMouse))
		{
			ImGui::TextColored(editor.mPlaying ? (editor.mPaused ? EditorTheme::Color(217, 172, 87) : EditorTheme::Color(112, 192, 138))
				: EditorTheme::Accent(),
				"%s", EditorLocalization::Text(editor.mPlaying ? (editor.mPaused ? "PAUSED" : "PLAYING") : "EDITOR"));
			ImGui::SameLine();
			ImGui::TextDisabled(" | %s",
				EditorLocalization::Text(editor.mDocument.Dirty() ? "Unsaved changes" : "All changes saved"));
			if (ImGui::GetContentRegionAvail().x > 300)
			{
				ImGui::SameLine();
				auto name = s.GetWorld().TryGet<NameComponent>(editor.mSelected);
				ImGui::TextDisabled(" | %s", name ? name->value.c_str() : EditorLocalization::Text("No selection"));
			}
			const float fpsWidth = ImGui::CalcTextSize("9999 FPS | 999.9 ms").x;
			if (ImGui::GetContentRegionAvail().x > fpsWidth + 20)
			{
				ImGui::SameLine(ImGui::GetWindowWidth() - fpsWidth - 12);
				auto fps = ImGui::GetIO().Framerate;
				ImGui::TextDisabled("%.0f FPS | %.1f ms", fps, fps > 0 ? 1000.f / fps : 0.f);
			}
		}
		ImGui::End();
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();
		ImGuiWindowClass docking;
		docking.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
		auto dock = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_None, &docking);
		auto node = ImGui::DockBuilderGetNode(dock);

		if (editor.mResetLayout || (node && !node->IsSplitNode() && node->Windows.Size == 0))
		{
			DockLayout(dock);
			editor.mResetLayout = false;
        EditorDefaultPanelFocusFrame = ImGui::GetFrameCount() + 5;
		}

	}
}
