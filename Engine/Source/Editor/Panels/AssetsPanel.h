#pragma once
#include "imgui.h"
namespace AtomEngine { class EditorLayer; class Scene; class World; struct ScriptAttachment;
// 面板负责自己的 UI；场景命令和共享选择状态由 EditorLayer 协调。
class AssetsPanel { public:
    void Draw(EditorLayer &editor, Scene &s);
private:
    ImGuiTextFilter search;
};
}
