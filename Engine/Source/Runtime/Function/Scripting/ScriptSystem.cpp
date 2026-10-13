#include "ScriptSystem.h"
#include "Runtime/Function/Framework/Component/SceneComponents.h"
#include "Runtime/Function/Framework/Component/TransformComponent.h"
#include "Runtime/Function/Input/Input.h"
#include "External/dotnet/nethost.h"
#include "External/dotnet/hostfxr.h"
#include "External/dotnet/coreclr_delegates.h"
#include "json.hpp"
#include <Windows.h>
#include <filesystem>
#include <set>
#include <algorithm>
#include <cmath>
#include <atomic>
namespace AtomEngine
{
using Json = nlohmann::json;
struct ScriptSystem::Impl
{
    struct NativeTransform
    {
        float position[3], rotation[4], scale[3];
    };
    struct Context
    {
        World *world = nullptr;
        std::string error;
    };
    struct NativeApi
    {
        uint32_t version = 3, size = sizeof(NativeApi);
        int(__cdecl *get)(void *, uint32_t, NativeTransform *);
        int(__cdecl *set)(void *, uint32_t, NativeTransform *);
        int(__cdecl *key)(int);
        void(__cdecl *log)(void *, const char *);
        int(__cdecl *velocity)(void *, uint32_t, float *, int);
        int(__cdecl *button)(int, int);
        float(__cdecl *axis)(int);
        int(__cdecl *uiText)(void *, uint32_t, char *, int, int);
    };
    using Dispatch = int(__cdecl *)(const char *, const NativeApi *, void *);
    Dispatch dispatch = nullptr;
    HMODULE nethost = nullptr, hostfxr = nullptr;
    Context context;
    NativeApi api;
    struct Instance
    {
        uint64_t id;
        std::string signature;
        bool failed = false;
        std::filesystem::path assembly;
        bool created = true;
    };
    std::map<std::pair<Entity, size_t>, Instance> instances;
    struct WatchedAssembly
    {
        std::filesystem::file_time_type loaded{}, pending{};
    };
    std::map<std::filesystem::path, WatchedAssembly> assemblies;
    float reloadTimer = 0;
    std::weak_ptr<void> lifetime;
    static inline std::atomic<uint64_t> nextId = 0;
    float accumulator = 0;
    Impl()
    {
        api.get = [](void *p, uint32_t e, NativeTransform *out) -> int {
            auto *w = static_cast<Context *>(p)->world;
            if (!w || !w->IsValid(Entity(e)))
                return 0;
            auto t = w->TryGet<TransformComponent>(Entity(e));
            if (!t)
                return 0;
            std::copy_n(t->transition.ptr(), 3, out->position);
            std::copy_n(t->rotation.ptr(), 4, out->rotation);
            std::copy_n(t->scale.ptr(), 3, out->scale);
            return 1;
        };
        api.set = [](void *p, uint32_t e, NativeTransform *in) -> int {
            auto *w = static_cast<Context *>(p)->world;
            if (!w || !w->IsValid(Entity(e)))
                return 0;
            auto t = w->TryGet<TransformComponent>(Entity(e));
            if (!t)
                return 0;
            for (float v : in->position)
                if (!std::isfinite(v))
                    return 0;
            for (float v : in->rotation)
                if (!std::isfinite(v))
                    return 0;
            for (float v : in->scale)
                if (!std::isfinite(v) || v <= 0)
                    return 0;
            Quaternion q(in->rotation[0], in->rotation[1], in->rotation[2], in->rotation[3]);
            float l = std::sqrt(Quaternion::Dot(q, q));
            if (l < 1e-6f)
                return 0;
            t->transition = Vector3(in->position[0], in->position[1], in->position[2]);
            t->rotation = q / l;
            t->scale = Vector3(in->scale[0], in->scale[1], in->scale[2]);
            return 1;
        };
        api.key = [](int k) -> int { return k >= 0 && k < 256 && Input::GetInstance()->IsPressKey(uint8_t(k)); };
        api.log = [](void *p, const char *s) {
            static_cast<Context *>(p)->error = s;
            OutputDebugStringA(s);
        };
        api.velocity = [](void *p, uint32_t e, float *v, int write) -> int {
            auto *w = static_cast<Context *>(p)->world;
            if (!w || !w->IsValid(Entity(e)))
                return 0;
            auto *b = w->TryGet<RigidBodyComponent>(Entity(e));
            if (!b)
                return 0;
            if (write)
            {
                for (int n = 0; n < 3; ++n)
                    if (!std::isfinite(v[n]))
                        return 0;
                b->linearVelocity = Vector3(v[0], v[1], v[2]);
            }
            else
                std::copy_n(b->linearVelocity.ptr(), 3, v);
            return 1;
        };
        api.button = [](int device, int index) -> int {
            auto *input = Input::GetInstance();
            if (device == 0)
                return input->IsPressMouse(index);
            if (device == 1)
                return index >= 0 && index < COUNT && input->IsPressGamePad(GamePadButton(index));
            if (device == 2)
                return index >= 0 && index < 256 && input->IsTriggerKey(uint8_t(index));
            return 0;
        };
        api.axis = [](int axis) -> float {
            auto &s = Input::GetInstance()->GetGamePadStick();
            switch (axis)
            {
            case 0:
                return s.leftStickX;
            case 1:
                return s.leftStickY;
            case 2:
                return s.rightStickX;
            case 3:
                return s.rightStickY;
            case 4:
                return s.leftTrigger;
            case 5:
                return s.rightTrigger;
            default:
                return 0;
            }
        };
        api.uiText = [](void *p, uint32_t e, char *text, int capacity, int write) -> int {
            auto *w = static_cast<Context *>(p)->world;
            if (!w || !w->IsValid(Entity(e)))
                return 0;
            auto *c = w->TryGet<UiElementComponent>(Entity(e));
            if (!c)
                return 0;
            if (write)
            {
                if (capacity < 0 || capacity > 1048576 || (capacity > 0 && !text))
                    return 0;
                if (capacity == 0)
                    c->text.clear();
                else
                    c->text.assign(text, size_t(capacity));
                return 1;
            }
            int count = int(c->text.size()) + 1;
            if (text && capacity >= count)
                std::copy_n(c->text.c_str(), count, text);
            return count;
        };
    }
    bool Call(Json j)
    {
        if (!dispatch)
            return false;
        auto text = j.dump();
        return dispatch(text.c_str(), &api, &context) == 0;
    }
    ~Impl()
    { /* hostfxr and nethost stay resident while CoreCLR is running. */
    }
};
ScriptSystem::ScriptSystem() : mImpl(std::make_unique<Impl>())
{
}
ScriptSystem::~ScriptSystem()
{
    Reset();
}
const std::string &ScriptSystem::Error() const
{
    return mImpl->context.error;
}
bool ScriptSystem::Initialize()
{
    auto &i = *mImpl;
    if (i.dispatch)
        return true;
    try
    {
        wchar_t exe[MAX_PATH];
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        auto root = std::filesystem::path(exe).parent_path();
        i.nethost = LoadLibraryW((root / L"nethost.dll").c_str());
        if (!i.nethost)
            throw std::runtime_error("Missing nethost.dll; build managed scripts first");
        auto locate = reinterpret_cast<decltype(&get_hostfxr_path)>(GetProcAddress(i.nethost, "get_hostfxr_path"));
        if (!locate)
            throw std::runtime_error("Invalid nethost library");
        wchar_t hostPath[4096];
        size_t count = 4096;
        if (locate(hostPath, &count, nullptr) != 0)
            throw std::runtime_error(".NET 10 runtime not installed");
        i.hostfxr = LoadLibraryW(hostPath);
        if (!i.hostfxr)
            throw std::runtime_error("Cannot load hostfxr");
        auto init = reinterpret_cast<hostfxr_initialize_for_runtime_config_fn>(
            GetProcAddress(i.hostfxr, "hostfxr_initialize_for_runtime_config"));
        auto delegate = reinterpret_cast<hostfxr_get_runtime_delegate_fn>(
            GetProcAddress(i.hostfxr, "hostfxr_get_runtime_delegate"));
        auto close = reinterpret_cast<hostfxr_close_fn>(GetProcAddress(i.hostfxr, "hostfxr_close"));
        if (!init || !delegate || !close)
            throw std::runtime_error("Invalid hostfxr exports");
        hostfxr_handle handle = nullptr;
        auto config = root / L"Scripts/Atom.Managed.runtimeconfig.json";
        int result = init(config.c_str(), nullptr, &handle);
        if (result < 0 || !handle)
            throw std::runtime_error("Cannot initialize managed runtime");
        load_assembly_and_get_function_pointer_fn load = nullptr;
        result = delegate(handle, hdt_load_assembly_and_get_function_pointer, reinterpret_cast<void **>(&load));
        close(handle);
        if (result != 0 || !load)
            throw std::runtime_error("Cannot get managed assembly loader");
        auto assembly = root / L"Scripts/Atom.Managed.dll";
        result = load(assembly.c_str(), L"Atom.Managed.Bridge, Atom.Managed", L"Dispatch", UNMANAGEDCALLERSONLY_METHOD,
                      nullptr, reinterpret_cast<void **>(&i.dispatch));
        if (result != 0 || !i.dispatch)
            throw std::runtime_error("Cannot load script bridge");
        i.context.error.clear();
        return true;
    }
    catch (const std::exception &e)
    {
        i.context.error = e.what();
        return false;
    }
}
void ScriptSystem::Reset()
{
    auto &i = *mImpl;
    if (i.lifetime.expired())
        i.context.world = nullptr;
    for (auto &[key, instance] : i.instances)
        i.Call({{"op", "destroy"}, {"id", instance.id}});
    i.instances.clear();
    i.lifetime.reset();
    i.context.world = nullptr;
    i.accumulator = 0;
    i.context.error.clear();
}
bool ScriptSystem::ReloadAssembly(const std::filesystem::path &file)
{
    auto &i = *mImpl;
    auto path = std::filesystem::absolute(file).lexically_normal();
    if (!Initialize() || !i.Call({{"op", "reload"}, {"assembly", path.generic_string()}}))
        return false;
    std::error_code ec;
    auto stamp = std::filesystem::last_write_time(path, ec);
    if (!ec)
        i.assemblies[path] = {stamp, stamp};
    for (auto it = i.instances.begin(); it != i.instances.end();)
    {
        if (it->second.assembly == path && !it->second.created)
            it = i.instances.erase(it);
        else
        {
            if (it->second.assembly == path)
                it->second.failed = false;
            ++it;
        }
    }
    i.context.error.clear();
    return true;
}
void ScriptSystem::Update(World &w, float dt)
{
    auto &i = *mImpl;
    auto life = w.GetLifetime().lock();
    if (i.lifetime.lock() != life)
    {
        Reset();
        i.lifetime = life;
    }
    i.context.world = &w;
    i.reloadTimer += (std::max)(0.f, dt);
    if (i.reloadTimer >= .5f)
    {
        i.reloadTimer = 0;
        for (auto &[path, watched] : i.assemblies)
        {
            if (!std::any_of(i.instances.begin(), i.instances.end(),
                             [&](const auto &item) { return item.second.assembly == path; }))
                continue;
            std::error_code ec;
            auto stamp = std::filesystem::last_write_time(path, ec);
            if (ec || stamp == watched.loaded)
                continue;
            if (stamp != watched.pending)
            {
                watched.pending = stamp;
                continue;
            }
            // Two stable polls avoid reading the output while the compiler is still replacing it.
            if (!ReloadAssembly(path))
                watched.loaded = stamp;
        }
    }
    std::set<std::pair<Entity, size_t>> active;
    for (auto e : w.View<ScriptComponent>())
    {
        auto &scripts = w.GetComponent<ScriptComponent>(e).scripts;
        for (size_t index = 0; index < scripts.size(); ++index)
        {
            auto &s = scripts[index];
            if (!s.enabled || s.assembly.empty() || s.type.empty())
                continue;
            auto key = std::make_pair(e, index);
            active.insert(key);
            Json definition = {{"assembly", s.assembly}, {"type", s.type}, {"fields", s.fields}};
            auto signature = definition.dump();
            auto it = i.instances.find(key);
            if (it != i.instances.end() && it->second.signature != signature)
            {
                i.Call({{"op", "destroy"}, {"id", it->second.id}});
                i.instances.erase(it);
                it = i.instances.end();
            }
            if (it == i.instances.end())
            {
                if (!Initialize())
                    continue;
                wchar_t exe[MAX_PATH];
                GetModuleFileNameW(nullptr, exe, MAX_PATH);
                auto path = std::filesystem::path(s.assembly);
                if (path.is_relative())
                    path = std::filesystem::path(exe).parent_path() / path;
                path = std::filesystem::absolute(path).lexically_normal();
                if (!i.assemblies.contains(path))
                {
                    std::error_code ec;
                    auto stamp = std::filesystem::last_write_time(path, ec);
                    i.assemblies[path] = {stamp, stamp};
                }
                auto id = ++i.nextId;
                definition["op"] = "create";
                definition["id"] = id;
                definition["entity"] = uint32_t(e);
                definition["assembly"] = path.generic_string();
                bool ok = i.Call(definition);
                it = i.instances.emplace(key, Impl::Instance{id, signature, !ok, path, ok}).first;
            }
            if (!it->second.failed && !i.Call({{"op", "update"}, {"id", it->second.id}, {"dt", dt}}))
                it->second.failed = true;
        }
    }
    for (auto it = i.instances.begin(); it != i.instances.end();)
        if (!active.contains(it->first))
        {
            i.Call({{"op", "destroy"}, {"id", it->second.id}});
            it = i.instances.erase(it);
        }
        else
            ++it;
}
void ScriptSystem::FixedUpdate(World &w, float dt)
{
    auto &i = *mImpl;
    i.context.world = &w;
    for (auto &[key, instance] : i.instances)
        if (!instance.failed && !i.Call({{"op", "fixed"}, {"id", instance.id}, {"dt", dt}}))
            instance.failed = true;
}

void ScriptSystem::UiClick(World &w, Entity entity)
{
    auto &i = *mImpl;
    i.context.world = &w;
    for (auto &[key, instance] : i.instances)
        if (key.first == entity && !instance.failed && !i.Call({{"op", "uiClick"}, {"id", instance.id}}))
            instance.failed = true;
}
void ScriptSystem::Collision(World &w, const PhysicsContact &c)
{
    auto &i = *mImpl;
    i.context.world = &w;
    for (auto &[key, instance] : i.instances)
        if (!instance.failed && (key.first == c.first || key.first == c.second))
            if (!i.Call({{"op", "collision"},
                         {"id", instance.id},
                         {"other", uint32_t(key.first == c.first ? c.second : c.first)},
                         {"trigger", c.trigger}}))
                instance.failed = true;
}
} // namespace AtomEngine
