#include "EditorPanelSupport.h"
namespace AtomEngine {
void EditorLayer::Initialize(const std::filesystem::path &path)
{
    EditorLocalization::Initialize(ResolveAssetFile("Asset/Localization"));
    strncpy_s(mScenePath, PathText(path).c_str(), _TRUNCATE);
    LoadPreferences();
    Recent(path);
}
void EditorLayer::Shutdown()
{
    SavePreferences();
    ClearThumbnails();
    mMaterialPreview.reset();
    if (mAssetPreviewHandle.GetCpuPtr() != D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
        Renderer::GetTextureHeap().Free(mAssetPreviewHandle);
    mAssetPreview = nullptr;
}
void EditorLayer::LoadPreferences()
{
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(Preferences().string().c_str()) != tinyxml2::XML_SUCCESS)
        return;
    if (auto root = doc.FirstChildElement("Editor"))
    {
        if (auto language = root->Attribute("language"))
            if (!EditorLocalization::SetLanguage(language))
                EditorLocalization::SetLanguage("en");
        mShowAssets = root->BoolAttribute("assets", true);
        mShowMaterials = root->BoolAttribute("materials", true);
        mShowCameraGizmos = root->BoolAttribute("cameraFrustums", true);
        mThumbnailView = root->BoolAttribute("thumbnails", true);
        mResetLayout = root->IntAttribute("layoutVersion") != 3;
        mMaximizeOnPlay = root->BoolAttribute("maximizeOnPlay", false);
        mShowConsole = root->BoolAttribute("console", true);
        mShowShaders = root->BoolAttribute("shaders", false);
        for (auto n = root->FirstChildElement("Recent"); n && mRecentScenes.size() < 10;
             n = n->NextSiblingElement("Recent"))
            if (auto path = n->Attribute("path"))
                mRecentScenes.emplace_back(UTF8ToWString(path));
    }
}
void EditorLayer::SavePreferences()
{
    // Smoke runs disable the layout file; keep their transient panel/play choices
    // out of the user's saved workspace as well.
    if (ImGui::GetCurrentContext() && !ImGui::GetIO().IniFilename)
        return;
    tinyxml2::XMLDocument doc;
    auto root = doc.NewElement("Editor");
    doc.InsertEndChild(root);
    root->SetAttribute("assets", mShowAssets);
    root->SetAttribute("language", EditorLocalization::CurrentLanguage().c_str());
    root->SetAttribute("materials", mShowMaterials);
    root->SetAttribute("cameraFrustums", mShowCameraGizmos);
    root->SetAttribute("layoutVersion", 3);
    root->SetAttribute("thumbnails", mThumbnailView);
    root->SetAttribute("maximizeOnPlay", mMaximizeOnPlay);
    root->SetAttribute("console", mShowConsole);
    root->SetAttribute("shaders", mShowShaders);
    for (auto &p : mRecentScenes)
    {
        auto recent = doc.NewElement("Recent");
        recent->SetAttribute("path", PathText(p).c_str());
        root->InsertEndChild(recent);
    }
    if (doc.SaveFile(Preferences().string().c_str()) != tinyxml2::XML_SUCCESS)
        WriteLog(LogLevel::Warning, "Editor", "Cannot save editor preferences");
}
void EditorLayer::Recent(const std::filesystem::path &path)
{
    auto absolute = std::filesystem::absolute(path).lexically_normal();
    std::erase(mRecentScenes, absolute);
    mRecentScenes.insert(mRecentScenes.begin(), absolute);
    if (mRecentScenes.size() > 10)
        mRecentScenes.resize(10);
}
}
