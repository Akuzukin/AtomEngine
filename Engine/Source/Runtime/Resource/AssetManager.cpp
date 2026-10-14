#include "AssetManager.h"
#include "Runtime/Function/Scene/GameProject.h"
#include "ModelLoader.h"
#include "AssetArchive.h"
#include "Primitive.h"
#include "Runtime/Core/LogSystem/LogSystem.h"
#include <set>

#include <map>
#include <mutex>
#include <filesystem>
#include <stdexcept>
#include "Runtime/Core/Utility/Utility.h"

namespace
{
AtomEngine::AssetManager::ModelBuilder sModelBuilder = nullptr;
std::wstring sRootPath = L"";
std::map<std::wstring, std::shared_ptr<AtomEngine::ManagedTexture>> sTextureCache;
std::map<std::wstring, std::shared_ptr<AtomEngine::Model>> sModelDataCache;
std::unordered_map<uint32_t, uint32_t> gSamplerPermutations;

std::mutex sTextureMutex;
std::mutex sModelMutex;
std::mutex sPreparationMutex;
} // namespace

namespace AtomEngine
{
struct AssetPreloadBatch::Prepared
{
    std::wstring path;
    std::unique_ptr<ModelData> model;
    std::shared_ptr<std::vector<byte>> texture;
    std::string error;
};
AssetPreloadBatch::~AssetPreloadBatch() = default;
AssetPreloadBatch::AssetPreloadBatch(std::vector<AssetRequest> requests, bool asynchronous)
    : mAsynchronous(asynchronous)
{
    std::set<std::string> keys;
    for (auto &request : requests)
    {
        auto path = request.path.empty() ? std::string()
                                         : WStringToUTF8(ResolveAssetFile(std::filesystem::path(sRootPath) /
                                                                                   UTF8ToWString(request.path))
                                                             .lexically_normal()
                                                             .wstring());
        auto key = std::to_string(int(request.kind)) + "|" + path + "|" + std::to_string(int(request.primitive)) + "|" +
                   std::to_string(int(request.fallback)) + "|" + std::to_string(request.sRGB);
        if (keys.insert(key).second)
            mRequests.push_back(std::move(request));
    }
    mReport.total = mRequests.size();
}
const PreloadReport &AssetPreloadBatch::Step(size_t budget)
{
    auto start = std::chrono::steady_clock::now();
    while (budget-- && !mReport.Ready())
    {
        const auto &r = mRequests[mReport.completed];
        auto label = r.kind == AssetKind::Primitive ? "Primitive " + std::to_string(int(r.primitive)) : r.path;
        try
        {
            std::shared_ptr<Prepared> prepared;
            if (mAsynchronous && r.kind != AssetKind::Primitive)
            {
                if (!mPending.valid())
                {
                    auto root = sRootPath;
                    mPending = std::async(std::launch::async, [r, root] {
                        auto p = std::make_shared<Prepared>();
                        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
                        try
                        {
                            p->path = ResolveAssetFile(std::filesystem::path(root) / UTF8ToWString(r.path))
                                          .lexically_normal()
                                          .wstring();
                            if (r.kind == AssetKind::Model)
                            {
                                p->model = std::make_unique<ModelData>();
                                if (!ModelLoader::LoadModel(*p->model, WStringToUTF8(p->path)))
                                    throw std::runtime_error("Model CPU import failed");
                            }
                            else
                            {
                                std::lock_guard prepareGuard(sPreparationMutex);
                                if (!GameArchiveMounted()) { CompileTextureOnDemand(p->path, TextureOptions(r.sRGB)); p->path = RemoveExtension(p->path) + L".dds"; }
                                p->texture = ReadFileSync(p->path);
                            }
                        }
                        catch (const std::exception &e)
                        {
                            p->error = e.what();
                        }
                        CoUninitialize();
                        return p;
                    });
                    break;
                }
                if (mPending.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
                    break;
                prepared = mPending.get();
                if (!prepared->error.empty())
                    throw std::runtime_error(prepared->error);
            }
            if (r.kind == AssetKind::Texture)
            {
                TextureRef texture;
                if (prepared)
                {
                    auto key = prepared->path + (r.sRGB ? L"_sRGB" : L"_linear") + std::to_wstring(int(r.fallback));
                    std::lock_guard guard(sTextureMutex);
                    auto found = sTextureCache.find(key);
                    if (found == sTextureCache.end())
                    {
                        auto tex = std::shared_ptr<ManagedTexture>(new ManagedTexture(key));
                        tex->mSourcePath = prepared->path;
                        tex->CreateFromMemory(prepared->texture, r.fallback, r.sRGB);
                        found = sTextureCache.emplace(key, std::move(tex)).first;
                    }
                    texture = found->second;
                }
                else
                    texture = AssetManager::LoadCovertTexture(r.path, r.fallback, r.sRGB);
                if (!texture.IsValid())
                    throw std::runtime_error("Texture could not be loaded");
                mTextures.push_back(std::move(texture));
            }
            else
            {
                std::shared_ptr<Model> model;
                if (prepared)
                {
                    std::lock_guard guard(sModelMutex);
                    auto found = sModelDataCache.find(prepared->path);
                    if (found == sModelDataCache.end())
                    {
                        if (!sModelBuilder)
                            throw std::runtime_error("No model GPU builder registered");
                        found = sModelDataCache.emplace(prepared->path, sModelBuilder(*prepared->model, prepared->path))
                                    .first;
                    }
                    model = found->second;
                }
                else
                    model = r.kind == AssetKind::Primitive ? Primitive::GetModel(r.primitive)
                                                           : AssetManager::LoadModel(r.path);
                if (!model)
                    throw std::runtime_error("Mesh could not be loaded");
                mModels.push_back(std::move(model));
            }
            ++mReport.succeeded;
            WriteLog(LogLevel::Debug, "Assets", "Preloaded " + label);
        }
        catch (const std::exception &e)
        {
            mReport.errors.push_back(label + ": " + e.what());
            WriteLog(LogLevel::Error, "Assets", mReport.errors.back());
        }
        ++mReport.completed;
    }
    mReport.milliseconds += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return mReport;
}
std::shared_ptr<AssetPreloadBatch> AssetManager::Preload(std::vector<AssetRequest> requests)
{
    auto batch = std::make_shared<AssetPreloadBatch>(std::move(requests));
    batch->Step(batch->Report().total);
    WriteLog(batch->Report().Success() ? LogLevel::Info : LogLevel::Warning, "Assets",
             "Preload " + std::to_string(batch->Report().succeeded) + "/" + std::to_string(batch->Report().total) +
                 " assets in " + std::to_string(batch->Report().milliseconds) + " ms");
    return batch;
}
std::shared_ptr<AssetPreloadBatch> AssetManager::PreloadAsync(std::vector<AssetRequest> requests)
{
    return std::make_shared<AssetPreloadBatch>(std::move(requests), true);
}

void AssetManager::SetModelBuilder(ModelBuilder builder)
{
    sModelBuilder = builder;
}

void AssetManager::Initialize(const std::wstring &rootPath)
{
    sRootPath = rootPath;
}

void AssetManager::Shutdown()
{
    std::scoped_lock guard(sTextureMutex, sModelMutex);
    sModelDataCache.clear();
    sTextureCache.clear();
}

void AssetManager::DestroyTexture(const std::wstring &key)
{
    std::lock_guard<std::mutex> Guard(sTextureMutex);

    auto iter = sTextureCache.find(key);
    if (iter != sTextureCache.end())
        sTextureCache.erase(iter);
}

void AssetManager::DestroyModel(const std::wstring &key)
{
    std::lock_guard<std::mutex> Guard(sModelMutex);
    auto iter = sModelDataCache.find(std::filesystem::absolute(key).lexically_normal().wstring());
    if (iter != sModelDataCache.end())
        sModelDataCache.erase(iter);
}

std::shared_ptr<ManagedTexture> FindOrLoadTexture(const std::wstring &fileName, eDefaultTexture fallback,
                                                  bool forceSRGB)
{
    const auto path =
        ResolveAssetFile(std::filesystem::path(sRootPath) / fileName).lexically_normal().wstring();
    const auto key = path + (forceSRGB ? L"_sRGB" : L"_linear") + std::to_wstring(static_cast<int>(fallback));
    std::lock_guard<std::mutex> guard(sTextureMutex);
    auto it = sTextureCache.find(key);
    if (it != sTextureCache.end())
        return it->second;
    auto tex = std::shared_ptr<ManagedTexture>(new ManagedTexture(key));
    tex->mSourcePath = path;
    tex->CreateFromMemory(ReadFileSync(path), fallback, forceSRGB);
    sTextureCache.emplace(key, tex);
    if (!tex->IsValid())
        WriteLog(LogLevel::Warning, "Assets", "Texture load failed; using fallback: " + WStringToUTF8(path));
    return tex;
}

TextureRef AssetManager::LoadTextureFile(const std::wstring &filePath, eDefaultTexture fallback, bool forceSRGB)
{
    return FindOrLoadTexture(filePath, fallback, forceSRGB);
}

TextureRef AssetManager::LoadTextureFile(const std::string &filePath, eDefaultTexture fallback, bool forceSRGB)
{
    return LoadTextureFile(UTF8ToWString(filePath), fallback, forceSRGB);
}

TextureRef AssetManager::LoadCovertTexture(const std::wstring &filePath, eDefaultTexture fallback, bool forceSRGB)
{
    std::wstring originalFile =
        ResolveAssetFile(std::filesystem::path(sRootPath) / filePath).lexically_normal().wstring();
    if (GameArchiveMounted()) return FindOrLoadTexture(originalFile, fallback, forceSRGB);
    {
        std::lock_guard prepareGuard(sPreparationMutex);
        CompileTextureOnDemand(originalFile, TextureOptions(forceSRGB));
    }

    std::wstring ddsFile = RemoveExtension(originalFile) + L".dds";
    return FindOrLoadTexture(ddsFile, fallback, forceSRGB);
}

TextureRef AssetManager::LoadCovertTexture(const std::string &filePath, eDefaultTexture fallback, bool forceSRGB)
{
    return LoadCovertTexture(UTF8ToWString(filePath), fallback, forceSRGB);
}

std::shared_ptr<Model> AtomEngine::AssetManager::LoadModel(const std::wstring &filePath)
{
    return LoadModel(WStringToUTF8(filePath));
}

std::shared_ptr<Model> AssetManager::LoadModel(const std::string &filePath)
{
    std::lock_guard<std::mutex> guard(sModelMutex);

    std::wstring filePathW = ResolveAssetFile(std::filesystem::path(sRootPath) / UTF8ToWString(filePath))
                                 .lexically_normal()
                                 .wstring();

    auto it = sModelDataCache.find(filePathW);
    if (it != sModelDataCache.end())
        return it->second;

    std::shared_ptr<Model> model;

    ModelData modelData;

    if (!ModelLoader::LoadModel(modelData, WStringToUTF8(filePathW)))
    {
        WriteLog(LogLevel::Error, "Assets", "Failed to load model: " + filePath);
        return nullptr;
    }

    if (!sModelBuilder)
        throw std::runtime_error("No model GPU builder registered");
    model = sModelBuilder(modelData, filePathW);

    sModelDataCache[filePathW] = model;
    return model;
}
} // namespace AtomEngine
