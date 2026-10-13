#include "Runtime/Function/Scene/SceneManager.h"
#include "Runtime/Function/Framework/ECS/GameObject.h"
#include <iostream>
#include <stdexcept>

namespace AtomEngine { void Log(const char*, ...) {} void LogDebug(const char*, ...) {} void Log(const wchar_t*, ...) {} }
using namespace AtomEngine;
static void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
struct TestScene final : Scene {
    TestScene(SceneManager& manager, int& shutdowns) : Scene("Test", manager), shutdowns(shutdowns) {}
    void Update(float) override {}
    void Shutdown() override { ++shutdowns; }
    int& shutdowns;
};
int main() {
    World world;
    auto object = world.CreateGameObject();
    auto entity = object->GetHandle();
    Check(object->HasComponent<DeathFlag>(), "Unnamed entity must have base components");
    object.reset();
    auto wrapper = world.GetGameObject(entity);
    Check(wrapper && wrapper->IsValid(), "Entity must survive wrapper destruction");
    auto copy = world.GetGameObject(entity);
    wrapper->Destroy();
    wrapper->Destroy();
    Check(!copy->IsValid(), "Other wrappers must observe entity destruction");
    Check(!world.GetGameObject(entity), "Destroyed entity cannot be retrieved");
    auto replacement = world.CreateGameObject();
    Check(!copy->IsValid(), "Entity reuse must not revive a stale handle");
    World other;
    auto otherObject = other.CreateGameObject();
    Check(*replacement != *otherObject, "World identity must participate in equality");
    std::unique_ptr<GameObject> orphan;
    { auto transient = std::make_unique<World>(); orphan = transient->CreateGameObject(); }
    Check(!orphan->IsValid(), "World destruction must invalidate wrappers");
    auto a = world.CreateGameObject("Same"), b = world.CreateGameObject("Same");
    a->Destroy();
    Check(world.FindByName("Same") == b->GetHandle(), "Duplicate display names must survive deletion");
    int adds = 0, removes = 0;
    world.SetOnComponentAdded([&](Entity, std::type_index) { ++adds; });
    world.SetOnComponentRemoved([&](Entity, std::type_index) { ++removes; });
    replacement->AddComponent<int>(4);
    replacement->RemoveComponent<int>();
    Check(adds == 1 && removes == 1, "Component callbacks must execute");
    SceneManager manager;
    int shutdowns = 0;
    auto first = std::make_unique<TestScene>(manager, shutdowns);
    auto* firstPtr = first.get();
    manager.PushScene(std::move(first));
    manager.OnPushScene(std::make_unique<TestScene>(manager, shutdowns));
    manager.Update(0);
    Check(manager.GetCurrentScene() != firstPtr && shutdowns == 0, "Push must preserve underlying scene");
    manager.OnPopScene(); manager.Update(0);
    Check(manager.GetCurrentScene() == firstPtr && shutdowns == 1, "Pop must reveal underlying scene");
    manager.OnReplaceScene(std::make_unique<TestScene>(manager, shutdowns)); manager.Update(0);
    Check(manager.GetCurrentScene() && shutdowns == 2, "Replace must shut down previous scene");
    manager.OnPopScene(); manager.Update(0);
    Check(!manager.GetCurrentScene(), "Last pop must leave an empty stack");
    manager.OnPushScene(std::make_unique<TestScene>(manager, shutdowns)); manager.Update(0);
    Check(manager.GetCurrentScene(), "Queued push must work on an empty stack");
    manager.Shutdown();
    Check(shutdowns == 4, "Shutdown must release all active scenes");
    std::cout << "Framework regressions passed\n";
}
