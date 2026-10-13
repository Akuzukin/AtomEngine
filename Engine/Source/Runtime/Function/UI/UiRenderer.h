#pragma once
#include "Runtime/Function/Framework/ECS/World.h"
#include "Runtime/Platform/DirectX12/Context/GraphicsContext.h"
namespace AtomEngine
{
class UiRenderer
{
  public:
    static void Initialize();
    static void Render(GraphicsContext &context, World &world);
    static void Shutdown();
};
} // namespace AtomEngine
