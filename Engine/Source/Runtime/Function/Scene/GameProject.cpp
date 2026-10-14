#include "GameProject.h"
#include <set>
#include <algorithm>
#include <fstream>
#include "Runtime/Resource/AssetArchive.h"
#include "json.hpp"
namespace AtomEngine
{
std::filesystem::path EngineDirectory()
{
    wchar_t exe[32768]{};
    GetModuleFileNameW(nullptr, exe, DWORD(std::size(exe)));
    return std::filesystem::path(exe).parent_path();
}
std::filesystem::path ResolveAssetFile(const std::filesystem::path& path)
{
    if (GameArchiveMounted()) return path.is_absolute() ? path : EngineDirectory() / path;
    if (path.is_absolute() || std::filesystem::exists(path)) return std::filesystem::absolute(path);
    return EngineDirectory() / path;
}
std::string GameProject::RelativePath(const std::filesystem::path& path)
{
    for (const auto& part : path) if (part == L"..") throw std::runtime_error("Project paths cannot leave the project directory");
    auto p = path.lexically_normal();
    if (p.empty() || p.is_absolute() || p.has_root_name()) throw std::runtime_error("Project paths must be relative");
    for (const auto& part : p) if (part == L"..") throw std::runtime_error("Project paths cannot leave the project directory");
    return WStringToUTF8(p.generic_wstring());
}
void GameProject::Validate(bool requireFiles) const
{
    if (file.extension() != L".atproj") throw std::runtime_error("Projects must use .atproj extension");
    if (settings.name.empty() || settings.name.size() > 128) throw std::runtime_error("Invalid project name");
    if (width < 320 || width > 7680 || height < 240 || height > 4320) throw std::runtime_error("Invalid game window size");
    std::set<std::string> unique;
    for (auto& scene : scenes)
    {
        auto p = RelativePath(UTF8ToWString(scene));
        std::transform(p.begin(), p.end(), p.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        if (!unique.insert(p).second) throw std::runtime_error("Duplicate project scene");
        if (std::filesystem::path(UTF8ToWString(scene)).extension() != L".scene" ||
            (requireFiles && !std::filesystem::is_regular_file(Root() / UTF8ToWString(scene))))
            throw std::runtime_error("Missing project scene: " + scene);
    }
    if (scenes.empty() || std::find(scenes.begin(), scenes.end(), settings.startupScene) == scenes.end())
        throw std::runtime_error("Startup scene must be included in the project scene list");
    if (!scriptProject.empty()) RelativePath(UTF8ToWString(scriptProject));
    for (auto& asset : additionalAssets) RelativePath(UTF8ToWString(asset));
}
GameProject GameProject::Load(const std::filesystem::path& path)
{
    GameProject value; value.file = std::filesystem::absolute(path).lexically_normal();
    tinyxml2::XMLDocument doc;
    std::ifstream stream(value.file, std::ios::binary);
    std::string xml((std::istreambuf_iterator<char>(stream)), {});
    if (!stream || doc.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS) throw std::runtime_error("Cannot read .atproj file");
    auto root = doc.FirstChildElement("AtomProject");
    if (!root || root->IntAttribute("version") != 1) throw std::runtime_error("Unsupported .atproj version");
    if (auto text = root->Attribute("name")) value.settings.name = text;
    if (auto text = root->Attribute("startupScene")) value.settings.startupScene = text;
    if (auto text = root->Attribute("scriptProject")) value.scriptProject = text;
    if (auto text = root->Attribute("buildOutput")) value.buildOutput = text;
    value.settings.maximizeOnPlay = root->BoolAttribute("maximizeOnPlay");
    value.width = root->IntAttribute("width", 1280); value.height = root->IntAttribute("height", 720);
    value.fullscreen = root->BoolAttribute("fullscreen");
    if (auto list = root->FirstChildElement("Scenes"))
        for (auto n = list->FirstChildElement("Scene"); n; n = n->NextSiblingElement("Scene"))
            if (auto p = n->Attribute("path")) value.scenes.push_back(p);
    if (auto list = root->FirstChildElement("Include"))
        for (auto n = list->FirstChildElement("Asset"); n; n = n->NextSiblingElement("Asset"))
            if (auto p = n->Attribute("path")) value.additionalAssets.push_back(p);
    if (auto scene = root->FirstChildElement("Scene"))
    {
        if (scene->FirstChildElement("Object")) throw std::runtime_error("Project defaults cannot contain scene objects");
        tinyxml2::XMLPrinter printer; scene->Accept(&printer); World temporary;
        SceneSerializer::FromXml(temporary, printer.CStr()); value.settings.defaults = temporary.settings;
    }
    value.Validate(); return value;
}
void GameProject::Save() const
{
    Validate();
    World temporary; temporary.settings = settings.defaults;
    auto xml = SceneSerializer::ToXml(temporary, "Project defaults");
    World validation; SceneSerializer::FromXml(validation, xml);
    tinyxml2::XMLDocument doc, defaults; defaults.Parse(xml.c_str());
    auto root = doc.NewElement("AtomProject"); doc.InsertEndChild(root);
    root->SetAttribute("version", 1); root->SetAttribute("name", settings.name.c_str());
    root->SetAttribute("startupScene", settings.startupScene.c_str()); root->SetAttribute("scriptProject", scriptProject.c_str());
    root->SetAttribute("buildOutput", buildOutput.c_str());
    root->SetAttribute("width", width); root->SetAttribute("height", height); root->SetAttribute("fullscreen", fullscreen);
    root->SetAttribute("maximizeOnPlay", settings.maximizeOnPlay);
    auto list = doc.NewElement("Scenes"); root->InsertEndChild(list);
    for (auto& path : scenes) { auto n = doc.NewElement("Scene"); n->SetAttribute("path", path.c_str()); list->InsertEndChild(n); }
    list = doc.NewElement("Include"); root->InsertEndChild(list);
    for (auto& path : additionalAssets) { auto n = doc.NewElement("Asset"); n->SetAttribute("path", path.c_str()); list->InsertEndChild(n); }
    root->InsertEndChild(defaults.RootElement()->DeepClone(&doc));
    std::filesystem::create_directories(Root()); auto pending = file; pending += L".tmp";
    tinyxml2::XMLPrinter printer; doc.Print(&printer);
    { std::ofstream stream(pending, std::ios::binary); stream << printer.CStr(); stream.close(); if (!stream) throw std::runtime_error("Cannot write project file"); }
    if (!MoveFileExW(pending.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot replace project file");
}
std::string GameProject::FindScene(const std::string& nameOrPath) const
{
    for (auto& path : scenes) if (path == nameOrPath) return path;
    std::string match;
    for (auto& path : scenes) if (WStringToUTF8(std::filesystem::path(UTF8ToWString(path)).stem().wstring()) == nameOrPath)
    { if (!match.empty()) throw std::runtime_error("Ambiguous scene name; use the full project path"); match = path; }
    if (match.empty()) throw std::runtime_error("Scene is not in the project: " + nameOrPath);
    return match;
}
void ProjectSession::RequestScene(const std::string& nameOrPath)
{
    if (!project && !game) throw std::runtime_error("No active scene catalog");
    std::string path;
    for (auto& scene : ScenePaths()) if (scene == nameOrPath) { path = scene; break; }
    if (path.empty()) for (auto& scene : ScenePaths()) if (WStringToUTF8(std::filesystem::path(UTF8ToWString(scene)).stem().wstring()) == nameOrPath)
    { if (!path.empty()) throw std::runtime_error("Ambiguous scene name; use the full scene path"); path = scene; }
    if (path.empty()) throw std::runtime_error("Scene is not in the game scene catalog: " + nameOrPath);
    if (!AssetFileExists(SceneRoot() / UTF8ToWString(path))) throw std::runtime_error("Scene file is missing");
    pendingScene = path;
    lastLoadError.clear();
}
const std::vector<std::string>& ProjectSession::ScenePaths() const
{
    static const std::vector<std::string> empty;
    return project ? project->scenes : game ? game->scenes : empty;
}
std::filesystem::path ProjectSession::SceneRoot() const { return project ? project->Root() : EngineDirectory(); }
GameLaunchSettings GameLaunchSettings::Load()
{
    auto data = nlohmann::json::parse(ReadAssetText("__runtime/startup.json"));
    if (data.at("version") != 1) throw std::runtime_error("Unsupported game launch data");
    GameLaunchSettings settings;
    settings.name = data.at("name").get<std::string>(); settings.startupScene = data.at("startupScene").get<std::string>();
    settings.scenes = data.at("scenes").get<std::vector<std::string>>(); settings.width = data.at("width"); settings.height = data.at("height"); settings.fullscreen = data.at("fullscreen");
    if (settings.name.empty() || settings.width < 320 || settings.width > 7680 || settings.height < 240 || settings.height > 4320 || settings.scenes.empty() || settings.scenes.size() > 100000 || std::find(settings.scenes.begin(),settings.scenes.end(),settings.startupScene) == settings.scenes.end()) throw std::runtime_error("Invalid game launch data");
    std::set<std::string> unique;
    for (auto& path : settings.scenes) if (GameProject::RelativePath(UTF8ToWString(path)) != path || !unique.insert(path).second || !AssetFileExists(UTF8ToWString(path))) throw std::runtime_error("Invalid packaged scene catalog");
    auto defaults = data.at("defaults").get<std::string>();
    tinyxml2::XMLDocument document;
    if (document.Parse(defaults.c_str()) != tinyxml2::XML_SUCCESS || !document.FirstChildElement("Scene") || document.FirstChildElement("Scene")->FirstChildElement("Object")) throw std::runtime_error("Invalid game defaults");
    World temporary; SceneSerializer::FromXml(temporary, defaults); settings.defaults = temporary.settings;
    return settings;
}
}
