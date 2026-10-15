#include "../EditorPanelSupport.h"
#include "../ScriptFieldEditor.h"
namespace AtomEngine
{
	void ViewportPanel::Draw(EditorLayer& editor, Scene& s)
	{
        if (ImGui::GetFrameCount() == EditorDefaultPanelFocusFrame) ImGui::SetNextWindowFocus();
		ImGui::Begin(EditorLocalization::Label("Viewport"));
		s.GetCamera().SetAspectRatio(float(gOverlayBuffer.GetHeight()) / std::max(1u, gOverlayBuffer.GetWidth()));
		editor.mViewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		editor.PlayToolbar(s);
		const char* names[] = { "Move [T]", "Rotate [R]", "Scale [S]" };
		for (int i = 0; i < 3; ++i)
		{
			if (i)
				ImGui::SameLine();
			auto id = std::string("##") + names[i];
			if (EditorTheme::IconButton(id.c_str(), i == 0 ? EditorTheme::Icon::Move : i == 1 ? EditorTheme::Icon::Rotate : EditorTheme::Icon::Scale, editor.mGizmoOperation == i + 1, EditorTheme::Accent()))
				editor.mGizmoOperation = i + 1;
			EditorTheme::Tooltip(EditorLocalization::Text(i == 0 ? "Translate selected object (T)"
				: i == 1 ? "Rotate selected object (R)"
				: "Scale selected object (S)"));
		}
		ImGui::Checkbox(EditorLocalization::Label("Local"), &editor.mLocal);
		ImGui::SameLine();
		ImGui::Checkbox(EditorLocalization::Label("Snap"), &editor.mSnap);
		ImGui::SameLine();
		float* snapStep = editor.mGizmoOperation == 2 ? &editor.mRotateSnap : editor.mGizmoOperation == 3 ? &editor.mScaleSnap : &editor.mMoveSnap;
		ImGui::SetNextItemWidth(70);
		ImGui::DragFloat(EditorLocalization::Label("Step##TransformSnapStep"), snapStep, .05f, .001f, 100, "%.2f",
			ImGuiSliderFlags_AlwaysClamp);
		ImGui::Separator();
		auto available = ImGui::GetContentRegionAvail();
		float aspect = float(gOverlayBuffer.GetWidth()) / std::max(1u, gOverlayBuffer.GetHeight());
		ImVec2 size(available.x, available.x / aspect);
		if (size.y > available.y)
			size = ImVec2(available.y * aspect, available.y);
		editor.mViewportHovered = false;
		editor.mGizmoActive = false;
		if (editor.mViewportTexture && size.x > 1 && size.y > 1)
		{
			auto cursor = ImGui::GetCursorScreenPos();
			cursor.x += (available.x - size.x) * .5f;
			cursor.y += (available.y - size.y) * .5f;
			ImGui::SetCursorScreenPos(cursor);
			ImGuiCommon::ImageDisplayEncoded((ImTextureID)editor.mViewportTexture, size);
			editor.mViewportHovered = ImGui::IsItemHovered();
			auto mouse = ImGui::GetMousePos();
			s.GetWorld().uiViewportSize = Vector2(size.x, size.y); s.GetWorld().uiRenderSize = Vector2(float(gOverlayBuffer.GetWidth()), float(gOverlayBuffer.GetHeight()));
			auto scenePointer = Vector2((mouse.x - cursor.x) * gSceneColorBuffer.GetWidth() / size.x,
                (mouse.y - cursor.y) * gSceneColorBuffer.GetHeight() / size.y);
            editor.mRuntimePointer = Vector2((mouse.x - cursor.x) * gOverlayBuffer.GetWidth() / size.x,
                (mouse.y - cursor.y) * gOverlayBuffer.GetHeight() / size.y);
			if (!editor.mPlaying && ImGui::BeginDragDropTarget())
			{
				if (auto payload = ImGui::AcceptDragDropPayload("ATOM_ASSET"))
				{
					auto path = std::string(static_cast<const char*>(payload->Data));
					auto ext = Extension(UTF8ToWString(path));
					if (ext == ".mat")
						editor.Run([&]
							{
								auto target = SelectionRenderer::Pick(s.GetWorld(), s.GetCamera(), scenePointer.x,
                                    scenePointer.y, float(gSceneColorBuffer.GetWidth()),
									float(gSceneColorBuffer.GetHeight()));
								editor.ApplyMaterial(s.GetWorld(), target, UTF8ToWString(path));
								editor.Select(s.GetWorld(), target);
							});
					else if (ext == ".scene")
						editor.OpenScene(UTF8ToWString(path));
					else if (ext == ".prefab")
						editor.Run([&]
							{
								editor.Select(s.GetWorld(), SceneSerializer::InstantiatePrefab(s.GetWorld(), UTF8ToWString(path)));
							});
					else if (ModelFile(UTF8ToWString(path)))
						editor.Run([&]
							{
								auto model = AssetManager::LoadModel(path);
								if (!model)
									throw std::runtime_error("Cannot load model: " + path);
								auto o =
									s.GetWorld().CreateGameObject(PathText(std::filesystem::path(UTF8ToWString(path)).stem()));
								o->AddComponent<TransformComponent>();
								o->AddComponent<MeshSourceComponent>(BuiltinMesh::None, path);
								o->AddComponent<MaterialComponent>(model);
								o->AddComponent<MeshComponent>(model);
								editor.Select(s.GetWorld(), o->GetHandle());
							});
				}
				ImGui::EndDragDropTarget();
			}
			ImGuizmo::SetDrawlist();
			ImGuizmo::SetOrthographic(false);
			ImGuizmo::SetRect(cursor.x, cursor.y, size.x, size.y);
			bool cameraGesture = ImGui::GetIO().KeyAlt || ImGui::GetIO().KeyShift ||
				ImGui::IsMouseDown(ImGuiMouseButton_Middle) || ImGui::IsMouseDown(ImGuiMouseButton_Right);
			ImGuizmo::Enable(!editor.mPlaying && !editor.mModal && editor.mGizmoOperation != 0 && !cameraGesture);
			auto t = s.GetWorld().TryGet<TransformComponent>(editor.mSelected);
			if (t && !editor.mPlaying && editor.mGizmoOperation != 0)
			{
				auto view = s.GetCamera().GetViewMatrix();
				auto camera = s.GetCamera();
				camera.ReverseZ(false);
				auto projection = camera.GetProjMatrix();
				auto matrix = t->GetMatrix();
				float snap[3] = { *snapStep, *snapStep, *snapStep };
				auto op = editor.mGizmoOperation == 1 ? ImGuizmo::TRANSLATE
					: editor.mGizmoOperation == 2 ? ImGuizmo::ROTATE
					: ImGuizmo::SCALE;
				ImGui::GetWindowDrawList()->PushClipRect(cursor, ImVec2(cursor.x + size.x, cursor.y + size.y), true);
				if (ImGuizmo::Manipulate(&view[0][0], &projection[0][0], op, editor.mLocal ? ImGuizmo::LOCAL : ImGuizmo::WORLD,
					&matrix[0][0], nullptr, editor.mSnap ? snap : nullptr))
					editor.Run([&] { EditorActions::SetWorldMatrix(s.GetWorld(), editor.mSelected, matrix); });
				editor.mGizmoActive = !cameraGesture && (ImGuizmo::IsUsing() || ImGuizmo::IsOver());
				ImGui::GetWindowDrawList()->PopClipRect();
			}
			if (editor.mViewportHovered && !editor.mGizmoActive && !editor.mPlaying && !editor.mModal && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
				!ImGui::GetIO().KeyAlt)
				editor.Select(s.GetWorld(),
					SelectionRenderer::Pick(s.GetWorld(), s.GetCamera(), scenePointer.x, scenePointer.y,
						float(gSceneColorBuffer.GetWidth()), float(gSceneColorBuffer.GetHeight())));
		}
		ImGui::End();
	}
	void ViewportPanel::DrawPlay(EditorLayer& editor, Scene& scene)
	{
		auto viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::Begin(EditorLocalization::Label("Play view"), nullptr,
			ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoMove);
		editor.mViewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		editor.PlayToolbar(scene);
		auto cursor = ImGui::GetCursorScreenPos(), size = ImGui::GetContentRegionAvail();
		editor.mViewportHovered = false;
		editor.mGizmoActive = false;
		if (editor.mViewportTexture && size.x > 1 && size.y > 1)
		{
			// Layout and input use the UI composite buffer, which can differ from scene resolution.
            scene.GetWorld().uiViewportSize=Vector2(size.x,size.y);
            scene.GetWorld().uiRenderSize=Vector2(float(gOverlayBuffer.GetWidth()),float(gOverlayBuffer.GetHeight()));
			scene.GetCamera().SetAspectRatio(size.y / size.x);
			ImGuiCommon::ImageDisplayEncoded((ImTextureID)editor.mViewportTexture, size);
			editor.mViewportHovered = ImGui::IsItemHovered();
			auto mouse = ImGui::GetMousePos();
			editor.mRuntimePointer = Vector2((mouse.x - cursor.x) * gOverlayBuffer.GetWidth() / size.x,
                (mouse.y - cursor.y) * gOverlayBuffer.GetHeight() / size.y);
		}
		ImGui::End();
	}
}
