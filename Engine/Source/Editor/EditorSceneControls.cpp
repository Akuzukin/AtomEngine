#include "EditorLayer.h"
#include "ScriptTools.h"
#include "ImGuiCommon.h"
#include "EditorTheme.h"
#include "EditorLocalization.h"
#include "CameraGizmos.h"
#include "Runtime/Function/Scene/SceneView.h"
#include "Runtime/Platform/DirectX12/Buffer/BufferManager.h"
#include "Runtime/Platform/DirectX12/Core/DirectX12Core.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/WindowManager.h"
#include "imgui.h"
namespace AtomEngine
{
void EditorLayer::RenderCameraGizmos(Scene &scene)
{
    if (mShowCameraGizmos && !mPlaying)
        CameraGizmos::Draw(scene.GetWorld(), scene.GetCamera(), mSelected);
}



void EditorLayer::ClearThumbnails()
{
    for (auto &[path, thumbnail] : mThumbnails)
        if (!thumbnail.handle.IsNull())
            Renderer::GetTextureHeap().Free(thumbnail.handle);
    mThumbnails.clear();
}
} // namespace AtomEngine
