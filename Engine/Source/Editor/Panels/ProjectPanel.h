#pragma once
#include <filesystem>
#include <future>
namespace AtomEngine
{
class EditorLayer; class Scene;
class ProjectPanel
{
public:
    bool visible = false;
    void Draw(EditorLayer& editor, Scene& scene);
    void Activate(EditorLayer& editor, Scene& scene, const std::filesystem::path& path, bool create);
    bool Save(EditorLayer& editor, Scene& scene);
    bool Busy() const { return build.valid(); }
    void StartBuild(EditorLayer& editor, Scene& scene, bool publish);
    static std::filesystem::path Dialog(bool save);
private:
    std::future<int> build;
    std::string message;
    char scenePath[1024]{}, includePath[1024]{};
};
}
