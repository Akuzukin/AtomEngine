#include "../EditorPanelSupport.h"
#include "../ScriptFieldEditor.h"
namespace AtomEngine
{
	void HierarchyPanel::Draw(EditorLayer& editor, World& w)
	{
		ImGui::Begin(EditorLocalization::Label("Hierarchy"));
		ImGui::BeginDisabled(editor.mPlaying);
		if (ImGui::Button(EditorLocalization::Label("Create")))
			ImGui::OpenPopup(EditorLocalization::Label("Create object"));
		if (ImGui::BeginPopup(EditorLocalization::Label("Create object")))
		{
			const char* names[] = { "Empty", "Plane", "Cube", "Sphere", "Quad" };
			for (int i = 0; i < 5; ++i)
				if (ImGui::MenuItem(EditorLocalization::Label(names[i])))
					editor.Run([&] { editor.Create(w, BuiltinMesh(i)); });
			if (ImGui::MenuItem(EditorLocalization::Label("Camera")))
				editor.Run([&]
					{
						editor.Create(w, BuiltinMesh::None);
						w.SetName(editor.mSelected, "Camera");
						CameraComponent c;
						c.primary = w.View<CameraComponent>().begin() == w.View<CameraComponent>().end();
						w.AddComponent<CameraComponent>(editor.mSelected, c);
					});
			if (ImGui::MenuItem(EditorLocalization::Label("Sun")))
				editor.Run([&]
					{
						auto lights = w.View<DirectionalLightComponent>();
						if (lights.begin() != lights.end())
							editor.Select(w, *lights.begin());
						else
						{
							editor.Create(w, BuiltinMesh::None);
							w.SetName(editor.mSelected, "Sun");
							w.AddComponent<DirectionalLightComponent>(editor.mSelected);
							Camera basis;
							basis.SetLookDirection(Vector3(.35f, -1, .25f).NormalizedCopy());
							w.GetComponent<TransformComponent>(editor.mSelected).rotation = basis.GetRotation();
						}
					});
			if (ImGui::MenuItem(EditorLocalization::Label("UI Text")))
				editor.Run([&]
					{
						editor.Create(w, BuiltinMesh::None);
						w.AddComponent<UiElementComponent>(editor.mSelected);
					});
			ImGui::EndPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button(EditorLocalization::Label("Duplicate")))
			editor.Run([&] { editor.Duplicate(w); });
		ImGui::SameLine();
		if (ImGui::Button(EditorLocalization::Label("Delete")))
			editor.Run([&]
				{
					EditorActions::DeleteTree(w, editor.mSelected);
					editor.Select(w, entt::null);
				});
		ImGui::EndDisabled();

		ImGui::Separator();
		ImGui::TextDisabled(EditorLocalization::Text("WORLD OUTLINER"));
		ImGui::SetNextItemWidth(-1);
		if (ImGui::InputTextWithHint("##Search objects", EditorLocalization::Text("Search objects..."), search.InputBuf,
			sizeof(search.InputBuf)))
			search.Build();
		std::vector<Entity> roots;
		std::map<Entity, std::vector<Entity>> children;
		for (auto e : w.View<PersistentIdComponent>())
		{
			auto h = w.TryGet<HierarchyComponent>(e);
			auto parent = h ? EditorActions::Find(w, h->parentId) : entt::null;
			if (parent == entt::null)
				roots.push_back(e);
			else
				children[parent].push_back(e);
		}
		auto label = [&](Entity e)
			{
				auto n = w.TryGet<NameComponent>(e);
				return n ? n->value : "Object";
			};
		auto sort = [&](std::vector<Entity>& entities)
			{
				std::sort(entities.begin(), entities.end(), [&](Entity a, Entity b)
					{
						auto kind = [&](Entity entity)
							{
								return w.HasComponent<CameraComponent>(entity) ? 0
									: w.HasComponent<DirectionalLightComponent>(entity) ? 1
									: 2;
							};
						if (kind(a) != kind(b))
							return kind(a) < kind(b);
						auto x = label(a), y = label(b);
						return x == y ? EditorActions::Id(w, a) < EditorActions::Id(w, b) : x < y;
					});
			};
		ImGui::TextDisabled(EditorLocalization::Text("%zu root objects"), roots.size());
		ImGui::Spacing();
		sort(roots);
		for (auto& [parent, list] : children)
			sort(list);
		ImGui::Selectable(EditorLocalization::Label("Scene root (drop to detach)"), false);
		if (!editor.mPlaying && ImGui::BeginDragDropTarget())
		{
			if (auto payload = ImGui::AcceptDragDropPayload("ATOM_ENTITY"))
			{
				auto entity = *static_cast<const Entity*>(payload->Data);
				editor.Run([&] { EditorActions::Reparent(w, entity, entt::null); });
			}
			ImGui::EndDragDropTarget();
		}
		std::function<bool(Entity)> matches = [&](Entity e)
			{
				if (search.PassFilter(label(e).c_str()))
					return true;
				for (auto c : children[e])
					if (matches(c))
						return true;
				return false;
			};
		std::function<void(Entity)> draw = [&](Entity e)
			{
				if (!w.IsValid(e) || !matches(e))
					return;
				auto flags =
					ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
				if (e == editor.mSelected)
					flags |= ImGuiTreeNodeFlags_Selected;
				if (children[e].empty())
					flags |= ImGuiTreeNodeFlags_Leaf;
				if (search.IsActive())
					ImGui::SetNextItemOpen(true);
				bool open =
					ImGui::TreeNodeEx(reinterpret_cast<void*>(uintptr_t(uint32_t(e)) + 1), flags, "%s", label(e).c_str());
				if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
					editor.Select(w, e);
				if (!editor.mPlaying && ImGui::BeginDragDropSource())
				{
					ImGui::SetDragDropPayload("ATOM_ENTITY", &e, sizeof(e));
					ImGui::TextUnformatted(label(e).c_str());
					ImGui::EndDragDropSource();
				}
				if (!editor.mPlaying && ImGui::BeginDragDropTarget())
				{
					if (auto payload = ImGui::AcceptDragDropPayload("ATOM_ENTITY"))
					{
						auto entity = *static_cast<const Entity*>(payload->Data);
						editor.Run([&] { EditorActions::Reparent(w, entity, e); });
					}
					if (auto payload = ImGui::AcceptDragDropPayload("ATOM_ASSET"))
					{
						auto path = std::filesystem::path(UTF8ToWString(static_cast<const char*>(payload->Data)));
						if (path.extension() == L".mat")
							editor.Run([&] { editor.ApplyMaterial(w, e, path); });
					}
					ImGui::EndDragDropTarget();
				}
				if (ImGui::BeginPopupContextItem())
				{
					editor.Select(w, e);
					ImGui::BeginDisabled(editor.mPlaying);
					if (ImGui::MenuItem(EditorLocalization::Label("Duplicate subtree")))
						editor.Run([&] { editor.Duplicate(w); });
					if (ImGui::MenuItem(EditorLocalization::Label("Delete subtree")))
						editor.Run([&]
							{
								EditorActions::DeleteTree(w, e);
								editor.Select(w, entt::null);
							});
					ImGui::EndDisabled();
					ImGui::EndPopup();
				}
				if (open)
				{
					for (auto child : children[e])
						draw(child);
					ImGui::TreePop();
				}
			};
		for (auto e : roots)
			draw(e);
		ImGui::End();
	}
}
