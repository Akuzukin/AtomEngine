#pragma once
#include <filesystem>
#include "Runtime/Function/Framework/ECS/World.h"
namespace AtomEngine
{
	class EditorLayer;
	class ParticleComponentEditor
	{
	public:
		static void Open(World& world, Entity entity, EditorLayer* owner = nullptr);
		static void OpenAsset(const std::filesystem::path& path, EditorLayer* owner = nullptr);
		static std::filesystem::path CreateAsset(const std::filesystem::path& directory, EditorLayer* owner = nullptr);
		static void SaveAs(const std::filesystem::path& path);
		static bool HasUnsavedChanges();
		static void Draw();
		static void Render(float dt);
		static void Shutdown();
		static void CapturePreview(const std::filesystem::path& path);
	};
}
