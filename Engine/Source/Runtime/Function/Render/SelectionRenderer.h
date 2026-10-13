#pragma once
#include "Runtime/Function/Framework/ECS/World.h"
#include "Runtime/Function/Camera/CameraBase.h"
#include "Runtime/Platform/DirectX12/Context/GraphicsContext.h"
namespace AtomEngine
{
class SelectionRenderer
{
  public:
    static void Initialize();
    static void Shutdown();
    static void Select(World *world, Entity entity);
    static void Render(GraphicsContext &context, World &world, const Camera &camera);
    static void DrawColliders(World &world, const Camera &camera);
    static Entity Pick(World &world, const Camera &camera, float x, float y, float width, float height);
};
} // namespace AtomEngine
