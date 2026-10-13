#pragma once
#include "Runtime/Core/Math/MathInclude.h"
#include <map>
#include <string>
#include <vector>
namespace AtomEngine
{
struct CameraComponent
{
    float fieldOfView = 45, nearClip = .1f, farClip = 1000;
    bool primary = true, enabled = true;
};
struct DirectionalLightComponent
{
    Color color = Color::White;
    float intensity = 1;
    bool enabled = true, shadows = true, autoFitShadow = true;
};
struct PersistentIdComponent
{
    std::string value;
};
struct HierarchyComponent
{
    std::string parentId;
};
enum class BuiltinMesh
{
    None,
    Plane,
    Cube,
    Sphere,
    Quad
};
struct MeshSourceComponent
{
    BuiltinMesh primitive = BuiltinMesh::None;
    std::string asset;
    bool visible = true;
};
struct PrefabInstanceComponent
{
    std::string source;
};
enum class ColliderShape
{
    Box,
    Sphere,
    Capsule
};
struct ColliderComponent
{
    ColliderShape shape = ColliderShape::Box;
    Vector3 center = Vector3::ZERO;
    Quaternion rotation = Quaternion::IDENTITY;
    Vector3 halfExtents = Vector3(0.5f, 0.5f, 0.5f);
    float radius = 0.5f, halfHeight = 0.5f;
    bool trigger = false, visible = true;
};
enum class BodyMotion
{
    Static,
    Kinematic,
    Dynamic
};
struct RigidBodyComponent
{
    BodyMotion motion = BodyMotion::Dynamic;
    float mass = 1, friction = 0.5f, restitution = 0.1f;
    bool gravity = true;
    Vector3 linearVelocity = Vector3::ZERO;
};
struct ScriptAttachment
{
    std::string assembly = "Scripts/AtomScripts.dll", type = "Game.Rotate";
    bool enabled = true;
    std::map<std::string, std::string> fields;
};
struct ScriptComponent
{
    std::vector<ScriptAttachment> scripts;
};
struct UiElementComponent
{
    Vector2 position{20, 20}, size{200, 40};
    Color color = Color::White;
    std::string text = "Text", texture;
    float fontSize = 20;
    bool visible = true;
    bool interactable = false;
    int layer = 0;
    Color background = Color(.15f, .2f, .3f, .85f);
};
} // namespace AtomEngine
