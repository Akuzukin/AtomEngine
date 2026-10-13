#include "EditorDocument.h"
#include "Runtime/Function/Framework/Component/MeshComponent.h"
#include <cmath>
#include <set>
namespace AtomEngine
{
void EditorDocument::Reset(World &w, const std::string &name)
{
    mName = name;
    mStates = {{SceneSerializer::ToXml(w, name), "", "Initial scene", 0}};
    mCursor = 0;
    mSavedId = 0;
    mNextId = 0;
}
bool EditorDocument::Capture(World &w, const std::string &name, const std::string &selection, const std::string &label)
{
    mName = name;
    auto xml = SceneSerializer::ToXml(w, name);
    if (mStates.empty())
    {
        Reset(w, name);
        return false;
    }
    if (xml == mStates[mCursor].xml)
        return false;
    mStates.resize(mCursor + 1);
    mStates.push_back({std::move(xml), selection, label, ++mNextId});
    mCursor = mStates.size() - 1;
    size_t bytes = 0;
    for (auto &state : mStates)
        bytes += state.xml.size();
    while (mStates.size() > 1 && (mStates.size() > 100 || bytes > 16 * 1024 * 1024))
    {
        bytes -= mStates.front().xml.size();
        mStates.erase(mStates.begin());
        --mCursor;
    }
    return true;
}
void EditorDocument::MarkSaved(World &w, const std::string &name)
{
    Capture(w, name, "");
    mSavedId = mStates[mCursor].id;
}
void EditorDocument::SyncRestoredState(World &w)
{
    if (!mStates.empty())
        mStates[mCursor].xml = SceneSerializer::ToXml(w, mName);
}
std::string EditorDocument::Undo(World &w)
{
    if (!CanUndo())
        return {};
    auto &state = mStates[mCursor - 1];
    mName = SceneSerializer::FromXml(w, state.xml);
    --mCursor;
    state.xml = SceneSerializer::ToXml(w, mName);
    return state.selection;
}
std::string EditorDocument::Redo(World &w)
{
    if (!CanRedo())
        return {};
    auto &state = mStates[mCursor + 1];
    mName = SceneSerializer::FromXml(w, state.xml);
    ++mCursor;
    state.xml = SceneSerializer::ToXml(w, mName);
    return state.selection;
}
const std::string &EditorDocument::UndoLabel() const
{
    static const std::string empty;
    return mStates.empty() ? empty : mStates[mCursor].label;
}
Entity EditorActions::Find(World &w, const std::string &id)
{
    for (auto e : w.View<PersistentIdComponent>())
        if (w.GetComponent<PersistentIdComponent>(e).value == id)
            return e;
    return entt::null;
}
std::string EditorActions::Id(World &w, Entity e)
{
    auto id = w.TryGet<PersistentIdComponent>(e);
    return id ? id->value : "";
}
void EditorActions::DeleteTree(World &w, Entity root)
{
    if (!w.IsValid(root))
        return;
    std::set<std::string> ids{Id(w, root)};
    bool more = true;
    while (more)
    {
        more = false;
        for (auto e : w.View<HierarchyComponent, PersistentIdComponent>())
            if (ids.contains(w.GetComponent<HierarchyComponent>(e).parentId))
                more |= ids.insert(Id(w, e)).second;
    }
    std::vector<Entity> objects;
    for (auto e : w.View<PersistentIdComponent>())
        if (ids.contains(Id(w, e)))
            objects.push_back(e);
    for (auto e : objects)
        w.DestroyGameObject(e);
    SceneSerializer::ResolveHierarchy(w);
}
namespace
{
Transform Decompose(const Matrix4x4 &m)
{
    Transform result;
    result.scale = m.GetScale();
    result.rotation = m.GetRotation();
    result.transition = m.GetTrans();
    if (!std::isfinite(result.scale.x + result.scale.y + result.scale.z) ||
        std::min({result.scale.x, result.scale.y, result.scale.z}) < .001f)
        throw std::runtime_error("Scale must be positive and at least 0.001");
    auto composed = result.GetMatrix();
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            if (!std::isfinite(m[i][j]) || std::abs(composed[i][j] - m[i][j]) > 1e-3f * (1 + std::abs(m[i][j])))
                throw std::runtime_error("This transform introduces shear or reflection. Use uniform parent scale or "
                                         "local transform fields.");
    return result;
}
} // namespace
void EditorActions::SetWorldMatrix(World &w, Entity e, const Matrix4x4 &matrix)
{
    auto &t = w.GetComponent<TransformComponent>(e);
    auto local = t.parent ? matrix * t.parent->GetMatrix().Inverse() : matrix;
    auto value = Decompose(local);
    t.transition = value.transition;
    t.rotation = value.rotation;
    t.scale = value.scale;
}
void EditorActions::Reparent(World &w, Entity child, Entity parent)
{
    if (!w.IsValid(child) || !w.HasComponent<TransformComponent>(child))
        throw std::runtime_error("Object has no transform");
    if (parent != entt::null && (!w.IsValid(parent) || !w.HasComponent<TransformComponent>(parent)))
        throw std::runtime_error("Parent has no transform");
    auto p = parent;
    std::set<Entity> visited;
    while (p != entt::null)
    {
        if (p == child || !visited.insert(p).second)
            throw std::runtime_error("Parent would create a hierarchy cycle");
        auto h = w.TryGet<HierarchyComponent>(p);
        p = h ? Find(w, h->parentId) : entt::null;
    }
    auto &t = w.GetComponent<TransformComponent>(child);
    auto world = t.GetMatrix();
    auto local =
        parent == entt::null ? world : world * w.GetComponent<TransformComponent>(parent).GetMatrix().Inverse();
    auto value = Decompose(local);
    if (parent == entt::null)
        w.RemoveComponent<HierarchyComponent>(child);
    else if (auto h = w.TryGet<HierarchyComponent>(child))
        h->parentId = Id(w, parent);
    else
        w.AddComponent<HierarchyComponent>(child, Id(w, parent));
    t.transition = value.transition;
    t.rotation = value.rotation;
    t.scale = value.scale;
    SceneSerializer::ResolveHierarchy(w);
}
Vector3 EditorActions::FrameSelection(World &w, Entity e, Camera &camera)
{
    auto t = w.TryGet<TransformComponent>(e);
    if (!t)
        throw std::runtime_error("Object has no transform");
    auto matrix = t->GetMatrix();
    auto center = matrix.GetTrans();
    float radius = 1;
    if (auto mesh = w.TryGet<MeshComponent>(e); mesh && mesh->GetModel())
    {
        auto sphere = mesh->GetModel()->mBoundingSphere;
        center = Math::TransformCoord(sphere.GetCenter(), matrix);
        auto scale = matrix.GetScale();
        radius = std::max(.1f, sphere.GetRadius() * std::max({scale.x, scale.y, scale.z}));
    }
    float distance = radius / std::tan(camera.GetFOV() * .5f) * 1.4f;
    camera.SetLookAt(center - camera.GetForwardVec().NormalizedCopy() * std::max(distance, 1.f), center,
                     Vector3(0, 1, 0));
    camera.Update();
    return center;
}
} // namespace AtomEngine
