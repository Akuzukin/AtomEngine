#include "EditorPanelSupport.h"
namespace AtomEngine {
void EditorLayer::PollBackgroundWork(Scene &scene)
{
    mPrefabWatchTimer += ImGui::GetIO().DeltaTime;
    if (!mPlaying && !mGizmoActive && !ImGui::GetIO().WantTextInput && mPrefabWatchTimer >= 1)
    {
        mPrefabWatchTimer = 0;
        std::set<std::filesystem::path> sources;
        for (auto e : scene.GetWorld().View<PrefabInstanceComponent>())
        {
            const auto &p = scene.GetWorld().GetComponent<PrefabInstanceComponent>(e);
            if (!p.source.empty() && !p.baseline.empty())
                sources.insert(std::filesystem::absolute(p.source).lexically_normal());
        }
        for (const auto &source : sources)
        {
            std::error_code ec;
            auto stamp = std::filesystem::last_write_time(source, ec);
            if (ec)
                continue;
            auto [it, inserted] = mPrefabStamps.emplace(source, stamp);
            if (!inserted && it->second != stamp)
            {
                it->second = stamp;
                Run([&] {
                    auto id = EditorActions::Id(scene.GetWorld(), mSelected);
                    SceneSerializer::RefreshPrefabs(scene.GetWorld(), source);
                    Select(scene.GetWorld(), EditorActions::Find(scene.GetWorld(), id));
                    mHistoryPending = true;
                });
            }
        }
    }
    if (mScriptBuild.valid() && mScriptBuild.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    {
        int code = mScriptBuild.get();
        WriteLog(code ? LogLevel::Error : LogLevel::Info, "Scripts",
                 code ? "Script build failed; see Scripts/ScriptBuild.log. Running assembly retained."
                      : "Scripts built. Active assemblies reload automatically while playing.");
    }
    if (!mScenePreload)
        return;
    auto report = mScenePreload->Step(1);
    ImGui::Begin(EditorLocalization::Label("Loading scene"));
    ImGui::Text(EditorLocalization::Text("Assets: %zu / %zu"), report.completed, report.total);
    ImGui::ProgressBar(report.total ? float(report.completed) / report.total : 1);
    ImGui::End();
    if (!report.Ready())
        return;
    if (!report.Success())
    {
        mError = report.errors.front();
        mScenePreload.reset();
        return;
    }
    auto preload = std::move(mScenePreload);
    if (SceneSerializer::ToXml(scene.GetWorld(), scene.GetName()) != mPreloadSnapshot)
    {
        mError = "Scene changed while loading. Save your changes and open the scene again; resources are now cached.";
        return;
    }
    Run([&] {
        scene.Load(mPreloadPath);
        Select(scene.GetWorld(), entt::null);
        strncpy_s(mScenePath, WStringToUTF8(mPreloadPath.wstring()).c_str(), _TRUNCATE);
        Recent(mPreloadPath);
        if (ActiveProject.project) ActiveProject.currentScene = WStringToUTF8(std::filesystem::absolute(mPreloadPath).lexically_relative(ActiveProject.project->Root()).generic_wstring());
        mDocument.Reset(scene.GetWorld(), scene.GetName());
        mDocumentReady = true;
    });
}
}
