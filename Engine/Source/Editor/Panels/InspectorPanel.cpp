#include "../EditorPanelSupport.h"
#include "../ScriptFieldEditor.h"
namespace AtomEngine {
void InspectorPanel::Draw(EditorLayer &editor, Scene &s)
{
        if (ImGui::GetFrameCount() == EditorDefaultPanelFocusFrame) ImGui::SetNextWindowFocus();
		auto& w = s.GetWorld();
		ImGui::Begin(EditorLocalization::Label("Inspector"));
		if (!w.IsValid(editor.mSelected))
		{
			ImGui::Spacing();
			ImGui::TextDisabled(EditorLocalization::Text("NO OBJECT SELECTED"));
			ImGui::Separator();
			ImGui::TextWrapped(
				EditorLocalization::Text("Select an object in the viewport or Hierarchy to edit its properties."));
			ImGui::End();
			return;
		}
		auto e = editor.mSelected;
		ImGui::BeginDisabled(editor.mPlaying);
		auto name = w.TryGet<NameComponent>(e);
		std::string objectName = name ? name->value : "Object";
		ImGui::TextUnformatted(objectName.c_str());
		ImGui::TextDisabled("%s",
			EditorLocalization::Text(w.HasComponent<CameraComponent>(e) ? "CAMERA"
				: w.HasComponent<DirectionalLightComponent>(e) ? "DIRECTIONAL LIGHT"
				: "SCENE OBJECT"));
		ImGui::Separator();
		if (Text("Name", objectName))
			w.SetName(e, objectName);
		ImGui::TextDisabled(EditorLocalization::Text("Persistent ID"));
		EditorTheme::Tooltip(w.GetComponent<PersistentIdComponent>(e).value.c_str());
		ImGui::Spacing();
		if (auto t = w.TryGet<TransformComponent>(e);
			t && ImGui::CollapsingHeader(EditorLocalization::Label("Transform"), ImGuiTreeNodeFlags_DefaultOpen))
		{
			TransformEditor(*t);
			auto h = w.TryGet<HierarchyComponent>(e);
			std::string parent = h ? h->parentId : "";
			if (ImGui::BeginCombo(EditorLocalization::Label("Parent"),
				parent.empty() ? EditorLocalization::Text("None") : parent.c_str()))
			{
				if (ImGui::Selectable(EditorLocalization::Label("None"), parent.empty()))
				{
					editor.Run([&] { EditorActions::Reparent(w, e, entt::null); });
				}
				for (auto p : w.View<PersistentIdComponent, TransformComponent>())
					if (p != e)
					{
						auto id = w.GetComponent<PersistentIdComponent>(p).value;
						auto n = w.TryGet<NameComponent>(p);
						ImGui::PushID(int(uint32_t(p)));
						if (ImGui::Selectable(n ? n->value.c_str() : id.c_str(), parent == id))
						{
							editor.Run([&] { EditorActions::Reparent(w, e, p); });
						}
						ImGui::PopID();
					}
				ImGui::EndCombo();
			}
		}
        ImGui::EndDisabled();
        auto state=w.TryGet<ObjectStateComponent>(e);if(state){ImGui::Checkbox(EditorLocalization::Label("Active self"),&state->activeSelf);ImGui::Text(EditorLocalization::Text("Active in hierarchy: %s"),w.IsActive(e)?"Yes":"No");
            const char *names[]={"Mesh enabled","Collider enabled","Rigid body enabled","Scripts enabled","UI enabled","Audio enabled","Camera enabled","Light enabled","Joint enabled"};
            bool present[]={w.HasComponent<MeshSourceComponent>(e),w.HasComponent<ColliderComponent>(e),w.HasComponent<RigidBodyComponent>(e),w.HasComponent<ScriptComponent>(e),w.HasComponent<UiElementComponent>(e),w.HasComponent<AudioSourceComponent>(e),w.HasComponent<CameraComponent>(e),w.HasComponent<DirectionalLightComponent>(e),w.HasComponent<JointComponent>(e)};
            for(int kind=0;kind<9;++kind){if(!present[kind])continue;bool enabled=!(state->disabledComponents & (1u<<kind));if(ImGui::Checkbox(EditorLocalization::Label(names[kind]),&enabled))w.SetComponentEnabled(e,ComponentKind(kind),enabled);}}
        ImGui::BeginDisabled(editor.mPlaying);
		editor.SceneObjectInspector(s);
		if (auto source = w.TryGet<MeshSourceComponent>(e);
			source && ImGui::CollapsingHeader(EditorLocalization::Label("Mesh")))
		{
			int type = int(source->primitive);
			bool rebuild = EditorLocalization::Combo("Primitive", &type, "Model asset\0Plane\0Cube\0Sphere\0Quad\0");
			auto id = EditorActions::Id(w, e);
			if (editor.mModelDraftId != id)
			{
				editor.mModelDraft = source->asset;
				editor.mModelDraftId = id;
			}
			AssetPath("Model path", editor.mModelDraft, "|.obj|.fbx|.gltf|.glb|.dae|.ply|.stl|");
			rebuild |= ImGui::Button(EditorLocalization::Label("Apply mesh"));
			if (rebuild)
				editor.Run([&]
					{
						auto previous = *source;
						if (BuiltinMesh(type) == BuiltinMesh::None && !AssetManager::LoadModel(editor.mModelDraft))
							throw std::runtime_error("Cannot load model: " + editor.mModelDraft);
						source->primitive = BuiltinMesh(type);
						source->asset = editor.mModelDraft;
						try
						{
							SceneSerializer::RebuildMesh(w, e);
						}
						catch (...)
						{
							*source = previous;
							throw;
						}
					});
			if (ImGui::Checkbox(EditorLocalization::Label("Visible"), &source->visible))
			{
				if (auto mesh = w.TryGet<MeshComponent>(e))
					mesh->SetIsRender(source->visible);
			}
		}
		if (auto materials = w.TryGet<MaterialComponent>(e);
			materials && ImGui::CollapsingHeader(EditorLocalization::Label("Material"), ImGuiTreeNodeFlags_DefaultOpen))
			for (size_t i = 0; i < materials->mMaterials.size(); ++i)
			{
				ImGui::PushID(int(i));
				auto& m = materials->mMaterials[i];
				ImGui::Text(EditorLocalization::Text("Material %zu"), i);
				if (!editor.mPlaying && ImGui::BeginDragDropTarget())
				{
					if (auto payload = ImGui::AcceptDragDropPayload("ATOM_ASSET"))
					{
						auto path = std::filesystem::path(UTF8ToWString(static_cast<const char*>(payload->Data)));
						if (path.extension() == L".mat")
							editor.Run([&] { editor.ApplyMaterial(w, e, path, int(i)); });
					}
					ImGui::EndDragDropTarget();
				}
				if (ImGui::SmallButton(EditorLocalization::Label("Preview this slot")))
				{
					editor.mPreviewSlot = int(i);
					editor.mPreviewFromSelection = true;
					editor.mShowMaterials = true;
				}
				Color4("BaseColor", m.mBaseColor.ptr());
				Float("Metallic", &m.mMetallic, .01f, 0, 1, "%.3f", ImGuiSliderFlags_AlwaysClamp);
				Float("Roughness", &m.mRoughness, .01f, .045f, 1, "%.3f", ImGuiSliderFlags_AlwaysClamp);
				Float3("Specular F0", m.mFresnelF0.ptr(), 0.01f, 0.0f, 1.0f);
				Color3("Emissive", m.mEmissive.ptr());
				Float("Normal strength", &m.mNormalScale, .01f, 0, 8);
				Float("AO", &m.mAO, .01f, 0, 1);
				const char* slots[] = { "BaseColor texture", "Metallic texture", "Roughness texture", "Normal texture",
									   "AO texture",        "Emissive texture", "Specular texture" };
				for (size_t slot = 0; slot < 7; ++slot)
					AssetPath(slots[slot], m.texturePaths[slot], "|.dds|.png|.jpg|.jpeg|.bmp|.tga|.hdr|.exr|.tif|.tiff|");
				if (ImGui::TreeNode(EditorLocalization::Label("UV transform")))
				{
					TransformEditor(m.uvTransform);
					ImGui::TreePop();
				}
				ImGui::PopID();
			}
		if (auto c = w.TryGet<ColliderComponent>(e);
			c && ImGui::CollapsingHeader(EditorLocalization::Label("Collider"), ImGuiTreeNodeFlags_DefaultOpen))
		{
			int shape = int(c->shape);
			EditorLocalization::Combo("Shape", &shape, "Box\0Sphere\0Capsule\0Triangle Mesh (static)\0Convex Hull\0");
			c->shape = ColliderShape(shape);
            int collisionLayer=int(c->collisionLayer);if(ImGui::SliderInt(EditorLocalization::Label("Collision layer"),&collisionLayer,0,31))c->collisionLayer=uint32_t(collisionLayer);
            ImGui::InputScalar(EditorLocalization::Label("Collision mask"),ImGuiDataType_U32,&c->collisionMask,nullptr,nullptr,"%08X",ImGuiInputTextFlags_CharsHexadecimal);
			if (c->shape == ColliderShape::Mesh || c->shape == ColliderShape::ConvexHull)
			{
				ImGui::TextWrapped(EditorLocalization::Text(
					"Uses the Mesh bind pose. Triangle Mesh requires a static body; Convex Hull supports moving bodies."));
				if (c->shape == ColliderShape::Mesh)
					if (auto body = w.TryGet<RigidBodyComponent>(e))
						body->motion = BodyMotion::Static;
			}
			Float3("Collider center", c->center.ptr(), .01f);
			Rotation("Collider rotation", c->rotation);
			if (c->shape == ColliderShape::Box)
				Float3("Half extents", c->halfExtents.ptr(), .01f, .001f, 1000, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			else
				Float("Radius", &c->radius, .01f, .001f, 1000, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			if (c->shape == ColliderShape::Capsule)
				Float("Half height", &c->halfHeight, .01f, .001f, 1000, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::Checkbox(EditorLocalization::Label("Trigger"), &c->trigger);
			ImGui::Checkbox(EditorLocalization::Label("Visualize collider"), &c->visible);
		}
		if (auto r = w.TryGet<RigidBodyComponent>(e); r && ImGui::CollapsingHeader(EditorLocalization::Label("Rigid Body")))
		{
			int motion = int(r->motion);
			auto collider = w.TryGet<ColliderComponent>(e);
			bool staticMesh = collider && collider->shape == ColliderShape::Mesh;
			ImGui::BeginDisabled(staticMesh);
			EditorLocalization::Combo("Motion", &motion, "Static\0Kinematic\0Dynamic\0");
			ImGui::EndDisabled();
			r->motion = staticMesh ? BodyMotion::Static : BodyMotion(motion);
			Float("Mass", &r->mass, .1f, .001f, 10000, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			Float("Friction", &r->friction, .01f, 0, 2);
			Float("Restitution", &r->restitution, .01f, 0, 1);
			ImGui::Checkbox(EditorLocalization::Label("Gravity"), &r->gravity);
			Float3("Velocity", r->linearVelocity.ptr(), .05f);
		}
		if (auto joint = w.TryGet<JointComponent>(e);
			joint && ImGui::CollapsingHeader(EditorLocalization::Label("Joint"), ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Checkbox(EditorLocalization::Label("Joint enabled"), &joint->enabled);
			int type = int(joint->type);
			EditorLocalization::Combo("Joint type", &type, "Fixed\0Distance\0Hinge\0");
			joint->type = JointType(type);
			Text("Connected body ID", joint->connectedBody);
			if (ImGui::BeginCombo(EditorLocalization::Label("Connect to"), joint->connectedBody.c_str()))
			{
				for (auto target : w.View<ColliderComponent, PersistentIdComponent>())
					if (target != e)
					{
						auto name = w.TryGet<NameComponent>(target);
						auto id = w.GetComponent<PersistentIdComponent>(target).value;
						if (ImGui::Selectable((name ? name->value + "##" + id : id).c_str(), joint->connectedBody == id))
							joint->connectedBody = id;
					}
				ImGui::EndCombo();
			}
			if (joint->type != JointType::Fixed)
			{
				Float3("Anchor", joint->anchor.ptr(), .01f);
				Float3("Connected anchor", joint->connectedAnchor.ptr(), .01f);
			}
			else
				ImGui::TextWrapped(EditorLocalization::Text("Welds the bodies in their current relative pose."));
			if (joint->type == JointType::Distance)
			{
				ImGui::DragFloat(EditorLocalization::Label("Min distance"), &joint->minDistance, .01f, 0,
					joint->maxDistance, "%.2f", ImGuiSliderFlags_AlwaysClamp);
				ImGui::DragFloat(EditorLocalization::Label("Max distance"), &joint->maxDistance, .01f, joint->minDistance,
					10000, "%.2f", ImGuiSliderFlags_AlwaysClamp);
			}
			if (joint->type == JointType::Hinge)
			{
				Float3("Hinge axis", joint->axis.ptr(), .01f);
				if (joint->axis.LengthSqr() < 1e-8f)
					joint->axis = Vector3::UP;
				joint->axis.Normalize();
				ImGui::DragFloat(EditorLocalization::Label("Min angle"), &joint->minAngle, .5f, -180, 0, "%.1f",
					ImGuiSliderFlags_AlwaysClamp);
				ImGui::DragFloat(EditorLocalization::Label("Max angle"), &joint->maxAngle, .5f, 0, 180, "%.1f",
					ImGuiSliderFlags_AlwaysClamp);
			}
		}
		if (auto audio = w.TryGet<AudioSourceComponent>(e);
			audio && ImGui::CollapsingHeader(EditorLocalization::Label("Audio Source"), ImGuiTreeNodeFlags_DefaultOpen))
		{
			AssetPath("Audio clip", audio->clip, "|.mp3|.wav|");
			ImGui::SliderFloat(EditorLocalization::Label("Audio volume"), &audio->volume, 0, 1);
			ImGui::Checkbox(EditorLocalization::Label("Audio loop"), &audio->loop);
			ImGui::Checkbox(EditorLocalization::Label("Play on awake"), &audio->playOnAwake);
			if (ImGui::Button(EditorLocalization::Label("Preview audio"))) editor.Run([&]
				{
					Audio::GetInstance()->StopOwner(UINT64_MAX);
					Audio::GetInstance()->PlayClip(audio->clip, audio->volume, audio->loop, UINT64_MAX);
				});
			ImGui::SameLine();
			if (ImGui::Button(EditorLocalization::Label("Stop audio"))) Audio::GetInstance()->StopOwner(UINT64_MAX);
		}
		if (auto scripts = w.TryGet<ScriptComponent>(e);
			scripts && ImGui::CollapsingHeader(EditorLocalization::Label("C# Scripts"), ImGuiTreeNodeFlags_DefaultOpen))
		{
			size_t remove = size_t(-1);
			for (size_t i = 0; i < scripts->scripts.size(); ++i)
			{
				ImGui::PushID(int(i));
				auto& s = scripts->scripts[i];
				ImGui::Checkbox(EditorLocalization::Label("Enabled"), &s.enabled);
				AssetPath("Assembly", s.assembly, "|.dll|");
                ImGui::EndDisabled();
                editor.ScriptFields(s,editor.mPlaying && gContext.readScriptFields?gContext.readScriptFields(w,e,i):"{}");
                ImGui::BeginDisabled(editor.mPlaying);
				ImGui::SameLine();
				if (ImGui::Button(EditorLocalization::Label("Remove script")))
					remove = i;
				ImGui::Separator();
				ImGui::PopID();
			}
			if (remove != size_t(-1))
				scripts->scripts.erase(scripts->scripts.begin() + remove);
			if (ImGui::Button(EditorLocalization::Label("Attach script")))
				scripts->scripts.push_back({"Scripts/AtomScripts.dll","",true,{}});
			editor.ScriptCreation(w);
		}
		if (auto ui = w.TryGet<UiElementComponent>(e);
			ui && ImGui::CollapsingHeader(EditorLocalization::Label("UI"), ImGuiTreeNodeFlags_DefaultOpen))
		{
            ImGui::Checkbox(EditorLocalization::Label("Responsive layout"),&ui->responsive);
            Float2("Anchor min",&ui->anchorMin.x,.01f,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);Float2("Anchor max",&ui->anchorMax.x,.01f,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
            ui->anchorMax.x=std::max(ui->anchorMin.x,ui->anchorMax.x);ui->anchorMax.y=std::max(ui->anchorMin.y,ui->anchorMax.y);
            Float2("Pivot",&ui->pivot.x,.01f,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
            int layout=int(ui->layout);if(ImGui::Combo(EditorLocalization::Label("Child layout"),&layout,"None\0Horizontal\0Vertical\0"))ui->layout=UiLayout(layout);
            Float("Layout padding",&ui->padding,1,0,1024);Float("Layout spacing",&ui->spacing,1,0,1024);ImGui::InputInt("Layout order",&ui->layoutOrder);
            ImGui::Checkbox(EditorLocalization::Label("Progress bar"),&ui->progressBar);ImGui::SliderFloat(EditorLocalization::Label("Progress"),&ui->progress,0,1);
			Text("Text (UTF-8)", ui->text);
			AssetPath("Image", ui->texture, "|.dds|.png|.jpg|.jpeg|.bmp|.tga|");
			Float2("Screen position", &ui->position.x, 1);
			Float2("Size", &ui->size.x, 1, 1, 4096, "%.1f", ImGuiSliderFlags_AlwaysClamp);
			Float("Font size", &ui->fontSize, 1, 1, 256);
			Color4("UI color", ui->color.ptr());
			ImGui::Checkbox(EditorLocalization::Label("UI visible"), &ui->visible);
			ImGui::Checkbox(EditorLocalization::Label("Clickable"), &ui->interactable);
			ImGui::InputInt(EditorLocalization::Label("UI layer"), &ui->layer);
			Color4("Background", ui->background.ptr());
		}
		if (ImGui::Button(EditorLocalization::Label("Add component")))
			ImGui::OpenPopup(EditorLocalization::Label("Components"));
		if (ImGui::BeginPopup(EditorLocalization::Label("Components")))
		{
			if (!w.HasComponent<MeshSourceComponent>(e) && ImGui::MenuItem(EditorLocalization::Label("Mesh")))
			{
				editor.Run([&]
					{
						w.AddComponent<MeshSourceComponent>(e, BuiltinMesh::Cube);
						SceneSerializer::RebuildMesh(w, e);
					});
			}
			if (!w.HasComponent<ColliderComponent>(e) && ImGui::MenuItem(EditorLocalization::Label("Collider")))
				w.AddComponent<ColliderComponent>(e);
			if (!w.HasComponent<RigidBodyComponent>(e) && ImGui::MenuItem(EditorLocalization::Label("Rigid Body")))
				w.AddComponent<RigidBodyComponent>(e);
			if (!w.HasComponent<JointComponent>(e) && ImGui::MenuItem(EditorLocalization::Label("Joint")))
				w.AddComponent<JointComponent>(e);
			if (!w.HasComponent<ScriptComponent>(e) && ImGui::MenuItem(EditorLocalization::Label("C# Script")))
			{
				w.AddComponent<ScriptComponent>(e);
				w.GetComponent<ScriptComponent>(e).scripts.emplace_back();
			}
			if (!w.HasComponent<AudioSourceComponent>(e) && ImGui::MenuItem(EditorLocalization::Label("Audio Source")))
				w.AddComponent<AudioSourceComponent>(e);
			if (!w.HasComponent<UiElementComponent>(e) && ImGui::MenuItem(EditorLocalization::Label("UI")))
				w.AddComponent<UiElementComponent>(e);
			ImGui::EndPopup();
		}
		if (ImGui::Button(EditorLocalization::Label("Remove component")))
			ImGui::OpenPopup(EditorLocalization::Label("Remove component menu"));
		if (ImGui::BeginPopup(EditorLocalization::Label("Remove component menu")))
		{
			if (w.HasComponent<MeshSourceComponent>(e) && ImGui::MenuItem(EditorLocalization::Label("Mesh + materials")))
			{
				w.RemoveComponent<MeshComponent>(e);
				w.RemoveComponent<MaterialComponent>(e);
				w.RemoveComponent<MeshSourceComponent>(e);
			}
			if (w.HasComponent<ColliderComponent>(e) && ImGui::MenuItem(EditorLocalization::Label("Collider")))
				w.RemoveComponent<ColliderComponent>(e);
			if (w.HasComponent<RigidBodyComponent>(e) && ImGui::MenuItem(EditorLocalization::Label("Rigid body")))
				w.RemoveComponent<RigidBodyComponent>(e);
			if (w.HasComponent<ScriptComponent>(e) && ImGui::MenuItem(EditorLocalization::Label("Scripts")))
				w.RemoveComponent<ScriptComponent>(e);
			if (w.HasComponent<JointComponent>(e) && ImGui::MenuItem(EditorLocalization::Label("Joint")))
				w.RemoveComponent<JointComponent>(e);
			if (w.HasComponent<AudioSourceComponent>(e) && ImGui::MenuItem(EditorLocalization::Label("Audio Source")))
			{
				Audio::GetInstance()->StopOwner(UINT64_MAX);
				w.RemoveComponent<AudioSourceComponent>(e);
			}
			if (w.HasComponent<UiElementComponent>(e) && ImGui::MenuItem(EditorLocalization::Label("UI")))
				w.RemoveComponent<UiElementComponent>(e);
			ImGui::EndPopup();
		}
		ImGui::EndDisabled();
		ImGui::End();
	}
void InspectorPanel::DrawSceneComponents(EditorLayer &editor, Scene &scene)
{
    auto &world = scene.GetWorld();
    auto camera = world.TryGet<CameraComponent>(editor.mSelected);
    auto light = world.TryGet<DirectionalLightComponent>(editor.mSelected);
    if (!camera && !light)
        return;
    if (camera && ImGui::CollapsingHeader(EditorLocalization::Label("Camera"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox(EditorLocalization::Label("Camera enabled"), &camera->enabled);
        ImGui::Text("%s", EditorLocalization::Text(camera->primary ? "Primary scene camera" : "Additional camera"));
        if (!camera->primary && ImGui::Button(EditorLocalization::Label("Make primary camera")))
        {
            for (auto e : world.View<CameraComponent>())
                world.GetComponent<CameraComponent>(e).primary = e == editor.mSelected;
            editor.mHistoryPending = true;
        }
        ImGui::DragFloat(EditorLocalization::Label("Field of view"), &camera->fieldOfView, .2f, 1, 179, "%.1f deg",
                         ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat(EditorLocalization::Label("Near clip"), &camera->nearClip, .01f, .001f,
                         std::max(.001f, camera->farClip - .01f), "%.3f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat(EditorLocalization::Label("Far clip"), &camera->farClip, 1, camera->nearClip + .01f, 100000,
                         "%.1f", ImGuiSliderFlags_AlwaysClamp);
        if (ImGui::Button(EditorLocalization::Label("Align camera to editor view")))
            editor.Run([&] {
                auto &view = scene.GetCamera();
                Transform pose(view.GetPosition(), view.GetRotation(), Vector3::UNIT_SCALE);
                EditorActions::SetWorldMatrix(world, editor.mSelected, pose.GetMatrix());
                camera->fieldOfView = view.GetFOV() * 180.f / Math::PI;
                camera->nearClip = view.GetNearClip();
                camera->farClip = view.GetFarClip();
                editor.mHistoryPending = true;
            });
        if (ImGui::Button(EditorLocalization::Label("Look through camera")))
            editor.Run([&] {
                ApplySceneCamera(world, scene.GetCamera(), editor.mSelected);
                editor.mFocus = scene.GetCamera().GetPosition() + scene.GetCamera().GetForwardVec() * 5.f;
            });
    }
    if (light &&
        ImGui::CollapsingHeader(EditorLocalization::Label("Directional Light"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox(EditorLocalization::Label("Sun enabled"), &light->enabled);
        ImGui::ColorEdit3(EditorLocalization::Label("Sun color"), light->color.ptr());
        ImGui::DragFloat(EditorLocalization::Label("Intensity"), &light->intensity, .05f, 0, 1000, "%.2f",
                         ImGuiSliderFlags_AlwaysClamp);
        ImGui::Checkbox(EditorLocalization::Label("Cast shadows"), &light->shadows);
        ImGui::Checkbox(EditorLocalization::Label("Auto fit shadow"), &light->autoFitShadow);
        auto direction = world.GetComponent<TransformComponent>(editor.mSelected).GetMatrix().GetRotation() * Vector3::FORWARD;
        ImGui::Text(EditorLocalization::Text("Direction: %.3f, %.3f, %.3f"), direction.x, direction.y, direction.z);
        ImGui::TextWrapped(EditorLocalization::Text("Use Transform rotation or the Rotate tool to aim the sun. "
                                                    "Position does not affect directional lighting."));
    }
}
}
