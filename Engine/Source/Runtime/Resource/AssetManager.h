#pragma once
#include "TextureRef.h"
#include "Model.h"
#include <string>
#include <functional>
#include <chrono>
#include "Runtime/Function/Framework/Component/SceneComponents.h"

namespace AtomEngine
{
struct ModelData;
enum class AssetKind { Model, Texture, Primitive };
struct AssetRequest {
    AssetKind kind = AssetKind::Model;
    std::string path;
    BuiltinMesh primitive = BuiltinMesh::None;
    eDefaultTexture fallback = kWhiteOpaque2D;
    bool sRGB = false;
};
struct PreloadReport {
    size_t total = 0, completed = 0, succeeded = 0;
    double milliseconds = 0;
    std::vector<std::string> errors;
    bool Ready() const { return completed == total; }
    bool Success() const { return Ready() && errors.empty(); }
};
// Call Step on the engine thread: loading includes GPU resource creation.
// Keep the batch alive until scene activation to pin weak-cached primitives.
class AssetPreloadBatch {
public:
    explicit AssetPreloadBatch(std::vector<AssetRequest> requests);
    const PreloadReport& Step(size_t assetBudget = 1);
    const PreloadReport& Report() const { return mReport; }
private:
    std::vector<AssetRequest> mRequests;
    std::vector<std::shared_ptr<Model>> mModels;
    std::vector<TextureRef> mTextures;
    PreloadReport mReport;
};

	class AssetManager
	{
	public:
		using ModelBuilder = std::shared_ptr<Model>(*)(ModelData&, const std::wstring&);
		static void SetModelBuilder(ModelBuilder builder);
		static void Initialize(const std::wstring& RootPath);

		static void Shutdown();
        static std::shared_ptr<AssetPreloadBatch> Preload(std::vector<AssetRequest> requests);

		static void DestroyTexture(const std::wstring& key);
		static void DestroyModel(const std::wstring& key);
		static TextureRef LoadTextureFile(const std::wstring& filePath, eDefaultTexture fallback = kMagenta2D, bool sRGB = false);
		static TextureRef LoadTextureFile(const std::string& filePath, eDefaultTexture fallback = kMagenta2D, bool sRGB = false);

		static TextureRef LoadCovertTexture(const std::wstring& filePath, eDefaultTexture fallback = kMagenta2D, bool sRGB = false);
        static TextureRef LoadCovertTexture(const std::string& filePath, eDefaultTexture fallback = kMagenta2D, bool sRGB = false);

		static std::shared_ptr<Model> LoadModel(const std::wstring& filePath);
		static std::shared_ptr<Model> LoadModel(const std::string& filePath);

	};
}


