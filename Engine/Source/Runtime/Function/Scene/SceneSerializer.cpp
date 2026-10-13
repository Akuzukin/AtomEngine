#include "SceneSerializer.h"
#include "Runtime/Function/Framework/ECS/GameObject.h"
#include "Runtime/Function/Framework/Component/SceneComponents.h"
#include "Runtime/Function/Framework/Component/MeshComponent.h"
#include "Runtime/Resource/Primitive.h"
#include "Runtime/Resource/AssetManager.h"
#include "External/tinyxml2/tinyxml2.h"
#include <set>
#include "Runtime/Core/LogSystem/LogSystem.h"
#include <cmath>
#include <stdexcept>
namespace AtomEngine
{
namespace
{
using namespace tinyxml2;
XMLElement *Child(XMLNode *p, const char *n)
{
    auto e = p->GetDocument()->NewElement(n);
    p->InsertEndChild(e);
    return e;
}
std::string Str(const XMLElement *e, const char *n, const char *d = "")
{
    auto v = e->Attribute(n);
    return v ? v : d;
}
float Float(const XMLElement *e, const char *n, float d)
{
    float v = d;
    auto r = e->QueryFloatAttribute(n, &v);
    if ((r != XML_SUCCESS && r != XML_NO_ATTRIBUTE) || !std::isfinite(v))
        throw std::runtime_error(std::string("Invalid float: ") + n);
    return v;
}
int Enum(const XMLElement *e, const char *n, int d, int max)
{
    int v = d;
    auto r = e->QueryIntAttribute(n, &v);
    if ((r != XML_SUCCESS && r != XML_NO_ATTRIBUTE) || v < 0 || v > max)
        throw std::runtime_error(std::string("Invalid enum: ") + n);
    return v;
}
void Vec(XMLElement *e, const Vector3 &v)
{
    e->SetAttribute("x", v.x);
    e->SetAttribute("y", v.y);
    e->SetAttribute("z", v.z);
}
Vector3 Vec(const XMLElement *e, Vector3 d = Vector3::ZERO)
{
    return e ? Vector3(Float(e, "x", d.x), Float(e, "y", d.y), Float(e, "z", d.z)) : d;
}
void Quat(XMLElement *e, const Quaternion &q)
{
    e->SetAttribute("x", q.x);
    e->SetAttribute("y", q.y);
    e->SetAttribute("z", q.z);
    e->SetAttribute("w", q.w);
}
Quaternion Quat(const XMLElement *e)
{
    if (!e)
        return Quaternion::IDENTITY;
    Quaternion q(Float(e, "x", 0), Float(e, "y", 0), Float(e, "z", 0), Float(e, "w", 1));
    float l = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (l < 1e-6f)
        throw std::runtime_error("Zero quaternion");
    return q / l;
}
void WriteTransform(XMLElement *e, const Transform &t)
{
    Vec(Child(e, "Position"), t.transition);
    Quat(Child(e, "Rotation"), t.rotation);
    Vec(Child(e, "Scale"), t.scale);
}
void ReadTransform(const XMLElement *e, Transform &t)
{
    t.transition = Vec(e->FirstChildElement("Position"));
    t.rotation = Quat(e->FirstChildElement("Rotation"));
    t.scale = Vec(e->FirstChildElement("Scale"), Vector3::UNIT_SCALE);
    if (t.scale.x <= 0 || t.scale.y <= 0 || t.scale.z <= 0)
        throw std::runtime_error("Scale must be positive");
}
void WriteObject(World &w, Entity id, XMLElement *e)
{
    e->SetAttribute("id", w.GetComponent<PersistentIdComponent>(id).value.c_str());
    if (auto c = w.TryGet<NameComponent>(id))
        e->SetAttribute("name", c->value.c_str());
    if (auto c = w.TryGet<HierarchyComponent>(id))
        e->SetAttribute("parent", c->parentId.c_str());
    if (auto c = w.TryGet<PrefabInstanceComponent>(id))
    {
        e->SetAttribute("prefab", c->source.c_str());
        auto link = Child(e, "PrefabLink");
        link->SetAttribute("sourceId", c->sourceId.c_str());
        link->SetAttribute("instanceId", c->instanceId.c_str());
        Child(link, "Baseline")->SetText(c->baseline.c_str());
        for (auto &[source, target] : c->members)
        {
            auto member = Child(link, "Member");
            member->SetAttribute("source", source.c_str());
            member->SetAttribute("target", target.c_str());
        }
    }
    if (auto c = w.TryGet<TransformComponent>(id))
        WriteTransform(Child(e, "Transform"), *c);
    if (auto c = w.TryGet<CameraComponent>(id))
    {
        auto n = Child(e, "Camera");
        n->SetAttribute("fieldOfView", c->fieldOfView);
        n->SetAttribute("nearClip", c->nearClip);
        n->SetAttribute("farClip", c->farClip);
        n->SetAttribute("primary", c->primary);
        n->SetAttribute("enabled", c->enabled);
    }
    if (auto c = w.TryGet<DirectionalLightComponent>(id))
    {
        auto n = Child(e, "DirectionalLight");
        Vec(Child(n, "Color"), c->color.ToVector3());
        n->SetAttribute("intensity", c->intensity);
        n->SetAttribute("enabled", c->enabled);
        n->SetAttribute("shadows", c->shadows);
        n->SetAttribute("autoFitShadow", c->autoFitShadow);
    }
    if (auto c = w.TryGet<MeshSourceComponent>(id))
    {
        auto n = Child(e, "Mesh");
        n->SetAttribute("primitive", int(c->primitive));
        n->SetAttribute("asset", c->asset.c_str());
        n->SetAttribute("visible", c->visible);
    }
    if (auto c = w.TryGet<MaterialComponent>(id))
        for (auto &m : c->mMaterials)
        {
            auto n = Child(e, "Material");
            Vec(Child(n, "BaseColor"), m.mBaseColor.ToVector3());
            n->FirstChildElement("BaseColor")->SetAttribute("alpha", m.mBaseColor.ToVector4().w);
            Vec(Child(n, "Emissive"), m.mEmissive);
            Vec(Child(n, "Fresnel"), m.mFresnelF0);
            n->SetAttribute("metallic", m.mMetallic);
            n->SetAttribute("roughness", m.mRoughness);
            n->SetAttribute("normalScale", m.mNormalScale);
            n->SetAttribute("ao", m.mAO);
            n->SetAttribute("useLight", m.useLight);
            n->SetAttribute("shadingModel", int(m.shadingModel));
            n->SetAttribute("overrideModelTextures", m.overrideModelTextures);
            n->SetAttribute("packedTextureChannels", m.packedTextureChannels);
            WriteTransform(Child(n, "UV"), m.uvTransform);
            for (size_t i = 0; i < m.texturePaths.size(); ++i)
                if (!m.texturePaths[i].empty())
                {
                    auto t = Child(n, "Texture");
                    t->SetAttribute("slot", int(i));
                    t->SetAttribute("path", m.texturePaths[i].c_str());
                }
        }
    if (auto c = w.TryGet<ColliderComponent>(id))
    {
        auto n = Child(e, "Collider");
        n->SetAttribute("shape", int(c->shape));
        Vec(Child(n, "Center"), c->center);
        Quat(Child(n, "Rotation"), c->rotation);
        Vec(Child(n, "HalfExtents"), c->halfExtents);
        n->SetAttribute("radius", c->radius);
        n->SetAttribute("halfHeight", c->halfHeight);
        n->SetAttribute("trigger", c->trigger);
        n->SetAttribute("visible", c->visible);
    }
    if (auto c = w.TryGet<RigidBodyComponent>(id))
    {
        auto n = Child(e, "RigidBody");
        n->SetAttribute("motion", int(c->motion));
        n->SetAttribute("mass", c->mass);
        n->SetAttribute("friction", c->friction);
        n->SetAttribute("restitution", c->restitution);
        n->SetAttribute("gravity", c->gravity);
        Vec(Child(n, "Velocity"), c->linearVelocity);
    }
    if (auto c = w.TryGet<JointComponent>(id))
    {
        auto n = Child(e, "Joint");
        n->SetAttribute("type", int(c->type));
        n->SetAttribute("connectedBody", c->connectedBody.c_str());
        n->SetAttribute("enabled", c->enabled);
        Vec(Child(n, "Anchor"), c->anchor);
        Vec(Child(n, "ConnectedAnchor"), c->connectedAnchor);
        Vec(Child(n, "Axis"), c->axis);
        n->SetAttribute("minDistance", c->minDistance);
        n->SetAttribute("maxDistance", c->maxDistance);
        n->SetAttribute("minAngle", c->minAngle);
        n->SetAttribute("maxAngle", c->maxAngle);
    }
    if (auto c = w.TryGet<ScriptComponent>(id))
        for (auto &s : c->scripts)
        {
            auto n = Child(e, "Script");
            n->SetAttribute("assembly", s.assembly.c_str());
            n->SetAttribute("type", s.type.c_str());
            n->SetAttribute("enabled", s.enabled);
            for (auto &[key, val] : s.fields)
            {
                auto f = Child(n, "Field");
                f->SetAttribute("name", key.c_str());
                f->SetAttribute("value", val.c_str());
            }
        }
    if (auto c = w.TryGet<UiElementComponent>(id))
    {
        auto n = Child(e, "UI");
        n->SetAttribute("x", c->position.x);
        n->SetAttribute("y", c->position.y);
        n->SetAttribute("width", c->size.x);
        n->SetAttribute("height", c->size.y);
        n->SetAttribute("text", c->text.c_str());
        n->SetAttribute("texture", c->texture.c_str());
        n->SetAttribute("fontSize", c->fontSize);
        n->SetAttribute("interactable", c->interactable);
        n->SetAttribute("layer", c->layer);
        Vec(Child(n, "Background"), c->background.ToVector3());
        n->FirstChildElement("Background")->SetAttribute("alpha", c->background.ToVector4().w);
        n->SetAttribute("visible", c->visible);
        Vec(Child(n, "Color"), c->color.ToVector3());
        n->FirstChildElement("Color")->SetAttribute("alpha", c->color.ToVector4().w);
    }
}
Entity ReadObject(World &w, const XMLElement *e, bool freshId)
{
    auto o = w.CreateGameObject(Str(e, "name"));
    Entity id = o->GetHandle();
    struct Guard
    {
        World &w;
        Entity id;
        bool committed = false;
        ~Guard()
        {
            if (!committed)
                w.DestroyGameObject(id);
        }
    } guard{w, id};
    if (!freshId)
    {
        auto key = Str(e, "id");
        if (key.empty())
            throw std::runtime_error("Object has no persistent ID");
        o->GetComponent<PersistentIdComponent>().value = key;
    }
    if (auto parent = Str(e, "parent"); !parent.empty())
        o->AddComponent<HierarchyComponent>(parent);
    if (auto prefab = Str(e, "prefab"); !prefab.empty())
        o->AddComponent<PrefabInstanceComponent>(prefab);
    o->AddComponent<TransformComponent>();
    if (auto n = e->FirstChildElement("Transform"))
        ReadTransform(n, o->GetComponent<TransformComponent>());
    if (auto n = e->FirstChildElement("Camera"))
    {
        CameraComponent c;
        c.fieldOfView = Float(n, "fieldOfView", 45);
        c.nearClip = Float(n, "nearClip", .1f);
        c.farClip = Float(n, "farClip", 1000);
        c.primary = n->BoolAttribute("primary", true);
        c.enabled = n->BoolAttribute("enabled", true);
        if (c.fieldOfView < 1 || c.fieldOfView > 179 || c.nearClip <= 0 || c.farClip <= c.nearClip)
            throw std::runtime_error("Invalid camera projection");
        o->AddComponent<CameraComponent>(c);
    }
    if (auto n = e->FirstChildElement("DirectionalLight"))
    {
        DirectionalLightComponent c;
        auto color = Vec(n->FirstChildElement("Color"), Vector3::UNIT_SCALE);
        c.color = Color(color.x, color.y, color.z, 1);
        c.intensity = Float(n, "intensity", 1);
        c.enabled = n->BoolAttribute("enabled", true);
        c.shadows = n->BoolAttribute("shadows", true);
        c.autoFitShadow = n->BoolAttribute("autoFitShadow", true);
        if (c.intensity < 0 || color.x < 0 || color.y < 0 || color.z < 0)
            throw std::runtime_error("Invalid directional light");
        o->AddComponent<DirectionalLightComponent>(c);
    }
    if (auto n = e->FirstChildElement("Mesh"))
    {
        o->AddComponent<MeshSourceComponent>(BuiltinMesh(Enum(n, "primitive", 0, 4)), Str(n, "asset"),
                                             n->BoolAttribute("visible", true));
        SceneSerializer::RebuildMesh(w, id);
    }
    if (e->FirstChildElement("Material"))
    {
        if (!o->HasComponent<MaterialComponent>())
            o->AddComponent<MaterialComponent>();
        auto &list = o->GetComponent<MaterialComponent>().mMaterials;
        size_t index = 0;
        for (auto n = e->FirstChildElement("Material"); n; n = n->NextSiblingElement("Material"), ++index)
        {
            if (index >= list.size())
                throw std::runtime_error("Material count exceeds mesh slots");
            auto &m = list[index];
            auto color = n->FirstChildElement("BaseColor");
            auto rgb = Vec(color, Vector3::UNIT_SCALE);
            m.mBaseColor = Color(rgb.x, rgb.y, rgb.z, color ? Float(color, "alpha", 1) : 1);
            m.mEmissive = Vec(n->FirstChildElement("Emissive"));
            m.mFresnelF0 = Vec(n->FirstChildElement("Fresnel"), Vector3(0.04f, 0.04f, 0.04f));
            m.mMetallic = Float(n, "metallic", 0);
            m.mRoughness = Float(n, "roughness", 1);
            m.mNormalScale = Float(n, "normalScale", 1);
            m.mAO = Float(n, "ao", 1);
            m.useLight = n->UnsignedAttribute("useLight", 1);
            m.shadingModel = ShadingModel(Enum(n, "shadingModel", 0, 2));
            m.overrideModelTextures = n->BoolAttribute("overrideModelTextures", false);
            m.packedTextureChannels = n->UnsignedAttribute("packedTextureChannels", 0);
            if (m.packedTextureChannels > 3)
                throw std::runtime_error("Invalid packed texture channels");
            if (m.mMetallic < 0 || m.mMetallic > 1 || m.mRoughness < 0 || m.mRoughness > 1 || m.mAO < 0 || m.mAO > 1)
                throw std::runtime_error("Invalid PBR factor");
            if (auto uv = n->FirstChildElement("UV"))
                ReadTransform(uv, m.uvTransform);
            for (auto t = n->FirstChildElement("Texture"); t; t = t->NextSiblingElement("Texture"))
                m.texturePaths[Enum(t, "slot", 0, 6)] = Str(t, "path");
        }
    }
    if (auto n = e->FirstChildElement("Collider"))
    {
        o->AddComponent<ColliderComponent>();
        auto &c = o->GetComponent<ColliderComponent>();
        c.shape = ColliderShape(Enum(n, "shape", 0, 4));
        c.center = Vec(n->FirstChildElement("Center"));
        c.rotation = Quat(n->FirstChildElement("Rotation"));
        c.halfExtents = Vec(n->FirstChildElement("HalfExtents"), Vector3(.5f, .5f, .5f));
        c.radius = Float(n, "radius", .5f);
        c.halfHeight = Float(n, "halfHeight", .5f);
        c.trigger = n->BoolAttribute("trigger");
        c.visible = n->BoolAttribute("visible", true);
        if (c.radius <= 0 || c.halfHeight <= 0 || c.halfExtents.x <= 0 || c.halfExtents.y <= 0 || c.halfExtents.z <= 0)
            throw std::runtime_error("Invalid collider dimensions");
    }
    if (auto n = e->FirstChildElement("RigidBody"))
    {
        o->AddComponent<RigidBodyComponent>();
        auto &c = o->GetComponent<RigidBodyComponent>();
        c.motion = BodyMotion(Enum(n, "motion", 2, 2));
        c.mass = Float(n, "mass", 1);
        c.friction = Float(n, "friction", .5f);
        c.restitution = Float(n, "restitution", .1f);
        c.gravity = n->BoolAttribute("gravity", true);
        c.linearVelocity = Vec(n->FirstChildElement("Velocity"));
        if (c.mass <= 0 || c.friction < 0 || c.restitution < 0 || c.restitution > 1)
            throw std::runtime_error("Invalid rigid body settings");
    }
    if (e->FirstChildElement("Script"))
    {
        o->AddComponent<ScriptComponent>();
        for (auto n = e->FirstChildElement("Script"); n; n = n->NextSiblingElement("Script"))
        {
            ScriptAttachment s;
            s.assembly = Str(n, "assembly");
            s.type = Str(n, "type");
            s.enabled = n->BoolAttribute("enabled", true);
            for (auto f = n->FirstChildElement("Field"); f; f = f->NextSiblingElement("Field"))
                s.fields[Str(f, "name")] = Str(f, "value");
            o->GetComponent<ScriptComponent>().scripts.push_back(std::move(s));
        }
    }
    if (auto n = e->FirstChildElement("PrefabLink"))
    {
        auto *c = w.TryGet<PrefabInstanceComponent>(id);
        if (!c)
        {
            w.AddComponent<PrefabInstanceComponent>(id, Str(e, "prefab"));
            c = w.TryGet<PrefabInstanceComponent>(id);
        }
        c->sourceId = Str(n, "sourceId");
        c->instanceId = Str(n, "instanceId");
        if (auto baseline = n->FirstChildElement("Baseline"); baseline && baseline->GetText())
            c->baseline = baseline->GetText();
        if (!c->baseline.empty())
        {
            tinyxml2::XMLDocument baseline;
            if (baseline.Parse(c->baseline.c_str()) != XML_SUCCESS || !baseline.FirstChildElement("Object") ||
                c->sourceId.empty() || c->instanceId.empty() ||
                Str(baseline.FirstChildElement("Object"), "id") != c->sourceId)
                throw std::runtime_error("Invalid prefab baseline/link");
        }
        for (auto member = n->FirstChildElement("Member"); member; member = member->NextSiblingElement("Member"))
            c->members[Str(member, "source")] = Str(member, "target");
    }
    if (auto n = e->FirstChildElement("Joint"))
    {
        JointComponent c;
        c.type = JointType(Enum(n, "type", 0, 2));
        c.connectedBody = Str(n, "connectedBody");
        c.enabled = n->BoolAttribute("enabled", true);
        c.anchor = Vec(n->FirstChildElement("Anchor"));
        c.connectedAnchor = Vec(n->FirstChildElement("ConnectedAnchor"));
        c.axis = Vec(n->FirstChildElement("Axis"), Vector3::UP);
        c.minDistance = Float(n, "minDistance", 0);
        c.maxDistance = Float(n, "maxDistance", 1);
        c.minAngle = Float(n, "minAngle", -180);
        c.maxAngle = Float(n, "maxAngle", 180);
        if (c.axis.LengthSqr() < 1e-8f || c.minDistance < 0 || c.maxDistance < c.minDistance || c.minAngle < -180 ||
            c.minAngle > 0 || c.maxAngle < 0 || c.maxAngle > 180)
            throw std::runtime_error("Invalid joint settings");
        c.axis.Normalize();
        w.AddComponent<JointComponent>(id, c);
    }
    if (auto n = e->FirstChildElement("UI"))
    {
        o->AddComponent<UiElementComponent>();
        auto &c = o->GetComponent<UiElementComponent>();
        c.position = Vector2(Float(n, "x", 20), Float(n, "y", 20));
        c.size = Vector2(Float(n, "width", 200), Float(n, "height", 40));
        c.text = Str(n, "text");
        c.texture = Str(n, "texture");
        c.fontSize = Float(n, "fontSize", 20);
        c.interactable = n->BoolAttribute("interactable", false);
        c.layer = n->IntAttribute("layer", 0);
        auto bg = n->FirstChildElement("Background");
        if (bg)
        {
            auto rgb = Vec(bg);
            c.background = Color(rgb.x, rgb.y, rgb.z, Float(bg, "alpha", 1));
        }
        if (c.size.x <= 0 || c.size.y <= 0 || c.size.x > 4096 || c.size.y > 4096 || c.fontSize <= 0 || c.fontSize > 256)
            throw std::runtime_error("Invalid UI dimensions");
        c.visible = n->BoolAttribute("visible", true);
        auto rgb = Vec(n->FirstChildElement("Color"), Vector3::UNIT_SCALE);
        c.color = Color(rgb.x, rgb.y, rgb.z,
                        n->FirstChildElement("Color") ? Float(n->FirstChildElement("Color"), "alpha", 1) : 1);
    }
    guard.committed = true;
    return id;
}
void WriteSettings(XMLElement *root, const SceneSettings &s)
{
    auto n = Child(root, "Environment");
    n->SetAttribute("sky", s.environment.c_str());
    n->SetAttribute("diffuse", s.diffuseIBL.c_str());
    n->SetAttribute("specular", s.specularIBL.c_str());
    n->SetAttribute("brdf", s.brdfLut.c_str());
    n->SetAttribute("intensity", s.iblIntensity);
    n->SetAttribute("bias", s.iblBias);
    n->SetAttribute("exposure", s.exposure);
    n->SetAttribute("enabled", s.skyEnabled);
    n->SetAttribute("fxaa", s.fxaa);
    n->SetAttribute("autoExposure", s.autoExposure);
    if (s.hasEditorCamera)
    {
        auto c = Child(root, "EditorCamera");
        Vec(Child(c, "Position"), s.editorPosition);
        Quat(Child(c, "Rotation"), s.editorRotation);
        c->SetAttribute("fov", s.editorFov);
        c->SetAttribute("near", s.editorNear);
        c->SetAttribute("far", s.editorFar);
    }
}
void ReadSettings(const XMLElement *root, SceneSettings &s)
{
    if (auto n = root->FirstChildElement("Environment"))
    {
        s.environment = Str(n, "sky", s.environment.c_str());
        s.diffuseIBL = Str(n, "diffuse", s.diffuseIBL.c_str());
        s.specularIBL = Str(n, "specular", s.specularIBL.c_str());
        s.brdfLut = Str(n, "brdf", s.brdfLut.c_str());
        s.iblIntensity = Float(n, "intensity", 1);
        s.iblBias = Float(n, "bias", 0);
        s.exposure = Float(n, "exposure", 1);
        s.skyEnabled = n->BoolAttribute("enabled", true);
        s.fxaa = n->BoolAttribute("fxaa", true);
        s.autoExposure = n->BoolAttribute("autoExposure", true);
        if (s.iblIntensity < 0 || s.iblBias < 0 || s.exposure <= 0)
            throw std::runtime_error("Invalid environment settings");
    }
    if (auto c = root->FirstChildElement("EditorCamera"))
    {
        s.editorPosition = Vec(c->FirstChildElement("Position"));
        s.editorRotation = Quat(c->FirstChildElement("Rotation"));
        s.editorFov = Float(c, "fov", 45);
        s.editorNear = Float(c, "near", .1f);
        s.editorFar = Float(c, "far", 1000);
        if (s.editorFov <= 0 || s.editorFov >= 180 || s.editorNear <= 0 || s.editorFar <= s.editorNear)
            throw std::runtime_error("Invalid editor camera");
        s.hasEditorCamera = true;
    }
}
void SaveXml(tinyxml2::XMLDocument &doc, const std::filesystem::path &path)
{
    if (!path.parent_path().empty())
        std::filesystem::create_directories(path.parent_path());
    auto temp = path;
    temp += L".tmp";
    if (doc.SaveFile(temp.string().c_str()) != XML_SUCCESS)
        throw std::runtime_error("Cannot write scene file");
    // Replace atomically on Windows while preserving the old file on failure.
    if (!MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot replace scene file");
}
} // namespace
std::vector<AssetRequest> SceneSerializer::CollectResources(const std::filesystem::path &p)
{
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(p.string().c_str()) != tinyxml2::XML_SUCCESS)
        throw std::runtime_error("Cannot scan resources: " + p.string() + ": " + doc.ErrorStr());
    auto root = doc.FirstChildElement(p.extension() == L".prefab" ? "Prefab" : "Scene");
    if (!root || root->IntAttribute("version") != 1)
        throw std::runtime_error("Unsupported scene/prefab format");
    std::vector<AssetRequest> assets;
    if (auto environment = root->FirstChildElement("Environment"))
        for (auto attribute : {"sky", "diffuse", "specular", "brdf"})
        {
            auto path = Str(environment, attribute);
            if (!path.empty())
                assets.push_back({AssetKind::Texture, path, BuiltinMesh::None, kBlackCubeMap, false});
        }
    for (auto object = root->FirstChildElement("Object"); object; object = object->NextSiblingElement("Object"))
    {
        if (auto mesh = object->FirstChildElement("Mesh"))
        {
            auto type = BuiltinMesh(Enum(mesh, "primitive", 0, 4));
            assets.push_back(
                {type == BuiltinMesh::None ? AssetKind::Model : AssetKind::Primitive, Str(mesh, "asset"), type});
        }
        for (auto material = object->FirstChildElement("Material"); material;
             material = material->NextSiblingElement("Material"))
            for (auto texture = material->FirstChildElement("Texture"); texture;
                 texture = texture->NextSiblingElement("Texture"))
            {
                auto slot = Enum(texture, "slot", 0, 6);
                auto path = Str(texture, "path");
                if (!path.empty())
                    assets.push_back({AssetKind::Texture, path, BuiltinMesh::None,
                                      slot == 3 ? kDefaultNormalMap : kWhiteOpaque2D, slot == 0 || slot == 5});
            }
        if (auto ui = object->FirstChildElement("UI"))
        {
            auto path = Str(ui, "texture");
            if (!path.empty())
                assets.push_back({AssetKind::Texture, path, BuiltinMesh::None, kWhiteOpaque2D, true});
        }
    }
    return assets;
}
void SceneSerializer::RebuildMesh(World &w, Entity e)
{
    auto &source = w.GetComponent<MeshSourceComponent>(e);
    auto model = source.primitive == BuiltinMesh::None ? AssetManager::LoadModel(source.asset)
                                                       : Primitive::GetModel(source.primitive);
    if (!model)
        throw std::runtime_error("Mesh asset could not be loaded");
    w.RemoveComponent<MeshComponent>(e);
    w.RemoveComponent<MaterialComponent>(e);
    w.AddComponent<MaterialComponent>(e, model);
    w.AddComponent<MeshComponent>(e, model);
    w.GetComponent<MeshComponent>(e).SetIsRender(source.visible);
}
std::string SceneSerializer::ToXml(World &w, const std::string &name)
{
    ResolveHierarchy(w);
    tinyxml2::XMLDocument doc;
    auto root = Child(&doc, "Scene");
    root->SetAttribute("version", 1);
    root->SetAttribute("name", name.c_str());
    WriteSettings(root, w.settings);
    std::vector<Entity> objects;
    for (auto e : w.View<PersistentIdComponent>())
        objects.push_back(e);
    std::sort(objects.begin(), objects.end(), [&](Entity a, Entity b) {
        return w.GetComponent<PersistentIdComponent>(a).value < w.GetComponent<PersistentIdComponent>(b).value;
    });
    for (auto e : objects)
        WriteObject(w, e, Child(root, "Object"));
    tinyxml2::XMLPrinter printer;
    doc.Print(&printer);
    return printer.CStr();
}
std::string SceneSerializer::FromXml(World &w, std::string_view xml)
{
    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.data(), xml.size()) != tinyxml2::XML_SUCCESS)
        throw std::runtime_error(doc.ErrorStr());
    auto root = doc.FirstChildElement("Scene");
    if (!root || root->IntAttribute("version") != 1)
        throw std::runtime_error("Unsupported scene format");
    World staged;
    ReadSettings(root, staged.settings);
    std::set<std::string> ids;
    for (auto n = root->FirstChildElement("Object"); n; n = n->NextSiblingElement("Object"))
    {
        if (!ids.insert(Str(n, "id")).second)
            throw std::runtime_error("Duplicate persistent ID");
        ReadObject(staged, n, false);
    }
    ResolveHierarchy(staged);
    w.ReplaceWith(staged);
    return Str(root, "name", "Scene");
}
Entity SceneSerializer::Duplicate(World &w, Entity source)
{
    if (!w.IsValid(source))
        throw std::runtime_error("Select an object to duplicate");
    ResolveHierarchy(w);
    std::set<std::string> selected{w.GetComponent<PersistentIdComponent>(source).value};
    bool more = true;
    while (more)
    {
        more = false;
        for (auto e : w.View<HierarchyComponent, PersistentIdComponent>())
            if (selected.contains(w.GetComponent<HierarchyComponent>(e).parentId))
                more |= selected.insert(w.GetComponent<PersistentIdComponent>(e).value).second;
    }
    tinyxml2::XMLDocument doc;
    auto root = Child(&doc, "Objects");
    for (auto e : w.View<PersistentIdComponent>())
        if (selected.contains(w.GetComponent<PersistentIdComponent>(e).value))
            WriteObject(w, e, Child(root, "Object"));
    std::map<std::string, std::string> remap;
    std::vector<Entity> created;
    Entity result = entt::null;
    try
    {
        for (auto n = root->FirstChildElement("Object"); n; n = n->NextSiblingElement("Object"))
        {
            auto e = ReadObject(w, n, true);
            created.push_back(e);
            auto old = Str(n, "id");
            remap.emplace(old, w.GetComponent<PersistentIdComponent>(e).value);
            if (old == w.GetComponent<PersistentIdComponent>(source).value)
                result = e;
        }
        for (auto e : created)
            if (auto h = w.TryGet<HierarchyComponent>(e); h && remap.contains(h->parentId))
                h->parentId = remap.at(h->parentId);
        for (auto e : created)
        {
            if (auto j = w.TryGet<JointComponent>(e); j && remap.contains(j->connectedBody))
                j->connectedBody = remap.at(j->connectedBody);
            if (auto p = w.TryGet<PrefabInstanceComponent>(e))
            {
                if (!p->instanceId.empty() && !remap.contains(p->instanceId))
                {
                    w.RemoveComponent<PrefabInstanceComponent>(e);
                    continue;
                }
                if (remap.contains(p->instanceId))
                    p->instanceId = remap.at(p->instanceId);
                for (auto &[source, target] : p->members)
                    if (remap.contains(target))
                        target = remap.at(target);
                    else
                    {
                        World temporary;
                        auto object = temporary.CreateGameObject();
                        target = object->GetComponent<PersistentIdComponent>().value;
                    }
            }
        }
        auto name = w.TryGet<NameComponent>(source);
        std::string base = name ? name->value : "Object", copy = base + " (Copy)";
        for (int n = 2; w.IsValid(w.FindByName(copy)); ++n)
            copy = base + " (Copy " + std::to_string(n) + ")";
        w.SetName(result, copy);
        ResolveHierarchy(w);
        return result;
    }
    catch (...)
    {
        for (auto e : created)
            w.DestroyGameObject(e);
        ResolveHierarchy(w);
        throw;
    }
}
void SceneSerializer::Save(World &w, const std::filesystem::path &p, const std::string &name)
{
    if (p.extension() != L".scene")
        throw std::runtime_error("Scenes must use .scene extension");
    ResolveHierarchy(w);
    tinyxml2::XMLDocument d;
    auto root = Child(&d, "Scene");
    root->SetAttribute("version", 1);
    root->SetAttribute("name", name.c_str());
    WriteSettings(root, w.settings);
    for (auto e : w.View<PersistentIdComponent>())
        WriteObject(w, e, Child(root, "Object"));
    SaveXml(d, p);
}
std::string SceneSerializer::Load(World &w, const std::filesystem::path &p)
{
    if (p.extension() != L".scene")
        throw std::runtime_error("Scenes must use .scene extension");
    tinyxml2::XMLDocument d;
    if (d.LoadFile(p.string().c_str()) != XML_SUCCESS)
        throw std::runtime_error(d.ErrorStr());
    auto root = d.FirstChildElement("Scene");
    if (!root || root->IntAttribute("version") != 1)
        throw std::runtime_error("Unsupported scene format");
    ScopedLogTimer timer("Scene", "Load " + p.string());
    auto preload = AssetManager::Preload(CollectResources(p));
    if (!preload->Report().Success())
        throw std::runtime_error("Scene preload failed: " + preload->Report().errors.front());
    World staged;
    ReadSettings(root, staged.settings);
    std::set<std::string> ids;
    for (auto n = root->FirstChildElement("Object"); n; n = n->NextSiblingElement("Object"))
    {
        if (!ids.insert(Str(n, "id")).second)
            throw std::runtime_error("Duplicate persistent ID");
        ReadObject(staged, n, false);
    }
    ResolveHierarchy(staged);
    RefreshPrefabs(staged);
    w.ReplaceWith(staged);
    return Str(root, "name", "Scene");
}
void SceneSerializer::ResolveHierarchy(World &w)
{
    std::map<std::string, Entity> ids;
    for (auto e : w.View<PersistentIdComponent>())
        if (!ids.emplace(w.GetComponent<PersistentIdComponent>(e).value, e).second)
            throw std::runtime_error("Duplicate object ID");
    for (auto e : w.View<TransformComponent>())
        w.GetComponent<TransformComponent>(e).parent = nullptr;
    for (auto e : w.View<HierarchyComponent, TransformComponent>())
    {
        auto key = w.GetComponent<HierarchyComponent>(e).parentId;
        if (key.empty())
            continue;
        std::set<Entity> chain{e};
        Entity p = e;
        while (auto c = w.TryGet<HierarchyComponent>(p))
        {
            if (c->parentId.empty())
                break;
            auto it = ids.find(c->parentId);
            if (it == ids.end())
                throw std::runtime_error("Missing parent object");
            p = it->second;
            if (!chain.insert(p).second)
                throw std::runtime_error("Hierarchy cycle");
        }
        auto parent = w.TryGet<TransformComponent>(ids.at(key));
        if (!parent)
            throw std::runtime_error("Parent has no transform");
        w.GetComponent<TransformComponent>(e).parent = parent;
    }
}
void SceneSerializer::SavePrefab(World &w, Entity root, const std::filesystem::path &p)
{
    if (!w.IsValid(root))
        throw std::runtime_error("No prefab object selected");
    ResolveHierarchy(w);
    tinyxml2::XMLDocument d;
    auto node = Child(&d, "Prefab");
    node->SetAttribute("version", 1);
    node->SetAttribute("root", w.GetComponent<PersistentIdComponent>(root).value.c_str());
    std::set<std::string> selected{w.GetComponent<PersistentIdComponent>(root).value};
    bool added = true;
    while (added)
    {
        added = false;
        for (auto e : w.View<HierarchyComponent, PersistentIdComponent>())
        {
            auto &h = w.GetComponent<HierarchyComponent>(e);
            auto &id = w.GetComponent<PersistentIdComponent>(e).value;
            if (selected.contains(h.parentId))
                added |= selected.insert(id).second;
        }
    }
    for (auto e : w.View<PersistentIdComponent>())
        if (selected.contains(w.GetComponent<PersistentIdComponent>(e).value))
        {
            auto n = Child(node, "Object");
            WriteObject(w, e, n);
            if (e == root)
                n->DeleteAttribute("parent");
        }
    SaveXml(d, p);
}
Entity SceneSerializer::InstantiatePrefab(World &w, const std::filesystem::path &p)
{
    tinyxml2::XMLDocument d;
    if (d.LoadFile(p.string().c_str()) != XML_SUCCESS)
        throw std::runtime_error(d.ErrorStr());
    auto root = d.FirstChildElement("Prefab");
    if (!root || root->IntAttribute("version") != 1)
        throw std::runtime_error("Unsupported prefab format");
    ScopedLogTimer timer("Scene", "Instantiate prefab " + p.string());
    auto preload = AssetManager::Preload(CollectResources(p));
    if (!preload->Report().Success())
        throw std::runtime_error("Prefab preload failed: " + preload->Report().errors.front());
    std::map<std::string, std::string> ids;
    std::vector<Entity> created;
    Entity result = entt::null;
    try
    {
        for (auto n = root->FirstChildElement("Object"); n; n = n->NextSiblingElement("Object"))
        {
            auto e = ReadObject(w, n, true);
            created.push_back(e);
            auto old = Str(n, "id");
            if (!ids.emplace(old, w.GetComponent<PersistentIdComponent>(e).value).second)
                throw std::runtime_error("Duplicate prefab ID");
            if (old == Str(root, "root"))
                result = e;
        }
        if (result == entt::null)
            throw std::runtime_error("Prefab has no root");
        for (auto e : created)
            if (auto h = w.TryGet<HierarchyComponent>(e); h && !h->parentId.empty())
                h->parentId = ids.at(h->parentId);
        if (auto instance = w.TryGet<PrefabInstanceComponent>(result))
            instance->source = p.generic_string();
        else
            w.AddComponent<PrefabInstanceComponent>(result, p.generic_string());
        auto instanceId = w.GetComponent<PersistentIdComponent>(result).value;
        for (auto n = root->FirstChildElement("Object"); n; n = n->NextSiblingElement("Object"))
        {
            auto old = Str(n, "id");
            Entity e = entt::null;
            for (auto candidate : created)
                if (w.GetComponent<PersistentIdComponent>(candidate).value == ids.at(old))
                    e = candidate;
            if (!w.HasComponent<PrefabInstanceComponent>(e))
                w.AddComponent<PrefabInstanceComponent>(e);
            auto &link = w.GetComponent<PrefabInstanceComponent>(e);
            link.source = p.generic_string();
            link.sourceId = old;
            link.instanceId = instanceId;
            tinyxml2::XMLPrinter printer;
            n->Accept(&printer);
            link.baseline = printer.CStr();
            if (e == result)
                link.members = ids;
        }
        for (auto e : created)
            if (auto j = w.TryGet<JointComponent>(e); j && ids.contains(j->connectedBody))
                j->connectedBody = ids.at(j->connectedBody);
        ResolveHierarchy(w);
        return result;
    }
    catch (...)
    {
        for (auto e : created)
            w.DestroyGameObject(e);
        throw;
    }
}
namespace
{
std::string XmlText(const XMLNode *node)
{
    XMLPrinter p;
    node->Accept(&p);
    return p.CStr();
}
std::string ElementKey(const XMLElement *e, size_t index)
{
    std::string key = e->Name();
    if (key == "Field" || key == "Texture")
        return key + ":" + Str(e, key == "Field" ? "name" : "slot");
    return key + ":" + std::to_string(index);
}
std::map<std::string, const XMLElement *> Children(const XMLElement *node)
{
    std::map<std::string, size_t> counts;
    std::map<std::string, const XMLElement *> out;
    for (auto e = node->FirstChildElement(); e; e = e->NextSiblingElement())
        out[ElementKey(e, counts[e->Name()]++)] = e;
    return out;
}
// Three-way merge: source changes propagate only where instance values still equal its previous baseline.
void MergeOverrides(const XMLElement *old, const XMLElement *live, XMLElement *next)
{
    for (auto a = old->FirstAttribute(); a; a = a->Next())
        if (!live->Attribute(a->Name()))
            next->DeleteAttribute(a->Name());
    for (auto a = live->FirstAttribute(); a; a = a->Next())
        if (Str(old, a->Name()) != a->Value())
            next->SetAttribute(a->Name(), a->Value());
    auto previous = Children(old), current = Children(live), updated = Children(next);
    for (auto &[key, before] : previous)
        if (!current.contains(key) && updated.contains(key))
            next->DeleteChild(const_cast<XMLElement *>(updated.at(key)));
    for (auto &[key, value] : current)
    {
        if (!previous.contains(key))
        {
            if (updated.contains(key))
                next->DeleteChild(const_cast<XMLElement *>(updated.at(key)));
            next->InsertEndChild(value->DeepClone(next->GetDocument()));
        }
        else if (updated.contains(key))
            MergeOverrides(previous.at(key), value, const_cast<XMLElement *>(updated.at(key)));
        else if (XmlText(previous.at(key)) != XmlText(value))
            next->InsertEndChild(value->DeepClone(next->GetDocument()));
    }
}
void StripLink(XMLElement *object)
{
    object->DeleteAttribute("prefab");
    if (auto link = object->FirstChildElement("PrefabLink"))
        object->DeleteChild(link);
}
void RemapObject(XMLElement *object, const std::map<std::string, std::string> &ids)
{
    if (auto parent = object->Attribute("parent"); parent && ids.contains(parent))
        object->SetAttribute("parent", ids.at(parent).c_str());
    if (auto joint = object->FirstChildElement("Joint"))
        if (auto target = joint->Attribute("connectedBody"); target && ids.contains(target))
            joint->SetAttribute("connectedBody", ids.at(target).c_str());
}
} // namespace
size_t SceneSerializer::PrefabOverrideCount(World &w, Entity e)
{
    return PrefabOverrides(w, e).size();
}
std::vector<std::string> SceneSerializer::PrefabOverrides(World &w, Entity e)
{
    auto link = w.TryGet<PrefabInstanceComponent>(e);
    if (!link || link->baseline.empty())
        return {};
    tinyxml2::XMLDocument old, live;
    if (old.Parse(link->baseline.c_str()) != XML_SUCCESS)
        throw std::runtime_error("Invalid prefab baseline");
    auto object = Child(&live, "Object");
    WriteObject(w, e, object);
    StripLink(object);
    object->SetAttribute("id", link->sourceId.c_str());
    std::map<std::string, std::string> ids;
    for (auto item : w.View<PrefabInstanceComponent, PersistentIdComponent>())
    {
        auto &c = w.GetComponent<PrefabInstanceComponent>(item);
        if (c.instanceId == link->instanceId)
            ids[w.GetComponent<PersistentIdComponent>(item).value] = c.sourceId;
    }
    RemapObject(object, ids);
    std::vector<std::string> changes;
    auto before = old.FirstChildElement("Object");
    std::function<void(const XMLElement *, const XMLElement *, const std::string &)> diff;
    diff = [&](const XMLElement *base, const XMLElement *current, const std::string &path) {
        for (auto a = current->FirstAttribute(); a; a = a->Next())
            if (!base->Attribute(a->Name()) || Str(base, a->Name()) != a->Value())
                changes.push_back(path + "/@" + a->Name() + ": " + Str(base, a->Name(), "<unset>") + " -> " +
                                  a->Value());
        for (auto a = base->FirstAttribute(); a; a = a->Next())
            if (!current->Attribute(a->Name()))
                changes.push_back(path + "/@" + a->Name() + " (removed)");
        auto previous = Children(base), live = Children(current);
        for (auto &[key, value] : live)
            if (!previous.contains(key))
                changes.push_back(path + "/" + key + " (added)");
            else
                diff(previous.at(key), value, path + "/" + key);
        for (auto &[key, value] : previous)
            if (!live.contains(key))
                changes.push_back(path + "/" + key + " (removed)");
    };
    diff(before, object, "Object");
    return changes;
}
void SceneSerializer::RefreshPrefabs(World &w, const std::filesystem::path &filter)
{
    bool linked = false;
    for (auto e : w.View<PrefabInstanceComponent>())
        linked |= !w.GetComponent<PrefabInstanceComponent>(e).members.empty();
    if (!linked)
        return;
    tinyxml2::XMLDocument scene;
    scene.Parse(ToXml(w, "Prefab refresh").c_str());
    auto root = scene.FirstChildElement("Scene");
    std::map<std::string, XMLElement *> objects;
    for (auto n = root->FirstChildElement("Object"); n; n = n->NextSiblingElement("Object"))
        objects[Str(n, "id")] = n;
    for (auto e : w.View<PrefabInstanceComponent>())
    {
        const auto &instance = w.GetComponent<PrefabInstanceComponent>(e);
        if (instance.members.empty() || instance.baseline.empty())
            continue;
        if (!filter.empty() && std::filesystem::absolute(instance.source).lexically_normal() !=
                                   std::filesystem::absolute(filter).lexically_normal())
            continue;
        tinyxml2::XMLDocument prefab;
        if (prefab.LoadFile(instance.source.c_str()) != XML_SUCCESS)
            throw std::runtime_error("Cannot refresh prefab: " + instance.source);
        auto source = prefab.FirstChildElement("Prefab");
        if (!source || source->IntAttribute("version") != 1)
            throw std::runtime_error("Invalid prefab source");
        auto ids = instance.members;
        std::set<std::string> sourceIds;
        for (auto n = source->FirstChildElement("Object"); n; n = n->NextSiblingElement("Object"))
        {
            auto id = Str(n, "id");
            if (!sourceIds.insert(id).second)
                throw std::runtime_error("Duplicate prefab source ID");
            if (!ids.contains(id))
            {
                World temporary;
                auto obj = temporary.CreateGameObject();
                ids[id] = obj->GetComponent<PersistentIdComponent>().value;
            }
        }
        if (!sourceIds.contains(Str(source, "root")))
            throw std::runtime_error("Prefab source has no root object");
        std::map<std::string, std::string> reverse;
        for (auto &[a, b] : ids)
            reverse[b] = a;
        for (auto &[id, target] : instance.members)
            if (!sourceIds.contains(id) && objects.contains(target))
            {
                root->DeleteChild(objects.at(target));
                objects.erase(target);
            }
        std::erase_if(ids, [&](const auto &entry) { return !sourceIds.contains(entry.first); });
        for (auto n = source->FirstChildElement("Object"); n; n = n->NextSiblingElement("Object"))
        {
            auto id = Str(n, "id"), target = ids.at(id);
            // Missing previously known scene objects are instance deletion overrides.
            if (instance.members.contains(id) && !objects.contains(target))
                continue;
            auto merged = n->DeepClone(&scene)->ToElement();
            StripLink(merged);
            auto baseline = XmlText(merged);
            if (objects.contains(target))
            {
                auto current = objects.at(target);
                auto link = current->FirstChildElement("PrefabLink");
                tinyxml2::XMLDocument old, live;
                auto text = link ? link->FirstChildElement("Baseline") : nullptr;
                if (text && text->GetText() && old.Parse(text->GetText()) == XML_SUCCESS)
                {
                    auto normalized = current->DeepClone(&live)->ToElement();
                    live.InsertEndChild(normalized);
                    StripLink(normalized);
                    normalized->SetAttribute("id", id.c_str());
                    RemapObject(normalized, reverse);
                    MergeOverrides(old.FirstChildElement("Object"), normalized, merged);
                }
                root->DeleteChild(current);
            }
            merged->SetAttribute("id", target.c_str());
            RemapObject(merged, ids);
            merged->SetAttribute("prefab", instance.source.c_str());
            auto link = Child(merged, "PrefabLink");
            link->SetAttribute("sourceId", id.c_str());
            link->SetAttribute("instanceId", instance.instanceId.c_str());
            Child(link, "Baseline")->SetText(baseline.c_str());
            if (id == Str(source, "root"))
                for (auto &[a, b] : ids)
                {
                    auto member = Child(link, "Member");
                    member->SetAttribute("source", a.c_str());
                    member->SetAttribute("target", b.c_str());
                }
            root->InsertEndChild(merged);
            objects[target] = merged;
        }
    }
    FromXml(w, XmlText(&scene));
}
void SceneSerializer::RevertPrefab(World &w, Entity e)
{
    const auto *selected = w.TryGet<PrefabInstanceComponent>(e);
    if (!selected)
        throw std::runtime_error("Select a prefab instance");
    auto group = selected->instanceId;
    std::set<std::string> subtree{group};
    bool more = true;
    while (more)
    {
        more = false;
        for (auto item : w.View<HierarchyComponent, PersistentIdComponent>())
            if (subtree.contains(w.GetComponent<HierarchyComponent>(item).parentId))
                more |= subtree.insert(w.GetComponent<PersistentIdComponent>(item).value).second;
    }
    PrefabInstanceComponent rootLink;
    for (auto item : w.View<PrefabInstanceComponent>())
        if (w.GetComponent<PrefabInstanceComponent>(item).instanceId == group &&
            !w.GetComponent<PrefabInstanceComponent>(item).members.empty())
            rootLink = w.GetComponent<PrefabInstanceComponent>(item);
    tinyxml2::XMLDocument scene;
    scene.Parse(ToXml(w, "Prefab revert").c_str());
    auto root = scene.FirstChildElement("Scene");
    std::vector<XMLElement *> originals;
    for (auto n = root->FirstChildElement("Object"); n; n = n->NextSiblingElement("Object"))
        originals.push_back(n);
    for (auto n : originals)
    {
        auto link = n->FirstChildElement("PrefabLink");
        if (subtree.contains(Str(n, "id")) && (!link || Str(link, "instanceId") != group))
        {
            root->DeleteChild(n);
            continue;
        }
        if (link && Str(link, "instanceId") == group)
        {
            tinyxml2::XMLDocument baseline;
            auto b = link->FirstChildElement("Baseline");
            if (!b || !b->GetText() || baseline.Parse(b->GetText()) != XML_SUCCESS)
                throw std::runtime_error("Invalid prefab baseline");
            auto replacement = baseline.FirstChildElement("Object")->DeepClone(&scene)->ToElement();
            replacement->SetAttribute("id", Str(n, "id").c_str());
            replacement->SetAttribute("prefab", Str(n, "prefab").c_str());
            std::map<std::string, std::string> ids;
            for (auto item : w.View<PrefabInstanceComponent, PersistentIdComponent>())
            {
                auto &c = w.GetComponent<PrefabInstanceComponent>(item);
                if (c.instanceId == group)
                    ids[c.sourceId] = w.GetComponent<PersistentIdComponent>(item).value;
            }
            RemapObject(replacement, ids);
            replacement->InsertEndChild(link->DeepClone(&scene));
            root->InsertEndChild(replacement);
            root->DeleteChild(n);
        }
    }
    tinyxml2::XMLDocument source;
    if (source.LoadFile(rootLink.source.c_str()) != XML_SUCCESS || !source.FirstChildElement("Prefab"))
        throw std::runtime_error("Cannot revert prefab source");
    std::set<std::string> present;
    for (auto n = root->FirstChildElement("Object"); n; n = n->NextSiblingElement("Object"))
        present.insert(Str(n, "id"));
    for (auto n = source.FirstChildElement("Prefab")->FirstChildElement("Object"); n;
         n = n->NextSiblingElement("Object"))
    {
        auto id = Str(n, "id");
        if (!rootLink.members.contains(id) || present.contains(rootLink.members.at(id)))
            continue;
        auto replacement = n->DeepClone(&scene)->ToElement();
        StripLink(replacement);
        auto baseline = XmlText(replacement);
        replacement->SetAttribute("id", rootLink.members.at(id).c_str());
        RemapObject(replacement, rootLink.members);
        replacement->SetAttribute("prefab", rootLink.source.c_str());
        auto link = Child(replacement, "PrefabLink");
        link->SetAttribute("sourceId", id.c_str());
        link->SetAttribute("instanceId", group.c_str());
        Child(link, "Baseline")->SetText(baseline.c_str());
        root->InsertEndChild(replacement);
    }
    FromXml(w, XmlText(&scene));
    RefreshPrefabs(w, rootLink.source);
}
void SceneSerializer::ApplyPrefab(World &w, Entity e)
{
    auto *selected = w.TryGet<PrefabInstanceComponent>(e);
    if (!selected || selected->instanceId.empty())
        throw std::runtime_error("Select a linked prefab instance");
    auto group = selected->instanceId, path = selected->source;
    tinyxml2::XMLDocument prefab;
    auto root = Child(&prefab, "Prefab");
    root->SetAttribute("version", 1);
    std::map<std::string, std::string> ids;
    for (auto item : w.View<PrefabInstanceComponent, PersistentIdComponent>())
    {
        auto &c = w.GetComponent<PrefabInstanceComponent>(item);
        if (c.instanceId == group)
            ids[w.GetComponent<PersistentIdComponent>(item).value] = c.sourceId;
    }
    if (!ids.contains(group))
        throw std::runtime_error("Prefab root missing");
    std::set<std::string> subtree{group};
    bool more = true;
    while (more)
    {
        more = false;
        for (auto item : w.View<HierarchyComponent, PersistentIdComponent>())
            if (subtree.contains(w.GetComponent<HierarchyComponent>(item).parentId))
                more |= subtree.insert(w.GetComponent<PersistentIdComponent>(item).value).second;
    }
    for (const auto &id : subtree)
        if (!ids.contains(id))
            ids[id] = id;
    root->SetAttribute("root", ids.at(group).c_str());
    for (auto item : w.View<PersistentIdComponent>())
        if (subtree.contains(w.GetComponent<PersistentIdComponent>(item).value))
        {
            auto n = Child(root, "Object");
            WriteObject(w, item, n);
            StripLink(n);
            auto instanceId = Str(n, "id");
            n->SetAttribute("id", ids.at(instanceId).c_str());
            RemapObject(n, ids);
            if (instanceId == group)
                n->DeleteAttribute("parent");
        }
    SaveXml(prefab, std::filesystem::path(path));
    std::map<std::string, std::string> members;
    for (const auto &[target, source] : ids)
        members[source] = target;
    for (auto item : w.View<PersistentIdComponent>())
    {
        auto target = w.GetComponent<PersistentIdComponent>(item).value;
        if (!subtree.contains(target))
            continue;
        if (!w.HasComponent<PrefabInstanceComponent>(item))
        {
            PrefabInstanceComponent link;
            link.source = path;
            link.instanceId = group;
            link.sourceId = ids.at(target);
            w.AddComponent<PrefabInstanceComponent>(item, link);
        }
        if (target == group)
            w.GetComponent<PrefabInstanceComponent>(item).members = members;
    }
    RefreshPrefabs(w, std::filesystem::path(path));
}
} // namespace AtomEngine
