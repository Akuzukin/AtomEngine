#pragma once
#include "Editor/EditorLocalization.h"

inline void RunLocalizationSmokeChecks()
{
    using namespace AtomEngine::EditorLocalization;
    FeatureCheck(SetLanguage("zh-CN"), "Chinese catalog was not loaded");
    FeatureCheck(std::string(Text("File")) == "文件", "Chinese menu translation failed");
    auto viewportId = ImHashStr(Label("Viewport"));
    FeatureCheck(viewportId == ImHashStr("Viewport"), "Translated window changed its persistent ID");
    FeatureCheck(ImHashStr(Label("Step##SimulationStep")) != ImHashStr(Label("Step##TransformSnapStep")),
                 "Translated Step controls have conflicting IDs");
    FeatureCheck(std::string(Text("Unregistered localization key")) == "Unregistered localization key",
                 "Missing translation did not fall back to English");
    FeatureCheck(!SetLanguage("unknown-language") && CurrentLanguage() == "zh-CN",
                 "Unknown language changed the active catalog");
    auto chineseLabel = Label("Viewport");
    FeatureCheck(SetLanguage("en") && std::string(Text("File")) == "File", "English fallback failed");
    FeatureCheck(ImHashStr(Label("Viewport")) == viewportId, "Language switch changed viewport ID");
    FeatureCheck(std::string(chineseLabel) == "视口###Viewport", "Language switch invalidated cached label");
    SetLanguage("zh-CN");
    AtomEngine::WriteLog(AtomEngine::LogLevel::Info, "Smoke",
                         "Localization catalogs, English fallback, language switching and stable ImGui IDs passed");
}
