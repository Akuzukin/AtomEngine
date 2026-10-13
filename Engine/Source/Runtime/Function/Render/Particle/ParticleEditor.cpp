#include "ParticleEditor.h"
#include "Runtime/Core/Utility/Utility.h"
#include "Runtime/Function/Render/Particle/Particle.h"
#include "Runtime/Function/Render/Particle/ParticleSystem.h"

#include "imgui.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <json.hpp>

namespace AtomEngine
{
	using json = nlohmann::json;

	ParticleEditor& ParticleEditor::Get()
	{
		static ParticleEditor s;
		return s;
	}

	ParticleEditor::ParticleEditor()
	{
		mPathBuf[0] = '\0';
	}

	void ParticleEditor::DrawColor4(const char* label, Vector4& v)
	{
		float tmp[4] = { v.x, v.y, v.z, v.w };
		if (ImGui::ColorEdit4(label, tmp))
		{
			v.x = tmp[0]; v.y = tmp[1]; v.z = tmp[2]; v.w = tmp[3];
		}
	}

	void ParticleEditor::DrawEmitterUI(EmitterProperty& e)
	{
		ImGui::DragFloat3("EmitPosW", &e.EmitPosW.x);
		ImGui::DragFloat3("EmitDirW", &e.EmitDirW.x);
		ImGui::DragFloat3("EmitRightW", &e.EmitRightW.x);
		ImGui::DragFloat3("EmitUpW", &e.EmitUpW.x);
		ImGui::DragFloat3("Gravity", &e.Gravity.x);
		ImGui::DragFloat("EmitSpeed", &e.EmitSpeed);
		ImGui::DragFloat("EmitterVelocitySensitivity", &e.EmitterVelocitySensitivity);
		ImGui::DragFloat("FloorHeight", &e.FloorHeight);
		ImGui::DragFloat("Restitution", &e.Restitution);
		ImGui::DragInt("MaxParticles", (int*)&e.MaxParticles);
	}

	void ParticleEditor::Render()
	{
		if (!mOpen||!ImGui::GetCurrentContext()) return;

		ImGui::Begin("Particle Editor", &mOpen);

		if (ImGui::CollapsingHeader("Colors & Size", ImGuiTreeNodeFlags_DefaultOpen))
		{
			DrawColor4("MinStartColor", mCurrent.MinStartColor);
			DrawColor4("MaxStartColor", mCurrent.MaxStartColor);
			DrawColor4("MinEndColor", mCurrent.MinEndColor);
			DrawColor4("MaxEndColor", mCurrent.MaxEndColor);

			ImGui::Separator();
			ImGui::Text("Size (start min,max / end min,max)");
			ImGui::DragFloat4("Size", &mCurrent.Size.x);
		}

		if (ImGui::CollapsingHeader("Emitter", ImGuiTreeNodeFlags_DefaultOpen))
		{
			DrawEmitterUI(mCurrent.EmitProperties);
		}

		if (ImGui::CollapsingHeader("Other", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::DragFloat4("Velocity (xMin,xMax,yMin,yMax)", &mCurrent.Velocity.x);
			ImGui::DragFloat3("Spread", &mCurrent.Spread.x);
			ImGui::DragFloat("EmitRate", &mCurrent.EmitRate);
			ImGui::DragFloat2("LifeMinMax", &mCurrent.LifeMinMax.x);
			ImGui::DragFloat2("MassMinMax", &mCurrent.MassMinMax.x);
			ImGui::DragFloat("TotalActiveLifetime", &mCurrent.TotalActiveLifetime);

			char pathbuf[260];
			std::string cur = WStringToUTF8(mCurrent.TexturePath);
			std::memcpy(pathbuf, cur.c_str(), sizeof(pathbuf));
			pathbuf[sizeof(pathbuf) - 1] = 0;
			if (ImGui::InputText("Texture Path", pathbuf, sizeof(pathbuf)))
			{
				mCurrent.TexturePath = UTF8ToWString(pathbuf);
			}
		}

		ImGui::Separator();

		{
			const char* names[] = { "Normal", "Add", "None", "Screen" };
			auto BlendModeToIndex = [](BlendMode b)->int {
				switch (b)
				{
				case BlendMode::kBlendNormal: return 0;
				case BlendMode::kBlendAlphaAdd: return 1;
				case BlendMode::kBlendNone: return 2;
				case BlendMode::kBlendScreen: return 3;
				default: return 1;
				}
			};
			auto IndexToBlendMode = [](int idx)->BlendMode {
				switch (idx)
				{
				case 0: return BlendMode::kBlendNormal;
				case 1: return BlendMode::kBlendAlphaAdd;
				case 2: return BlendMode::kBlendNone;
				case 3: return BlendMode::kBlendScreen;
				default: return BlendMode::kBlendAlphaAdd;
				}
			};

			int curIndex = BlendModeToIndex(mBlendMode);
			if (ImGui::Combo("Blend Mode", &curIndex, names, IM_ARRAYSIZE(names)))
			{
				mBlendMode = IndexToBlendMode(curIndex);
				ParticleSystem::SetBlendMode(mBlendMode);
			}
		}

		if (ImGui::Button("Spawn Particle (create new)"))
		{
			ParticleSystem::CreateParticle(mCurrent);
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset to Defaults"))
		{
			mCurrent = ParticleProperty();
		}

		ImGui::Separator();

		ImGui::InputText("File Path (json)", mPathBuf, sizeof(mPathBuf));
		if (ImGui::Button("Save JSON"))
		{
			std::string utf8 = mPathBuf;
			if (utf8.empty())
				ImGui::OpenPopup("SaveError");
			else if (!SaveToFile(utf8))
				ImGui::OpenPopup("SaveError");
		}
		ImGui::SameLine();
		if (ImGui::Button("Load JSON"))
		{
			std::string utf8 = mPathBuf;
			if (utf8.empty() || !std::filesystem::exists(utf8))
				ImGui::OpenPopup("LoadError");
			else if (!LoadFromFile(utf8))
				ImGui::OpenPopup("LoadError");
		}

		if (ImGui::BeginPopup("SaveError"))
		{
			ImGui::Text("Save failed. Check path.");
			if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}
		if (ImGui::BeginPopup("LoadError"))
		{
			ImGui::Text("Load failed. Check path/format.");
			if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		ImGui::Separator();

		if (ImGui::CollapsingHeader("ActiveParticles", ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto& particles = ParticleSystem::GetParticles();

			int i = 0;
			for (auto& particle : particles)
			{
				ImGui::PushID(i);

				if (!particle)
				{
					ImGui::Text("Emitter %d: <null>", i);
					ImGui::PopID();
					++i;
					continue;
				}

				ParticleProperty& prop = particle->GetProperty();
				bool dirty = false;

				if (ImGui::TreeNode(("Emitter " + std::to_string(i)).c_str()))
				{
					// Colors & Size
					ImGui::Text("Colors & Size");
					dirty |= ImGui::ColorEdit4("MinStartColor", &prop.MinStartColor.x);
					dirty |= ImGui::ColorEdit4("MaxStartColor", &prop.MaxStartColor.x);
					dirty |= ImGui::ColorEdit4("MinEndColor", &prop.MinEndColor.x);
					dirty |= ImGui::ColorEdit4("MaxEndColor", &prop.MaxEndColor.x);
					dirty |= ImGui::DragFloat4("Size", &prop.Size.x, 0.1f, 0.0f, 1000.0f);

					ImGui::Separator();

					// Velocity / Spread / Life / Mass / EmitRate
					ImGui::Text("Other");
					dirty |= ImGui::DragFloat4("Velocity(xMin,xMax,yMin,yMax)", &prop.Velocity.x, 0.1f);
					dirty |= ImGui::DragFloat3("Spread", &prop.Spread.x, 0.1f);
					dirty |= ImGui::DragFloat("EmitRate", &prop.EmitRate, 0.1f, 0.0f, 1e6f);
					dirty |= ImGui::DragFloat2("LifeMinMax", &prop.LifeMinMax.x, 0.01f, 0.0f, 1e6f);
					dirty |= ImGui::DragFloat2("MassMinMax", &prop.MassMinMax.x, 0.01f, 0.0f, 1e6f);
					dirty |= ImGui::DragFloat("TotalActiveLifetime", &prop.TotalActiveLifetime, 0.01f, -1e6f, 1e6f);

					ImGui::Separator();

					// Emitter sub-properties
					if (ImGui::TreeNode("EmitterProperty"))
					{
						auto& e = prop.EmitProperties;
						dirty |= ImGui::DragFloat3("EmitPosW", &e.EmitPosW.x, 0.01f);
						dirty |= ImGui::DragFloat3("EmitDirW", &e.EmitDirW.x, 0.01f);
						dirty |= ImGui::DragFloat3("EmitRightW", &e.EmitRightW.x, 0.01f);
						dirty |= ImGui::DragFloat3("EmitUpW", &e.EmitUpW.x, 0.01f);
						dirty |= ImGui::DragFloat3("Gravity", &e.Gravity.x, 0.01f);
						dirty |= ImGui::DragFloat("EmitSpeed", &e.EmitSpeed, 0.01f);
						dirty |= ImGui::DragFloat("EmitterVelocitySensitivity", &e.EmitterVelocitySensitivity, 0.01f);
						dirty |= ImGui::DragFloat("FloorHeight", &e.FloorHeight, 0.01f);
						dirty |= ImGui::ColorEdit3("Emissive",e.EmissiveColor.ptr());
						{
							int tmpMax = (int)e.MaxParticles;
							if (ImGui::DragInt("MaxParticles", &tmpMax, 1.0f, 1, 0x8000000))
							{
								e.MaxParticles = (uint32_t)tmpMax;
								dirty = true;
							}
						}
						ImGui::TreePop();
					}

					// Texture path editable
					{
						std::string cur = WStringToUTF8(prop.TexturePath);
						char buf[260];
						std::memcpy(buf, cur.c_str(), std::min<size_t>(cur.size() + 1, sizeof(buf)));
						buf[sizeof(buf) - 1] = 0;
						if (ImGui::InputText("Texture Path", buf, sizeof(buf)))
						{
							prop.TexturePath = UTF8ToWString(buf);
							dirty = true;
						}
					}

					ImGui::Separator();

					if (ImGui::Button("Apply Edited To Particle"))
					{
						particle->SetProperty(prop);
					}
					ImGui::SameLine();
					if (ImGui::Button("Map Editor -> Particle"))
					{
						particle->SetProperty(mCurrent);
					}
					ImGui::SameLine();
					if (ImGui::Button("Load Particle -> Editor"))
					{
						mCurrent = particle->GetProperty();
					}

					if (dirty)
						particle->SetProperty(prop);

					ImGui::TreePop();
				}

				ImGui::PopID();
				++i;
			}
		}

		ImGui::End();
	}

	// Helper converters
	static json vec4_to_json(const Vector4& v)
	{
		return json::array({ v.x, v.y, v.z, v.w });
	}
	static Vector4 json_to_vec4(const json& j)
	{
		Vector4 v;
		if (j.is_array() && j.size() >= 4) { v.x = j[0].get<float>(); v.y = j[1].get<float>(); v.z = j[2].get<float>(); v.w = j[3].get<float>(); }
		return v;
	}
	static json vec3_to_json(const Vector3& v)
	{
		return json::array({ v.x, v.y, v.z });
	}
	static Vector3 json_to_vec3(const json& j)
	{
		Vector3 v;
		if (j.is_array() && j.size() >= 3) { v.x = j[0].get<float>(); v.y = j[1].get<float>(); v.z = j[2].get<float>(); }
		return v;
	}
	static json vec2_to_json(const Vector2& v)
	{
		return json::array({ v.x, v.y });
	}
	static Vector2 json_to_vec2(const json& j)
	{
		Vector2 v;
		if (j.is_array() && j.size() >= 2) { v.x = j[0].get<float>(); v.y = j[1].get<float>(); }
		return v;
	}

	std::string ParticleEditor::ToJson() const
	{
		json j;
		j["MinStartColor"] = vec4_to_json(mCurrent.MinStartColor);
		j["MaxStartColor"] = vec4_to_json(mCurrent.MaxStartColor);
		j["MinEndColor"] = vec4_to_json(mCurrent.MinEndColor);
		j["MaxEndColor"] = vec4_to_json(mCurrent.MaxEndColor);

		j["Velocity"] = vec4_to_json(mCurrent.Velocity);
		j["Size"] = vec4_to_json(mCurrent.Size);
		j["Spread"] = vec3_to_json(mCurrent.Spread);
		j["EmitRate"] = mCurrent.EmitRate;
		j["LifeMinMax"] = vec2_to_json(mCurrent.LifeMinMax);
		j["MassMinMax"] = vec2_to_json(mCurrent.MassMinMax);

		// Emitter
		json e;
		e["LastEmitPosW"] = vec3_to_json(mCurrent.EmitProperties.LastEmitPosW);
		e["EmitSpeed"] = mCurrent.EmitProperties.EmitSpeed;
		e["EmitPosW"] = vec3_to_json(mCurrent.EmitProperties.EmitPosW);
		e["FloorHeight"] = mCurrent.EmitProperties.FloorHeight;
		e["EmitDirW"] = vec3_to_json(mCurrent.EmitProperties.EmitDirW);
		e["Restitution"] = mCurrent.EmitProperties.Restitution;
		e["EmitRightW"] = vec3_to_json(mCurrent.EmitProperties.EmitRightW);
		e["EmitterVelocitySensitivity"] = mCurrent.EmitProperties.EmitterVelocitySensitivity;
		e["EmitUpW"] = vec3_to_json(mCurrent.EmitProperties.EmitUpW);
		e["MaxParticles"] = mCurrent.EmitProperties.MaxParticles;
		e["Gravity"] = vec3_to_json(mCurrent.EmitProperties.Gravity);
		e["EmissiveColor"] = vec3_to_json(mCurrent.EmitProperties.EmissiveColor);
		j["EmitProperties"] = e;

		j["TexturePath"] = WStringToUTF8(mCurrent.TexturePath);
		j["TotalActiveLifetime"] = mCurrent.TotalActiveLifetime;

		return j.dump(4);
	}

	bool ParticleEditor::SaveToFile(const std::string& utf8Path)
	{
		try
		{
			std::ofstream ofs(utf8Path, std::ios::binary);
			if (!ofs) return false;
			ofs << ToJson();
			ofs.close();
			return true;
		}
		catch (...) { return false; }
	}

	bool ParticleEditor::FromJson(const std::string& text)
	{
		try
		{
			auto j = json::parse(text);

			if (j.contains("MinStartColor")) mCurrent.MinStartColor = json_to_vec4(j["MinStartColor"]);
			if (j.contains("MaxStartColor")) mCurrent.MaxStartColor = json_to_vec4(j["MaxStartColor"]);
			if (j.contains("MinEndColor")) mCurrent.MinEndColor = json_to_vec4(j["MinEndColor"]);
			if (j.contains("MaxEndColor")) mCurrent.MaxEndColor = json_to_vec4(j["MaxEndColor"]);

			if (j.contains("Velocity")) mCurrent.Velocity = json_to_vec4(j["Velocity"]);
			if (j.contains("Size")) mCurrent.Size = json_to_vec4(j["Size"]);
			if (j.contains("Spread")) mCurrent.Spread = json_to_vec3(j["Spread"]);
			if (j.contains("EmitRate")) mCurrent.EmitRate = j["EmitRate"].get<float>();
			if (j.contains("LifeMinMax")) mCurrent.LifeMinMax = json_to_vec2(j["LifeMinMax"]);
			if (j.contains("MassMinMax")) mCurrent.MassMinMax = json_to_vec2(j["MassMinMax"]);

			if (j.contains("EmitProperties"))
			{
				auto e = j["EmitProperties"];
				if (e.contains("LastEmitPosW")) mCurrent.EmitProperties.LastEmitPosW = json_to_vec3(e["LastEmitPosW"]);
				if (e.contains("EmitSpeed")) mCurrent.EmitProperties.EmitSpeed = e["EmitSpeed"].get<float>();
				if (e.contains("EmitPosW")) mCurrent.EmitProperties.EmitPosW = json_to_vec3(e["EmitPosW"]);
				if (e.contains("FloorHeight")) mCurrent.EmitProperties.FloorHeight = e["FloorHeight"].get<float>();
				if (e.contains("EmitDirW")) mCurrent.EmitProperties.EmitDirW = json_to_vec3(e["EmitDirW"]);
				if (e.contains("Restitution")) mCurrent.EmitProperties.Restitution = e["Restitution"].get<float>();
				if (e.contains("EmitRightW")) mCurrent.EmitProperties.EmitRightW = json_to_vec3(e["EmitRightW"]);
				if (e.contains("EmitterVelocitySensitivity")) mCurrent.EmitProperties.EmitterVelocitySensitivity = e["EmitterVelocitySensitivity"].get<float>();
				if (e.contains("EmitUpW")) mCurrent.EmitProperties.EmitUpW = json_to_vec3(e["EmitUpW"]);
				if (e.contains("MaxParticles")) mCurrent.EmitProperties.MaxParticles = e["MaxParticles"].get<uint32_t>();
				if (e.contains("Gravity")) mCurrent.EmitProperties.Gravity = json_to_vec3(e["Gravity"]);
				if (e.contains("EmissiveColor")) mCurrent.EmitProperties.EmissiveColor = json_to_vec3(e["EmissiveColor"]);
			}

			if (j.contains("TexturePath"))
				mCurrent.TexturePath = UTF8ToWString(j["TexturePath"].get<std::string>());

			if (j.contains("TotalActiveLifetime"))
				mCurrent.TotalActiveLifetime = j["TotalActiveLifetime"].get<float>();

			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	bool ParticleEditor::LoadFromFile(const std::string& utf8Path)
	{
		try
		{
			std::ifstream ifs(utf8Path, std::ios::binary);
			if (!ifs) return false;
			std::ostringstream ss;
			ss << ifs.rdbuf();
			auto content = ss.str();
			ifs.close();
			return FromJson(content);
		}
		catch (...)
		{
			return false;
		}
	}
}
