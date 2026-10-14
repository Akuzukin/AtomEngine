#pragma once
#include "json.hpp"
#include <set>
#include <limits>
#include <sstream>
#include <iomanip>
namespace AtomEngine {
inline void EditorLayer::ValidateScripts(World &world) {
    std::set<std::string> checked;
    for(auto e:world.View<ScriptComponent>()) for(auto &s:world.GetComponent<ScriptComponent>(e).scripts)
        if(s.enabled && checked.insert(nlohmann::json{{"assembly",s.assembly},{"type",s.type},{"fields",s.fields}}.dump()).second && !mScriptInspector.ValidateFields(s)) throw std::runtime_error("Script "+s.type+" ("+world.GetComponent<PersistentIdComponent>(e).value+"): "+mScriptInspector.Error());
}

}
