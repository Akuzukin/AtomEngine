#include "ScriptSystem.h"
#include "Runtime/Function/Framework/Component/SceneComponents.h"
#include "Runtime/Function/Framework/Component/TransformComponent.h"
#include "Runtime/Function/Framework/Component/MeshComponent.h"
#include "Runtime/Function/Input/Input.h"
#include "External/dotnet/nethost.h"
#include "External/dotnet/hostfxr.h"
#include "External/dotnet/coreclr_delegates.h"
#include "json.hpp"
#include "Runtime/Function/Audio/Audio.h"
#include "Runtime/Function/Global/GlobalContext.h"
#include "Runtime/Function/Render/WindowManager.h"
#include "Runtime/Function/Render/Particle/ParticleEmitterRuntime.h"
#include "Runtime/Function/Scene/SceneSerializer.h"
#include "Runtime/Function/Scene/GameProject.h"
#include "Runtime/Function/Framework/ECS/GameObject.h"
#include "Runtime/Core/LogSystem/LogSystem.h"
#include "Runtime/Platform/DirectX12/Core/DirectX12Core.h"
#include <Windows.h>
#include <filesystem>
#include <set>
#include <algorithm>
#include <cmath>
#include <atomic>
#include <vector>
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
        std::string error, response, exchangeResponse;
        PhysicsWorld *physics=nullptr;
        struct Command { uint64_t request; int operation; uint32_t entity; std::string value; NativeTransform pose; };
        std::vector<Command> commands;
        std::map<uint64_t, uint32_t> results;
        std::set<uint64_t> inFlight;
        bool acceptCommands = false;
        uint64_t generation = 1;
    };
    struct NativeApi
    {
        uint32_t version = 7, size = sizeof(NativeApi);
        int(__cdecl *get)(void *, uint32_t, NativeTransform *);
        int(__cdecl *set)(void *, uint32_t, NativeTransform *);
        int(__cdecl *key)(int);
        void(__cdecl *log)(void *, const char *);
        int(__cdecl *velocity)(void *, uint32_t, float *, int);
        int(__cdecl *button)(int, int);
        float(__cdecl *axis)(int);
        int(__cdecl *uiText)(void *, uint32_t, char *, int, int);
        uint32_t(__cdecl *find)(void *, const char *);
        int(__cdecl *valid)(void *, uint32_t);
        int(__cdecl *visible)(void *, uint32_t, int);
        int(__cdecl *audio)(void *, uint32_t, int, const char *, float);
        uint64_t(__cdecl *command)(void *, uint32_t, int, const char *, NativeTransform *);
        int(__cdecl *result)(void *, uint64_t, uint32_t *);
        uint64_t(__cdecl *epoch)(void *);
        void(__cdecl *reply)(void *, const char *);
        int(__cdecl *exchange)(void *,const char *,char *,int);
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
        bool enabled=false;
        std::map<std::string,std::string> fields;
        bool updateCallback=true,fixedCallback=true,collisionCallback=true;
    };
    std::map<std::pair<Entity, size_t>, Instance> instances;
    struct WatchedAssembly
    {
        std::filesystem::file_time_type loaded{}, pending{};
    };
    std::map<std::filesystem::path, WatchedAssembly> assemblies;
    float reloadTimer = 0;
    std::weak_ptr<void> lifetime;
    static inline std::atomic<uint64_t> nextId = 0, nextCommand = 0;
    float accumulator = 0;
    float frameDelta = 0, fixedDelta = 1.f / 60;
    double elapsed = 0, fixedElapsed = 0;
    uint64_t frames = 0, fixedFrames = 0;
    bool paused = false;
    static bool HasEcsComponent(World &w, Entity e, ComponentKind kind)
    {
        switch(kind) {
        case ComponentKind::Mesh: return w.HasComponent<MeshComponent>(e) || w.HasComponent<MeshSourceComponent>(e);
        case ComponentKind::Collider: return w.HasComponent<ColliderComponent>(e);
        case ComponentKind::RigidBody: return w.HasComponent<RigidBodyComponent>(e);
        case ComponentKind::Script: return w.HasComponent<ScriptComponent>(e);
        case ComponentKind::UI: return w.HasComponent<UiElementComponent>(e);
        case ComponentKind::Audio: return w.HasComponent<AudioSourceComponent>(e);
        case ComponentKind::Camera: return w.HasComponent<CameraComponent>(e);
        case ComponentKind::Light: return w.HasComponent<DirectionalLightComponent>(e);
        case ComponentKind::Joint: return w.HasComponent<JointComponent>(e);
        case ComponentKind::Particle: return w.HasComponent<ParticleComponent>(e);
        }
        return false;
    }
    static Json QueryEcs(World &w, ComponentKind kind, bool activeOnly)
    {
        std::vector<uint32_t> entities;
        auto append=[&](auto view) {for(auto e:view)if(!activeOnly || w.IsEnabled(e,kind))entities.push_back(uint32_t(e));};
        switch(kind) {
        case ComponentKind::Mesh: append(w.View<MeshComponent>());append(w.View<MeshSourceComponent>());break;
        case ComponentKind::Collider: append(w.View<ColliderComponent>());break;
        case ComponentKind::RigidBody: append(w.View<RigidBodyComponent>());break;
        case ComponentKind::Script: append(w.View<ScriptComponent>());break;
        case ComponentKind::UI: append(w.View<UiElementComponent>());break;
        case ComponentKind::Audio: append(w.View<AudioSourceComponent>());break;
        case ComponentKind::Camera: append(w.View<CameraComponent>());break;
        case ComponentKind::Light: append(w.View<DirectionalLightComponent>());break;
        case ComponentKind::Joint: append(w.View<JointComponent>());break;
        case ComponentKind::Particle: append(w.View<ParticleComponent>());break;
        }
        std::sort(entities.begin(),entities.end());
        entities.erase(std::unique(entities.begin(),entities.end()),entities.end());
        return Json(entities);
    }
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
        api.key = [](int k) -> int { return Input::GetInstance()->GameplayInputEnabled() && k >= 0 && k < 256 && Input::GetInstance()->IsPressKey(uint8_t(k)); };
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
            if (!input->GameplayInputEnabled()) return 0;
            if (device == 0)
                return input->IsPressMouse(index);
            if (device == 1)
                return index >= 0 && index < COUNT && input->IsPressGamePad(GamePadButton(index));
            if (device == 2)
                return index >= 0 && index < 256 && input->IsTriggerKey(uint8_t(index));
            return 0;
        };
        api.axis = [](int axis) -> float {
            if (!Input::GetInstance()->GameplayInputEnabled()) return 0;
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
        api.find = [](void *p, const char *id) -> uint32_t {
            auto *w = static_cast<Context *>(p)->world;
            if (w && id)
                for (auto e : w->View<PersistentIdComponent>())
                    if (w->GetComponent<PersistentIdComponent>(e).value == id) return uint32_t(e);
            return uint32_t(entt::null);
        };
        api.valid = [](void *p, uint32_t e) -> int {
            auto *w = static_cast<Context *>(p)->world;
            return w && w->IsValid(Entity(e));
        };
        api.visible = [](void *p, uint32_t e, int value) -> int {
            auto *w = static_cast<Context *>(p)->world;
            if (!w || !w->IsValid(Entity(e))) return -1;
            auto *mesh = w->TryGet<MeshComponent>(Entity(e));
            auto *ui = w->TryGet<UiElementComponent>(Entity(e));
            auto *source = w->TryGet<MeshSourceComponent>(Entity(e));
            if (!mesh && !ui && !source) return -1;
            if (value >= 0) {
                if (source) source->visible = value != 0;
                if (mesh) mesh->SetIsRender(value != 0);
                if (ui) ui->visible = value != 0;
            }
            return mesh ? mesh->IsRender() : source ? source->visible : ui->visible;
        };
        api.exchange = [](void *p,const char *request,char *buffer,int capacity)->int {
            auto &c=*static_cast<Context *>(p);
            if(request) try {
                auto j=Json::parse(request);if(!c.world)throw std::runtime_error("Physics runtime unavailable");
                auto vector=[](const Json &v){if(!v.is_array() || v.size()!=3)throw std::runtime_error("Expected vector3");return Vector3(v.at(0).get<float>(),v.at(1).get<float>(),v.at(2).get<float>());};
                auto mask=j.value("mask",0xffffffffu);auto triggers=j.value("triggers",false);auto ignore=Entity(j.value("ignore",uint32_t(entt::null)));
                Json out=nullptr;auto op=j.at("op").get<std::string>();
                if(op=="particles") {
                    auto e=Entity(j.at("entity").get<uint32_t>());auto emitter=c.world->TryGet<ParticleComponent>(e);if(!emitter)throw std::runtime_error("Object has no particle component");
                    auto action=j.at("action").get<std::string>();
                    if(action=="emit"){auto pos=vector(j.at("position"));if(!std::isfinite(pos.x)||!std::isfinite(pos.y)||!std::isfinite(pos.z))throw std::runtime_error("Invalid particle position");EmitSceneParticle(*c.world,e,pos);out=true;}
                    else if(action=="stop"){ParticleSystem::StopParticle(emitter->runtimeId);out=true;}
                    else throw std::runtime_error("Invalid particle action");
                }
                else if(op=="mixer") {
                    auto audio=Audio::GetInstance();auto action=j.at("action").get<std::string>();
                    if(action=="master"){if(j.contains("value"))audio->SetMasterVolume(j.at("value").get<float>());out=audio->GetMasterVolume();}
                    else if(action=="bus"){int bus=j.at("bus").get<int>();if(j.contains("value"))audio->SetBusVolume(bus,j.at("value").get<float>());out=audio->GetBusVolume(bus);}
                    else if(action=="play"){audio->PlayClip(j.at("path").get<std::string>(),j.value("volume",1.f),false,uint64_t(j.at("entity").get<uint32_t>())+1,j.at("bus").get<int>());out=true;}
                    else throw std::runtime_error("Invalid mixer action");
                }
                else if(op=="application") {
                    if(!c.acceptCommands)throw std::runtime_error("Application requests require gameplay callbacks");
                    auto action=j.at("action").get<std::string>();
                    if(action=="quit")PostQuitMessage(0);
                    else if(action=="window") {auto width=j.at("width").get<int>(),height=j.at("height").get<int>();if(width<320||width>16384||height<240||height>16384)throw std::runtime_error("Invalid window size");auto window=gContext.windowManager;bool full=j.value("fullscreen",false);window->SetFullscreen(full);if(!full){auto handle=window->GetWindowHandle();RECT rect{0,0,width,height};AdjustWindowRect(&rect,GetWindowLongW(handle,GWL_STYLE),FALSE);SetWindowPos(handle,nullptr,0,0,rect.right-rect.left,rect.bottom-rect.top,SWP_NOMOVE|SWP_NOZORDER);}}
                    else throw std::runtime_error("Invalid application request");out=true;
                }
                else if(op=="scene") {
                    auto action=j.at("action").get<std::string>();
                    if(action=="current")out=ActiveProject.currentScene;
                    else if(action=="error")out=ActiveProject.lastLoadError;
                    else if(action=="loading")out=ActiveProject.pendingScene.has_value();
                    else if(action=="list")out=Json(ActiveProject.ScenePaths());
                    else if(action=="load") {
                        if(!c.acceptCommands)throw std::runtime_error("Scene loading is only available during gameplay callbacks");
                        ActiveProject.RequestScene(j.at("scene").get<std::string>());out=true;
                    } else throw std::runtime_error("Unknown scene operation");
                }
                else if(op=="ecsId") {
                    auto e=Entity(j.at("entity").get<uint32_t>());if(!c.world->IsValid(e))throw std::runtime_error("Destroyed object");out=c.world->GetComponent<PersistentIdComponent>(e).value;
                }
                else if(op=="ecsFind") {
                    auto name=j.at("name").get<std::string>();uint32_t found=uint32_t(entt::null);
                    for(auto e:c.world->View<NameComponent>())
                        if(c.world->GetComponent<NameComponent>(e).value==name && uint32_t(e)<found)found=uint32_t(e);
                    if(found!=uint32_t(entt::null))out=found;
                }
                else if(op=="ecsQuery" || op=="ecsHas") {
                    auto kind=j.at("kind").get<int>();if(kind<0 || kind>9)throw std::runtime_error("Invalid component kind");
                    if(op=="ecsQuery")out=QueryEcs(*c.world,ComponentKind(kind),j.value("activeOnly",true));
                    else {auto e=Entity(j.at("entity").get<uint32_t>());if(!c.world->IsValid(e))throw std::runtime_error("Destroyed object");out=HasEcsComponent(*c.world,e,ComponentKind(kind));}
                }
                else if(op=="ecsActiveBatch") {
                    auto &changes=j.at("changes");if(!changes.is_array() || changes.size()>16384)throw std::runtime_error("Invalid ECS batch size");
                    std::vector<std::pair<Entity,bool>> updates;updates.reserve(changes.size());
                    for(auto &change:changes) {auto e=Entity(change.at("entity").get<uint32_t>());if(!c.world->IsValid(e))throw std::runtime_error("Destroyed object in ECS batch");updates.emplace_back(e,change.at("active").get<bool>());}
                    for(auto [e,active]:updates) {if(!c.world->HasComponent<ObjectStateComponent>(e))c.world->AddComponent<ObjectStateComponent>(e);c.world->GetComponent<ObjectStateComponent>(e).activeSelf=active;}
                    out=true;
                }
                else if(op=="tint") {
                    auto e=Entity(j.at("entity").get<uint32_t>());auto m=c.world->TryGet<MaterialComponent>(e);
                    if(!m)throw std::runtime_error("Object has no material");
                    auto color=vector(j.at("color"));if(!std::isfinite(color.x)||!std::isfinite(color.y)||!std::isfinite(color.z)||color.x<0||color.y<0||color.z<0)throw std::runtime_error("Invalid material color");
                    float glow=j.value("glow",0.f);if(!std::isfinite(glow)||glow<0||glow>10)throw std::runtime_error("Invalid material glow");
                    for(auto &v:m->mMaterials){v.mBaseColor=Color(color.x,color.y,color.z,1);v.mEmissive=color*glow;}out=true;
                }
                else if(op=="pointer") {
                    auto &w=*c.world;
                    out={{"valid",w.gameplayPointerValid},{"position",{w.gameplayPointer.x,w.gameplayPointer.y}},{"size",{w.uiRenderSize.x,w.uiRenderSize.y}}};
                }
                else if(op=="input") {
                    auto device=j.at("device").get<int>(),code=j.at("code").get<int>(),state=j.at("state").get<int>();
                    auto input=Input::GetInstance();if(device<0||device>2||state<0||state>2||code<0||(device==0 && code>=256)||(device==1 && code>=5)||(device==2 && code>=COUNT))throw std::runtime_error("Invalid input query");
                    bool value=false;
                    if(input->GameplayInputEnabled()) {
                        if(device==0)value=state==0?input->IsPressKey(uint8_t(code)):state==1?input->IsTriggerKey(uint8_t(code)):input->IsReleaseKey(uint8_t(code));
                        else if(device==1)value=state==0?input->IsPressMouse(code):state==1?input->IsTriggerMouse(code):input->IsReleaseMouse(code);
                        else value=state==0?input->IsPressGamePad(GamePadButton(code)):state==1?input->IsTriggerGamePad(GamePadButton(code)):input->IsReleaseGamePad(GamePadButton(code));
                    }
                    out=value;
                }
                else if(op=="ui") {
                    auto e=Entity(j.at("entity").get<uint32_t>());auto component=c.world->TryGet<UiElementComponent>(e);if(!component)throw std::runtime_error("Object has no UI element");auto candidate=*component;
                    auto vec=[](const Json &v){if(!v.is_array()||v.size()!=2)throw std::runtime_error("Expected vector2");auto x=v[0].get<float>(),y=v[1].get<float>();if(!std::isfinite(x)||!std::isfinite(y))throw std::runtime_error("UI vector must be finite");return Vector2(x,y);};
                    if(j.contains("position"))candidate.position=vec(j["position"]);if(j.contains("size"))candidate.size=vec(j["size"]);if(j.contains("anchorMin"))candidate.anchorMin=vec(j["anchorMin"]);if(j.contains("anchorMax"))candidate.anchorMax=vec(j["anchorMax"]);if(j.contains("pivot"))candidate.pivot=vec(j["pivot"]);
                    if(j.contains("anchorMin")||j.contains("anchorMax"))candidate.responsive=true;
                    if(j.contains("progress"))candidate.progress=j["progress"].get<float>();if(j.contains("visible"))candidate.visible=j["visible"].get<bool>();if(j.contains("interactable"))candidate.interactable=j["interactable"].get<bool>();if(j.contains("responsive"))candidate.responsive=j["responsive"].get<bool>();
                    if(!std::isfinite(candidate.progress)||candidate.progress<0||candidate.progress>1||candidate.size.x<=0||candidate.size.y<=0||candidate.size.x>4096||candidate.size.y>4096||candidate.anchorMin.x<0||candidate.anchorMin.y<0||candidate.anchorMax.x>1||candidate.anchorMax.y>1||candidate.anchorMin.x>candidate.anchorMax.x||candidate.anchorMin.y>candidate.anchorMax.y||candidate.pivot.x<0||candidate.pivot.x>1||candidate.pivot.y<0||candidate.pivot.y>1)throw std::runtime_error("Invalid UI layout values");
                    *component=candidate;out={{"position",{candidate.position.x,candidate.position.y}},{"size",{candidate.size.x,candidate.size.y}},{"progress",candidate.progress},{"anchorMin",{candidate.anchorMin.x,candidate.anchorMin.y}},{"anchorMax",{candidate.anchorMax.x,candidate.anchorMax.y}},{"pivot",{candidate.pivot.x,candidate.pivot.y}},{"visible",candidate.visible},{"responsive",candidate.responsive},{"interactable",candidate.interactable}};
                }
                else if(op=="active" || op=="activeHierarchy" || op=="component") {
                    auto e=Entity(j.at("entity").get<uint32_t>());if(!c.world->IsValid(e))throw std::runtime_error("Destroyed object");
                    if(op=="component"){auto kind=j.at("kind").get<int>();if(kind<0 || kind>9)throw std::runtime_error("Invalid component kind");if(j.contains("value"))c.world->SetComponentEnabled(e,ComponentKind(kind),j.at("value").get<bool>());auto state=c.world->TryGet<ObjectStateComponent>(e);out=!state || !(state->disabledComponents & (1u<<kind));}
                    else {if(!c.world->HasComponent<ObjectStateComponent>(e))c.world->AddComponent<ObjectStateComponent>(e);auto &state=c.world->GetComponent<ObjectStateComponent>(e);if(j.contains("value"))state.activeSelf=j.at("value").get<bool>();out=op=="active"?state.activeSelf:c.world->IsActive(e);}
                }
                else if(op=="raycast"){if(!c.physics)throw std::runtime_error("Physics runtime unavailable");PhysicsHit hit;if(c.physics->Raycast(*c.world,vector(j.at("origin")),vector(j.at("direction")),j.at("distance").get<float>(),hit,mask,triggers,ignore))out={{"entity",uint32_t(hit.entity)},{"point",{hit.point.x,hit.point.y,hit.point.z}},{"normal",{hit.normal.x,hit.normal.y,hit.normal.z}},{"distance",hit.distance}};}
                else if(op=="overlap"){if(!c.physics)throw std::runtime_error("Physics runtime unavailable");out=Json::array();for(auto e:c.physics->OverlapSphere(*c.world,vector(j.at("center")),j.at("radius").get<float>(),mask,triggers,ignore))out.push_back(uint32_t(e));}
                else throw std::runtime_error("Unknown native request");
                c.exchangeResponse=out.dump();
            } catch(const std::exception &error){c.exchangeResponse=Json{{"error",error.what()}}.dump();}
            int count=int(c.exchangeResponse.size())+1;if(buffer && capacity>=count)std::copy_n(c.exchangeResponse.c_str(),count,buffer);return count;
        };
        api.reply = [](void *p, const char *value) { static_cast<Context *>(p)->response = value ? value : ""; };
        api.epoch = [](void *p) -> uint64_t { return static_cast<Context *>(p)->generation; };
        api.audio = [](void *p, uint32_t e, int operation, const char *path, float volume) -> int {
            auto *w = static_cast<Context *>(p)->world;
            if (!w || !w->IsValid(Entity(e))) return 0;
            try {
                auto audio = Audio::GetInstance(); uint64_t owner = uint64_t(e) + 1;
                if (operation == 1) { audio->StopOwner(owner); return 1; }
                if (!w->IsEnabled(Entity(e),ComponentKind::Audio))return 0;
                if (operation == 2 && path) { audio->PlayClip(path, volume, false, owner); return 1; }
                if (auto source = w->TryGet<AudioSourceComponent>(Entity(e)); operation == 0 && source && !source->clip.empty()) {
                    audio->PlayClip(source->clip, source->volume, source->loop, owner,source->bus); return 1;
                }
            } catch (const std::exception &error) { WriteLog(LogLevel::Warning, "Audio", error.what()); }
            return 0;
        };
        api.command = [](void *p, uint32_t e, int operation, const char *value, NativeTransform *pose) -> uint64_t {
            auto &c = *static_cast<Context *>(p);
            if (!c.acceptCommands || !c.world || !c.world->IsValid(Entity(e)) || operation < 0 || operation > 3 ||
                c.commands.size() >= 256 || (operation != 2 &&
                    c.results.size() + c.commands.size() + c.inFlight.size() >= 4096)) return 0;
            if (operation != 2) {
                if (!value || !*value || !pose) return 0;
                for (float v : pose->position) if (!std::isfinite(v)) return 0;
            }
            auto request = ++nextCommand;
            c.commands.push_back({request,operation,e,value ? value : "",pose ? *pose : NativeTransform{}});
            return request;
        };
        api.result = [](void *p, uint64_t request, uint32_t *entity) -> int {
            auto &c = *static_cast<Context *>(p);
            if (auto it = c.results.find(request); it != c.results.end()) {
                *entity = it->second; c.results.erase(it);
                return c.world && c.world->IsValid(Entity(*entity)) ? 1 : -1;
            }
            if (c.inFlight.contains(request)) return 0;
            for (auto &command : c.commands) if (command.request == request) return 0;
            return -1;
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
        auto op = j.value("op", std::string{});
        // Update() already publishes the shared frame clock before invoking any script.
        // Avoid serializing/parsing the identical clock once per actor; fixed/collision
        // callbacks still receive their own fixed-step context.
        if (op != "inspect" && op != "validate" && op != "update")
            j["time"] = {{"delta", frameDelta}, {"fixedDelta", fixedDelta}, {"elapsed", elapsed},
                         {"fixedElapsed", fixedElapsed}, {"frames", frames}, {"fixedFrames", fixedFrames}, {"paused", paused}};
        auto text = j.dump();
        return dispatch(text.c_str(), &api, &context) == 0;
    }
    void ReadCallbacks(Instance &instance,const Json &flags)
    {
        instance.updateCallback=flags.value("update",true);
        instance.fixedCallback=flags.value("fixedUpdate",true);
        instance.collisionCallback=flags.value("collision",true);
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
        auto bundledRoot = root / L"Runtime";
        get_hostfxr_parameters locateParams{sizeof(get_hostfxr_parameters), nullptr, bundledRoot.c_str()};
        auto params = std::filesystem::is_directory(bundledRoot / L"host/fxr") ? &locateParams : nullptr;
        if (locate(hostPath, &count, params) != 0)
            throw std::runtime_error(".NET 10 runtime not installed");
        i.hostfxr = LoadLibraryW(hostPath);
        WriteLog(LogLevel::Info, "Scripts", "Managed host: " + WStringToUTF8(hostPath));
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
        auto hostExe = std::filesystem::path(exe);
        hostfxr_initialize_parameters initParams{sizeof(hostfxr_initialize_parameters), hostExe.c_str(), bundledRoot.c_str()};
        int result = init(config.c_str(), params ? &initParams : nullptr, &handle);
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
namespace {
std::filesystem::path ScriptPath(const std::filesystem::path &path) {
    if (path.is_absolute()) return path;
    wchar_t exe[32768]{}; GetModuleFileNameW(nullptr,exe,DWORD(std::size(exe)));
    return std::filesystem::path(exe).parent_path()/path;
}
}
std::string ScriptSystem::DescribeAssembly(const std::filesystem::path &path)
{
    auto &i=*mImpl; i.context.error.clear(); i.context.response.clear();
    if(!Initialize() || !i.Call({{"op","inspect"},{"assembly",ScriptPath(path).generic_string()}}))
        throw std::runtime_error(i.context.error.empty()?"Cannot inspect scripts":i.context.error);
    return i.context.response;
}
bool ScriptSystem::ValidateFields(const ScriptAttachment &attachment)
{
    auto &i=*mImpl; i.context.error.clear();
    return Initialize() && i.Call({{"op","validate"},{"assembly",ScriptPath(UTF8ToWString(attachment.assembly)).generic_string()},
        {"type",attachment.type},{"fields",attachment.fields}});
}
void ScriptSystem::SynchronizeParameters(World &world) {
    auto &i=*mImpl;if(i.context.world!=&world || i.lifetime.lock()!=world.GetLifetime().lock())return;
    for(auto &[key,instance]:i.instances)if(instance.created && world.IsValid(key.first))if(auto component=world.TryGet<ScriptComponent>(key.first);component && key.second<component->scripts.size()){
        auto &attachment=component->scripts[key.second];if(attachment.fields==instance.fields)continue;
        if(i.Call({{"op","patchFields"},{"id",instance.id},{"fields",attachment.fields}})){instance.fields=attachment.fields;i.context.error.clear();}else attachment.fields=instance.fields;
    }
}
std::string ScriptSystem::ReadFields(World &world,Entity e,size_t index) {
    auto &i=*mImpl;auto it=i.instances.find({e,index});if(i.context.world!=&world || i.lifetime.lock()!=world.GetLifetime().lock() || it==i.instances.end() || !it->second.created)return "{}";
    i.context.response.clear();if(!i.Call({{"op","readFields"},{"id",it->second.id}}))return "{}";return i.context.response;
}
void ScriptSystem::SetPhysics(PhysicsWorld *physics){mImpl->context.physics=physics;}
void ScriptSystem::Reset()
{
    auto &i = *mImpl;
    if (i.lifetime.expired())
        i.context.world = nullptr;
    i.context.acceptCommands = false;
    for (auto &[key, instance] : i.instances)
        {if(instance.enabled)i.Call({{"op","disable"},{"id",instance.id}});i.Call({{"op", "destroy"}, {"id", instance.id}});}
    i.instances.clear();
    i.context.commands.clear(); i.context.results.clear(); i.context.inFlight.clear(); ++i.context.generation;
    i.lifetime.reset();
    i.context.world = nullptr;
    i.accumulator = 0;
    i.frameDelta = 0; i.fixedDelta = 1.f / 60; i.elapsed = i.fixedElapsed = 0;
    i.frames = i.fixedFrames = 0; i.paused = false;
    i.Call({{"op", "time"}});
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
    i.context.response.clear();
    if(i.Call({{"op","callbacks"}})&&!i.context.response.empty()){
        auto callbacks=Json::parse(i.context.response);
        for(auto &[key,instance]:i.instances){auto id=std::to_string(instance.id);if(callbacks.contains(id))i.ReadCallbacks(instance,callbacks[id]);}
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
    i.context.acceptCommands = true;
    dt = std::isfinite(dt) ? (std::max)(0.f, dt) : 0.f;
    i.frameDelta = dt; i.elapsed += double(dt); ++i.frames; i.paused = false;
    i.Call({{"op", "time"}});
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
    std::vector<std::pair<Entity, size_t>> updates;
    for (auto e : w.View<ScriptComponent>())
    {
        auto &scripts = w.GetComponent<ScriptComponent>(e).scripts;
        for (size_t index = 0; index < scripts.size(); ++index)
        {
            auto &s = scripts[index];
            if (s.assembly.empty() || s.type.empty())
                continue;
            auto key = std::make_pair(e, index);
            active.insert(key);
            Json definition = {{"assembly", s.assembly}, {"type", s.type}, {"fields", s.fields}};
            auto signature = Json{{"assembly",s.assembly},{"type",s.type}}.dump();
            auto it = i.instances.find(key);
            if (it != i.instances.end() && it->second.signature != signature)
            {
                if(it->second.enabled)i.Call({{"op","disable"},{"id",it->second.id}});
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
                i.context.response.clear();
                bool ok = i.Call(definition);
                it = i.instances.emplace(key, Impl::Instance{id, signature, !ok, path, ok, false, s.fields}).first;
                if(ok&&!i.context.response.empty())i.ReadCallbacks(it->second,Json::parse(i.context.response));
            }
            if(it->second.created && it->second.fields!=s.fields){
                if(i.Call({{"op","patchFields"},{"id",it->second.id},{"fields",s.fields}})){it->second.fields=s.fields;i.context.error.clear();}
                else s.fields=it->second.fields;
            }
            updates.push_back(key);
        }
    }
    for (auto it = i.instances.begin(); it != i.instances.end();)
        if (!active.contains(it->first))
        {
            if(it->second.enabled)i.Call({{"op","disable"},{"id",it->second.id}});
                i.Call({{"op", "destroy"}, {"id", it->second.id}});
            it = i.instances.erase(it);
        }
        else
            ++it;
    // Finish all OnCreate/OnDestroy callbacks before the first OnUpdate.
    // Startup registries must not depend on the ECS pool's iteration order,
    // which can change after restoring the editing snapshot on Stop.
    for (const auto &key : updates)
    {
        auto it = i.instances.find(key);
        if(it==i.instances.end())continue;
        auto e=key.first;auto component=w.TryGet<ScriptComponent>(e);
        bool enabled=component && key.second<component->scripts.size() && component->scripts[key.second].enabled && w.IsEnabled(e,ComponentKind::Script);
        if(it->second.created && it->second.enabled!=enabled){it->second.enabled=enabled;if(!i.Call({{"op",enabled?"enable":"disable"},{"id",it->second.id}}))it->second.failed=true;}
        if (enabled && it->second.updateCallback && !it->second.failed &&
            !i.Call({{"op", "update"}, {"id", it->second.id}, {"dt", dt}}))
            it->second.failed = true;
    }
    for(auto &[key,instance]:i.instances){auto component=w.TryGet<ScriptComponent>(key.first);bool enabled=component && key.second<component->scripts.size() && component->scripts[key.second].enabled && w.IsEnabled(key.first,ComponentKind::Script);
        if(instance.created && instance.enabled!=enabled){instance.enabled=enabled;if(!i.Call({{"op",enabled?"enable":"disable"},{"id",instance.id}}))instance.failed=true;}}
}
void ScriptSystem::FlushCommands(World &w)
{
    auto &i = *mImpl;
    if (i.context.world != &w) return;
    auto commands = std::move(i.context.commands); i.context.commands.clear();
    for (const auto &command : commands) i.context.inFlight.insert(command.request);
    for (const auto &command : commands)
    {
        if (command.operation == 2)
        {
            Entity root = Entity(command.entity); i.context.inFlight.erase(command.request); if (!w.IsValid(root)) continue;
            std::set<Entity> destroyed{root};
            bool changed = true;
            while (changed) {
                changed = false;
                for (auto e : w.View<HierarchyComponent>()) {
                    auto parent = w.GetComponent<HierarchyComponent>(e).parentId;
                    for (auto candidate : destroyed)
                        if (auto id = w.TryGet<PersistentIdComponent>(candidate); id && id->value == parent && !destroyed.contains(e))
                            { destroyed.insert(e); changed = true; break; }
                }
            }
            for (auto it = i.instances.begin(); it != i.instances.end();)
                if (destroyed.contains(it->first.first)) {
                    if(it->second.enabled)i.Call({{"op","disable"},{"id",it->second.id}});i.Call({{"op","destroy"},{"id",it->second.id}}); it = i.instances.erase(it);
                } else ++it;
            for (auto e : destroyed) { Audio::GetInstance()->StopOwner(uint64_t(uint32_t(e)) + 1); w.DestroyGameObject(e); }
        }
        else
        {
            Entity result = entt::null;
            try {
                if(command.operation==3){
                    result=Entity(command.entity);if(!w.IsValid(result))throw std::runtime_error("Prefab target was destroyed");
                    DX12Core::gCommandManager.IdleGPU();SceneSerializer::AttachPrefab(w,result,UTF8ToWString(command.value));
                    Audio::GetInstance()->StopOwner(uint64_t(uint32_t(result))+1);
                }
                else if (command.operation == 1) result = SceneSerializer::InstantiatePrefab(w, UTF8ToWString(command.value));
                else {
                    auto object = w.CreateGameObject(command.value);
                    result = object->GetHandle(); w.AddComponent<TransformComponent>(result);
                }
                if(command.operation!=3)w.GetComponent<TransformComponent>(result).transition = Vector3(command.pose.position);
            } catch (const std::exception &error) { result=entt::null;WriteLog(LogLevel::Warning,"Scripts",error.what()); }
            i.context.results[command.request] = uint32_t(result);
            i.context.inFlight.erase(command.request);
        }
    }
    SceneSerializer::ResolveHierarchy(w);
}
void ScriptSystem::FixedUpdate(World &w, float dt)
{
    auto &i = *mImpl;
    dt = std::isfinite(dt) ? (std::max)(0.f, dt) : 0.f;
    i.fixedDelta = dt; i.fixedElapsed += double(dt); ++i.fixedFrames;
    i.Call({{"op", "time"}});
    i.context.world = &w;
    for (auto &[key, instance] : i.instances)
        if (instance.enabled && instance.fixedCallback && w.IsEnabled(key.first,ComponentKind::Script) && !instance.failed && !i.Call({{"op", "fixed"}, {"id", instance.id}, {"dt", dt}}))
            instance.failed = true;
}

void ScriptSystem::PauseTime()
{
    auto &i = *mImpl;
    i.frameDelta = 0; i.paused = true;
    i.Call({{"op", "time"}});
}

void ScriptSystem::UiClick(World &w, Entity entity)
{
    auto &i = *mImpl;
    i.context.world = &w;
    for (auto &[key, instance] : i.instances)
        if (key.first == entity && instance.enabled && w.IsEnabled(key.first,ComponentKind::Script) && !instance.failed && !i.Call({{"op", "uiClick"}, {"id", instance.id}}))
            instance.failed = true;
}
void ScriptSystem::Collision(World &w, const PhysicsContact &c)
{
    auto &i = *mImpl;
    i.context.world = &w;
    for (auto &[key, instance] : i.instances)
        if (instance.enabled && instance.collisionCallback && w.IsEnabled(key.first,ComponentKind::Script) && !instance.failed && (key.first == c.first || key.first == c.second))
            if (!i.Call({{"op", "collision"},
                         {"id", instance.id},
                         {"other", uint32_t(key.first == c.first ? c.second : c.first)},
                         {"trigger", c.trigger}}))
                instance.failed = true;
}
} // namespace AtomEngine
