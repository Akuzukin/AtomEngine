#pragma once
#include "EditorLayer.h"
#include "Runtime/Function/Audio/Audio.h"
#include "EditorTheme.h"
#include "Runtime/Function/Global/GlobalContext.h"
#include "EditorLocalization.h"
#include "Runtime/Function/Scene/SceneView.h"
#include "Runtime/Function/Scene/GameProject.h"
#include "imgui.h"
#include "ImGuizmo.h"
#include "Runtime/Core/LogSystem/LogSystem.h"
#include "Runtime/Function/Framework/Component/MeshComponent.h"
#include "Runtime/Function/Framework/ECS/GameObject.h"
#include "Runtime/Function/Input/Input.h"
#include "Runtime/Function/Render/SelectionRenderer.h"
#include "Runtime/Function/Scene/SceneSerializer.h"
#include "Runtime/Platform/DirectX12/Buffer/BufferManager.h"
#include "Runtime/Platform/DirectX12/Core/DirectX12Core.h"
#include "Runtime/Resource/Shader.h"
#include <algorithm>
#include <cstring>
#include "Runtime/Function/Scene/SceneAuthoring.h"
#include "ScriptTools.h"
#include "ImGuiCommon.h"
#include "External/tinyxml2/tinyxml2.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/WindowManager.h"
#include "imgui_internal.h"
#include <set>
#include <commdlg.h>
#include <shellapi.h>
#include "Runtime/Function/Framework/Component/MaterialComponent.h"
#include "CameraGizmos.h"
#include "json.hpp"
#include <limits>
#include <sstream>
#include <iomanip>
#pragma comment(lib,"comdlg32.lib")
#pragma comment(lib,"shell32.lib")
namespace AtomEngine {
	namespace
	{
		bool Text(const char* label, std::string& s, float reserve = 0)
		{
			char buffer[1024]{};
			strncpy_s(buffer, s.c_str(), _TRUNCATE);
			ImGui::TextUnformatted(EditorLocalization::Text(label));
			ImGui::SetNextItemWidth(-1 - reserve);
			if (ImGui::InputText((std::string("##") + label).c_str(), buffer, sizeof(buffer)))
			{
				s = buffer;
				return true;
			}
			return false;
		}
		bool AssetPath(const char* label, std::string& value, const char* extensions)
		{
			bool changed = Text(label, value);
			if (ImGui::BeginDragDropTarget())
			{
				if (auto payload = ImGui::AcceptDragDropPayload("ATOM_ASSET"))
				{
					auto path = static_cast<const char*>(payload->Data);
					auto ext = std::filesystem::path(UTF8ToWString(path)).extension().string();
					std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
					if (std::string(extensions).find("|" + ext + "|") != std::string::npos)
					{
						value = path;
						changed = true;
					}
				}
				ImGui::EndDragDropTarget();
			}
			return changed;
		}
		bool Float(const char* label, float* value, float speed = 1.f, float minimum = 0.f, float maximum = 0.f,
			const char* format = "%.3f", ImGuiSliderFlags flags = 0)
		{
			ImGui::TextUnformatted(EditorLocalization::Text(label));
			ImGui::SetNextItemWidth(-1);
			return ImGui::DragFloat((std::string("##") + label).c_str(), value, speed, minimum, maximum, format, flags);
		}
		bool Float2(const char* label, float* value, float speed = 1.f, float minimum = 0.f, float maximum = 0.f,
			const char* format = "%.3f", ImGuiSliderFlags flags = 0)
		{
			ImGui::TextUnformatted(EditorLocalization::Text(label));
			ImGui::SetNextItemWidth(-1);
			return ImGui::DragFloat2((std::string("##") + label).c_str(), value, speed, minimum, maximum, format, flags);
		}
		bool Float3(const char* label, float* value, float speed = 1.f, float minimum = 0.f, float maximum = 0.f,
			const char* format = "%.3f", ImGuiSliderFlags flags = 0)
		{
			ImGui::TextUnformatted(EditorLocalization::Text(label));
			ImGui::SetNextItemWidth(-1);
			return ImGui::DragFloat3((std::string("##") + label).c_str(), value, speed, minimum, maximum, format, flags);
		}
		template <class... Args> bool Color3(const char* label, Args &&...args)
		{
			ImGui::TextUnformatted(EditorLocalization::Text(label));
			ImGui::SetNextItemWidth(-1);
			return ImGui::ColorEdit3((std::string("##") + label).c_str(), std::forward<Args>(args)...);
		}
		template <class... Args> bool Color4(const char* label, Args &&...args)
		{
			ImGui::TextUnformatted(EditorLocalization::Text(label));
			ImGui::SetNextItemWidth(-1);
			return ImGui::ColorEdit4((std::string("##") + label).c_str(), std::forward<Args>(args)...);
		}
		void Rotation(const char* label, Quaternion& q)
		{
			Matrix4x4 matrix;
			matrix.MakeAffine(Vector3::UNIT_SCALE, q, Vector3::ZERO);
			float position[3], angles[3], scale[3];
			ImGuizmo::DecomposeMatrixToComponents(&matrix[0][0], position, angles, scale);
			if (Float3(label, angles, .5f))
			{
				ImGuizmo::RecomposeMatrixFromComponents(position, angles, scale, &matrix[0][0]);
				q = matrix.GetRotation();
			}
		}
		void TransformEditor(Transform& t)
		{
			Float3("Position", t.transition.ptr(), .05f);
			Rotation("Rotation (degrees)", t.rotation);
			if (ImGui::SmallButton(EditorLocalization::Label("Reset transform")))
			{
				t.transition = Vector3::ZERO;
				t.rotation = Quaternion::IDENTITY;
				t.scale = Vector3::UNIT_SCALE;
			}
			Float3("Scale", t.scale.ptr(), .01f, .001f, 10000, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		}
	} // namespace
namespace
{
std::filesystem::path Preferences()
{
    wchar_t exe[32768]{};
    GetModuleFileNameW(nullptr, exe, DWORD(std::size(exe)));
    return std::filesystem::path(exe).parent_path() / "EditorPreferences.xml";
}
std::filesystem::path SceneDialog(bool save, const char *initial)
{
    wchar_t file[32768]{};
    auto wide = UTF8ToWString(initial);
    if (save)
        wcsncpy_s(file, wide.c_str(), _TRUNCATE);
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = gContext.windowManager->GetWindowHandle();
    dialog.lpstrFilter = L"AtomEngine scene (*.scene)\0*.scene\0\0";
    dialog.lpstrFile = file;
    dialog.nMaxFile = DWORD(std::size(file));
    dialog.lpstrDefExt = L"scene";
    auto title = UTF8ToWString(EditorLocalization::Text(save ? "Save scene" : "Open scene"));
    dialog.lpstrTitle = title.c_str();
    dialog.Flags =
        OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    if (save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog))
        return file;
    if (auto error = CommDlgExtendedError())
        WriteLog(LogLevel::Error, "Editor", "File dialog failed: " + std::to_string(error));
    return {};
}
std::string PathText(const std::filesystem::path &p)
{
    return WStringToUTF8(p.generic_wstring());
}
std::string Extension(const std::filesystem::path &p)
{
    auto s = p.extension().string();
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return s;
}
bool TextureFile(const std::filesystem::path &p)
{
    auto s = Extension(p);
    return s == ".dds" || s == ".png" || s == ".jpg" || s == ".jpeg" || s == ".bmp" || s == ".tga" || s == ".hdr" ||
           s == ".exr";
}
bool ModelFile(const std::filesystem::path &p)
{
    auto s = Extension(p);
    return s == ".obj" || s == ".fbx" || s == ".gltf" || s == ".glb" || s == ".dae" || s == ".ply" || s == ".stl";
}

}

}
