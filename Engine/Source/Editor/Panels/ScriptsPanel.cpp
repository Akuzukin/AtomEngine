#include "../EditorPanelSupport.h"
#include "../ScriptFieldEditor.h"
namespace AtomEngine
{
	void ScriptsPanel::Draw(EditorLayer& editor, World& world)
	{
		if (ImGui::Button(EditorLocalization::Label("Open scripts in VS Code")))editor.Run([]
			{
				auto workspace = ScriptTools::WorkspacePath();
				if (!std::filesystem::exists(workspace))throw std::runtime_error("VS Code script workspace not found");
				auto executable = ScriptTools::VsCodeExecutable();
				auto arguments = L"--reuse-window \"" + workspace.wstring() + L"\"";
				if (INT_PTR(ShellExecuteW(nullptr, L"open", executable.c_str(), arguments.c_str(), workspace.parent_path().c_str(), SW_SHOWNORMAL)) <= 32)throw std::runtime_error("Cannot launch VS Code; open Managed/AtomScripts.code-workspace manually");
			});
		ImGui::SameLine();
		ImGui::BeginDisabled(editor.mPlaying || editor.mScriptBuild.valid());
		if (ImGui::Button(EditorLocalization::Label("Create C# script")))
			ImGui::OpenPopup(EditorLocalization::Label("New Behaviour"));
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(editor.mScriptBuild.valid());
		if (ImGui::Button(EditorLocalization::Label("Build scripts")))
			editor.Run([&] { editor.mScriptBuild = ScriptTools::Build(); });
		ImGui::EndDisabled();
		if (ImGui::BeginPopup(EditorLocalization::Label("New Behaviour")))
		{
			static char name[128] = "";
			static bool callbacks = true, attach = true;
			ImGui::InputText(EditorLocalization::Label("Class name"), name, sizeof(name));
			ImGui::Checkbox(EditorLocalization::Label("Generate Behaviour overrides"), &callbacks);
			ImGui::BeginDisabled(!world.IsValid(editor.mSelected));
			ImGui::Checkbox(EditorLocalization::Label("Attach to selected object"), &attach);
			ImGui::EndDisabled();
			if (ImGui::Button(EditorLocalization::Label("Generate")))
				editor.Run([&]
					{
						auto file = ScriptTools::Generate(name, callbacks);
						editor.mAssetDirectory = file.parent_path();
						if (attach && world.IsValid(editor.mSelected))
						{
							if (!world.HasComponent<ScriptComponent>(editor.mSelected))
								world.AddComponent<ScriptComponent>(editor.mSelected);
							ScriptAttachment script;
							script.assembly = "Scripts/AtomScripts.dll";
							script.type = std::string("Game.") + name;
							auto& scripts = world.GetComponent<ScriptComponent>(editor.mSelected).scripts;
							if (!scripts.empty() && scripts.back().assembly.empty() && scripts.back().type.empty())
								scripts.back() = script;
							else
								scripts.push_back(script);
							editor.mHistoryPending = true;
						}
						editor.mRefreshAssets = true;
						ImGui::CloseCurrentPopup();
					});
			ImGui::EndPopup();
		}
	}
	void ScriptsPanel::DrawFields(EditorLayer& editor, World& world, ScriptAttachment& s, const std::string& live)
	{
		using Json = nlohmann::json;
		wchar_t exe[32768]{}; GetModuleFileNameW(nullptr, exe, DWORD(std::size(exe)));
		auto path = std::filesystem::path(UTF8ToWString(s.assembly)); if (path.is_relative())path = std::filesystem::path(exe).parent_path() / path;
		std::error_code ec; auto stamp = std::filesystem::last_write_time(path, ec);
		auto& cache = editor.mScriptSchemas[path.generic_string()];
		if (cache.json.empty() || cache.stamp != stamp)
		{
			cache.stamp = stamp;
			try { cache.json = editor.mScriptInspector.DescribeAssembly(path); cache.error.clear(); }
			catch (const std::exception& error) { cache.json = "[]"; cache.error = error.what(); }
		}
		if (ImGui::Button(EditorLocalization::Label("Refresh script types")))cache.json.clear();
		if (!cache.error.empty()) { ImGui::TextWrapped("%s", cache.error.c_str()); return; }
		auto types = Json::parse(cache.json, nullptr, false); if (types.is_discarded() || !types.is_array())return;
		ImGui::BeginDisabled(editor.mPlaying);
		if (ImGui::BeginCombo(EditorLocalization::Label("Script class"), s.type.empty() ? EditorLocalization::Text("Select script class") : s.type.c_str()))
		{
			for (auto& t : types) { auto name = t.at("name").get<std::string>(); if (ImGui::Selectable(name.c_str(), s.type == name)) { s.type = name; s.fields.clear(); } }
			ImGui::EndCombo();
		}
		ImGui::EndDisabled();
		auto current = Json::parse(live, nullptr, false);
		if (editor.mPlaying)ImGui::TextDisabled("%s", EditorLocalization::Text("Live values; Stop restores the editing snapshot."));
		Json schema; for (auto& t : types)if (t.at("name") == s.type) schema = t;
		if (schema.is_null()) { if (!s.type.empty())ImGui::TextColored(ImVec4(1, .4f, .3f, 1), "Unknown class: %s", s.type.c_str()); return; }
		std::set<std::string> known;
		for (auto& field : schema.at("fields"))
		{
			auto name = field.at("name").get<std::string>(), kind = field.at("kind").get<std::string>(); known.insert(name);
			ImGui::PushID(name.c_str());
			if (kind == "unsupported" || (kind == "enum" && field.at("choices").empty())) { ImGui::TextDisabled("%s (unsupported type)", name.c_str()); ImGui::PopID(); continue; }
			bool overridden = s.fields.contains(name);
			if (ImGui::Checkbox(EditorLocalization::Label("Override"), &overridden))
			{
				if (overridden && current.is_object() && current.contains(name))s.fields[name] = current.at(name).get<std::string>();
				else if (overridden)s.fields[name] = kind == "bool" ? "false" : kind == "enum" ? field.at("choices").at(0).get<std::string>() :
					(kind == "string" || kind == "audioClip" || kind == "particleComponent") ? "" : kind == "vector2" ? "[0,0]" : kind == "vector3" ? "[0,0,0]" : kind == "vector4" ? "[0,0,0,0]" : kind == "quaternion" ? "[0,0,0,1]" : "0";
				else s.fields.erase(name);
			}
			ImGui::SameLine(); ImGui::TextUnformatted(name.c_str());
			if (!overridden) { if (current.is_object() && current.contains(name))ImGui::TextDisabled(EditorLocalization::Text("Current: %s"), current.at(name).get<std::string>().c_str()); else ImGui::TextDisabled("%s", EditorLocalization::Text("Use script default")); ImGui::PopID(); continue; }
			auto& value = s.fields[name]; bool changed = false;
			try
			{
				if(kind=="particleComponent") {
                    ImGui::Button(value.empty()?"Drop component, .particle or prefab":value.c_str());
                    if(ImGui::BeginDragDropTarget()) {
                        Entity target=entt::null;
                        if(auto payload=ImGui::AcceptDragDropPayload("ATOM_ENTITY"))target=*static_cast<const Entity*>(payload->Data);
                        if(auto payload=ImGui::AcceptDragDropPayload("ATOM_ASSET")) {
                            auto path=std::filesystem::path(UTF8ToWString(static_cast<const char*>(payload->Data)));
                            try {if(path.extension()==L".prefab"||path.extension()==L".particle")target=SceneSerializer::InstantiateParticlePrefab(world,path);}
                            catch(...){ImGui::EndDragDropTarget();throw;}
                        }
                        if(world.IsValid(target)&&world.HasComponent<ParticleComponent>(target)){value="particle:"+world.GetComponent<PersistentIdComponent>(target).value;changed=true;}
                        ImGui::EndDragDropTarget();
                    }
                    ImGui::SameLine();if(ImGui::SmallButton("Clear")){value="";changed=true;}
                }
                else if (kind == "audioClip") { changed = AssetPath("Audio clip", value, "|.mp3|.wav|"); }
				else if (kind == "bool") { bool v = _stricmp(value.c_str(), "true") == 0; changed = ImGui::Checkbox("##value", &v); if (changed)value = v ? "true" : "false"; }
				else if (kind == "enum")
				{
					if (ImGui::BeginCombo("##value", value.c_str())) { for (auto& option : field.at("choices")) { auto text = option.get<std::string>(); if (ImGui::Selectable(text.c_str(), text == value))value = text; }ImGui::EndCombo(); }
				}
				else if (kind == "vector2" || kind == "vector3" || kind == "vector4" || kind == "quaternion")
				{
					auto v = Json::parse(value).get<std::vector<float>>(); int count = kind == "vector2" ? 2 : kind == "vector3" ? 3 : 4;
					if (v.size() != size_t(count))throw std::runtime_error("Expected vector array");
					changed = ImGui::InputScalarN("##value", ImGuiDataType_Float, v.data(), count);
					if (changed)value = Json(v).dump();
				}
				else if (kind == "string") { char text[2048]{}; strncpy_s(text, value.c_str(), _TRUNCATE); if (ImGui::InputText("##value", text, sizeof(text)))value = text; }
				else
				{
					std::ostringstream out; out << std::setprecision(17);
					if (kind == "float" || kind == "double") { double v = std::stod(value); if (!std::isfinite(v))throw std::runtime_error("Non-finite value"); changed = ImGui::InputScalar("##value", ImGuiDataType_Double, &v); out << v; }
					else if (kind == "int") { int v = std::stoi(value); changed = ImGui::InputInt("##value", &v); out << v; }
					else if (kind == "uint") { auto n = std::stoull(value); if (n > UINT32_MAX || value.starts_with('-'))throw std::runtime_error("Invalid unsigned value"); uint32_t v = uint32_t(n); changed = ImGui::InputScalar("##value", ImGuiDataType_U32, &v); out << v; }
					else if (kind == "long") { int64_t v = std::stoll(value); changed = ImGui::InputScalar("##value", ImGuiDataType_S64, &v); out << v; }
					else { if (value.starts_with('-'))throw std::runtime_error("Invalid unsigned value"); uint64_t v = std::stoull(value); changed = ImGui::InputScalar("##value", ImGuiDataType_U64, &v); out << v; }
					if (changed)value = out.str();
				}
			}
			catch (const std::exception& error) { ImGui::TextColored(ImVec4(1, .4f, .3f, 1), "%s", error.what()); if (ImGui::SmallButton(EditorLocalization::Label("Reset override")))s.fields.erase(name); }
			ImGui::PopID();
		}
		std::string removed; for (auto& [key, value] : s.fields)if (!known.contains(key)) { ImGui::PushID(key.c_str()); ImGui::TextColored(ImVec4(1, .4f, .3f, 1), "Unknown field: %s", key.c_str()); if (ImGui::SmallButton(EditorLocalization::Label("Remove invalid field")))removed = key; ImGui::PopID(); }
		if (!removed.empty())s.fields.erase(removed);
	}
}
