#pragma once
#include <memory>
#include <string>
namespace AtomEngine {
    class Model;
    struct ModelData;
    std::shared_ptr<Model> BuildModelGpuResources(ModelData& data, const std::wstring& path);
}
