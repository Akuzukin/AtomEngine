#pragma once
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Platform/DirectX12/Core/DirectX12Core.h"
#include "Runtime/Platform/DirectX12/Core/CommandListManager.h"
#include <stdexcept>

inline void RunRuntimeSmokeChecks()
{
    using namespace AtomEngine;
    // A real file with non-DDS contents exercises the decoder's failure path twice.
    auto invalid = AssetManager::LoadTextureFile(L"Asset/Shaders/Common.hlsli");
    auto again = AssetManager::LoadTextureFile(L"Asset/Shaders/Common.hlsli");
    if (invalid.IsValid() || again.IsValid() || invalid.Get() != again.Get())
        throw std::runtime_error("Invalid texture caching regression");
    auto copy = invalid;
    copy = copy;
    auto moved = std::move(copy);
    copy = nullptr;
    if (!moved.Get() || copy.Get()) throw std::runtime_error("Texture ownership regression");

    auto& heap = Renderer::GetTextureHeap();
    auto first = heap.Alloc(2);
    heap.Free(first);
    DX12Core::gCommandManager.IdleGPU();
    auto reused = heap.Alloc(2);
    if (first.GetCpuPtr() != reused.GetCpuPtr())
        throw std::runtime_error("Descriptor retirement/reuse regression");
    heap.Free(reused);
}
