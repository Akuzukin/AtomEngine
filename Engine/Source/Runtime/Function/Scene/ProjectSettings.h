#pragma once
#include "SceneSerializer.h"
#include "External/tinyxml2/tinyxml2.h"
#include "Runtime/Core/Utility/Utility.h"
#include <Windows.h>
namespace AtomEngine {
// 项目默认值与场景覆盖独立；验证完成后才替换当前项目默认值。
struct ProjectSettings {
    std::string name = "AtomEngine Project", startupScene;
    SceneSettings defaults;
    bool maximizeOnPlay = false;
    static ProjectSettings Load(const std::filesystem::path &path = "Asset/ProjectSettings.xml") {
        ProjectSettings value;
        if (!std::filesystem::exists(path)) return value;
        tinyxml2::XMLDocument doc;
        if(doc.LoadFile(path.string().c_str())!=tinyxml2::XML_SUCCESS) throw std::runtime_error("Cannot read project settings");
        auto root=doc.FirstChildElement("Project");
        if(!root || root->IntAttribute("version")!=1) throw std::runtime_error("Invalid project settings version");
        if(auto text=root->Attribute("name")) value.name=text;
        if(auto text=root->Attribute("startupScene")) value.startupScene=text;
        value.maximizeOnPlay=root->BoolAttribute("maximizeOnPlay",false);
        auto scene=root->FirstChildElement("Scene");if(!scene)throw std::runtime_error("Missing project defaults");
        tinyxml2::XMLPrinter printer;scene->Accept(&printer);World temporary;
        SceneSerializer::FromXml(temporary,printer.CStr());value.defaults=temporary.settings;
        return value;
    }
    void Save(const std::filesystem::path &path = "Asset/ProjectSettings.xml") const {
        if(name.empty() || name.size()>128)throw std::runtime_error("Project name must contain 1 to 128 bytes");
        if(!startupScene.empty() && (!std::filesystem::is_regular_file(UTF8ToWString(startupScene)) || std::filesystem::path(UTF8ToWString(startupScene)).extension()!=L".scene"))throw std::runtime_error("Startup scene must reference an existing .scene file");
        World temporary;temporary.settings=defaults;auto xml=SceneSerializer::ToXml(temporary,"Project defaults");World validation;SceneSerializer::FromXml(validation,xml);
        tinyxml2::XMLDocument doc,scene;scene.Parse(xml.c_str());auto root=doc.NewElement("Project");doc.InsertEndChild(root);
        root->SetAttribute("version",1);root->SetAttribute("name",name.c_str());root->SetAttribute("startupScene",startupScene.c_str());root->SetAttribute("maximizeOnPlay",maximizeOnPlay);
        root->InsertEndChild(scene.RootElement()->DeepClone(&doc));auto pending=path;pending+=L".tmp";
        if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path());
        if(doc.SaveFile(pending.string().c_str())!=tinyxml2::XML_SUCCESS || !MoveFileExW(pending.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot save project settings");
    }
};
}
