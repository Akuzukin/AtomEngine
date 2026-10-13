#pragma once
#include "ModelLoader.h"
#include "Runtime/Function/Framework/Component/SceneComponents.h"
namespace AtomEngine
{
// CPU geometry; render resources are built by the registered model builder.
class Primitive
{
  public:
    static ModelData Generate(BuiltinMesh type, uint32_t slices = 48, uint32_t stacks = 24);
    static std::shared_ptr<Model> GetModel(BuiltinMesh type);
    static void ClearCache();
};
} // namespace AtomEngine
