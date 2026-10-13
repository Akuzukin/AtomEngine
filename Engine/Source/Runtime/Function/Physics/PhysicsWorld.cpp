#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Geometry/ConvexHullBuilder.h>
#include "CollisionGeometry.h"
#include "PhysicsWorld.h"
#include "Runtime/Function/Framework/Component/SceneComponents.h"
#include "Runtime/Function/Framework/Component/TransformComponent.h"
#include <set>
#include <sstream>
#include <algorithm>
namespace AtomEngine
{
namespace
{
int users = 0;
JPH::Vec3 V(const Vector3 &v)
{
    return {v.x, v.y, v.z};
}
JPH::Quat Q(const Quaternion &q)
{
    return JPH::Quat(q.x, q.y, q.z, q.w).Normalized();
}
class Layers : public JPH::BroadPhaseLayerInterface
{
  public:
    uint32_t GetNumBroadPhaseLayers() const override
    {
        return 2;
    }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer l) const override
    {
        return JPH::BroadPhaseLayer(uint8_t(l));
    }
};
class PairFilter : public JPH::ObjectLayerPairFilter
{
  public:
    bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override
    {
        return a != 0 || b != 0;
    }
};
class BroadFilter : public JPH::ObjectVsBroadPhaseLayerFilter
{
  public:
    bool ShouldCollide(JPH::ObjectLayer a, JPH::BroadPhaseLayer b) const override
    {
        return a != 0 || b.GetValue() != 0;
    }
};
std::string Signature(const ColliderComponent &c, const RigidBodyComponent &b, const Vector3 &s)
{
    std::ostringstream out;
    out << int(c.shape) << ',' << c.center.x << ',' << c.center.y << ',' << c.center.z << ',' << c.rotation.x << ','
        << c.rotation.y << ',' << c.rotation.z << ',' << c.rotation.w << ',' << c.halfExtents.x << ','
        << c.halfExtents.y << ',' << c.halfExtents.z << ',' << c.radius << ',' << c.halfHeight << ',' << c.trigger
        << ',' << int(b.motion) << ',' << b.mass << ',' << b.friction << ',' << b.restitution << ',' << b.gravity << ','
        << s.x << ',' << s.y << ',' << s.z;
    return out.str();
}
} // namespace
std::vector<Vector3> CollisionWireEdges(MeshComponent &mesh, bool convex)
{
    auto triangles = CollisionTriangles(mesh);
    std::vector<Vector3> edges;
    if (!convex)
        for (size_t i = 0; i < triangles.size(); i += 3)
            for (int j = 0; j < 3; ++j)
            {
                edges.push_back(triangles[i + j]);
                edges.push_back(triangles[i + (j + 1) % 3]);
            }
    else
    {
        JPH::ConvexHullBuilder::Positions points;
        for (auto v : triangles)
            points.push_back(V(v));
        JPH::ConvexHullBuilder hull(points);
        const char *error = nullptr;
        auto result = hull.Initialize(256, .001f, error);
        if (result != JPH::ConvexHullBuilder::EResult::Success &&
            result != JPH::ConvexHullBuilder::EResult::MaxVerticesReached)
            return {};
        for (auto face : hull.GetFaces())
        {
            auto edge = face->mFirstEdge;
            do
            {
                edges.push_back(triangles.at(edge->mStartIdx));
                edges.push_back(triangles.at(edge->mNextEdge->mStartIdx));
                edge = edge->mNextEdge;
            } while (edge != face->mFirstEdge);
        }
    }
    return edges;
}
struct PhysicsWorld::Impl : public JPH::ContactListener
{
    Layers layers;
    PairFilter pair;
    BroadFilter broad;
    JPH::TempAllocatorImpl allocator{16 * 1024 * 1024};
    JPH::JobSystemSingleThreaded jobs{JPH::cMaxPhysicsJobs};
    JPH::PhysicsSystem system;
    struct Body
    {
        JPH::BodyID id;
        std::string signature;
        Vector3 position, velocity;
        Quaternion rotation;
    };
    std::map<Entity, Body> bodies;
    struct Joint
    {
        std::string signature;
        JPH::Ref<JPH::TwoBodyConstraint> constraint;
    };
    std::map<Entity, Joint> joints;
    void ClearJoints()
    {
        for (auto &[e, joint] : joints)
            system.RemoveConstraint(joint.constraint);
        joints.clear();
    }
    std::weak_ptr<void> lifetime;
    std::vector<PhysicsContact> contacts;
    float accumulator = 0;
    Impl()
    {
        system.Init(8192, 0, 8192, 8192, layers, broad, pair);
        system.SetGravity(JPH::Vec3(0, -9.81f, 0));
        system.SetContactListener(this);
    }
    void OnContactAdded(const JPH::Body &a, const JPH::Body &b, const JPH::ContactManifold &,
                        JPH::ContactSettings &) override
    {
        contacts.push_back(
            {Entity(uint32_t(a.GetUserData())), Entity(uint32_t(b.GetUserData())), a.IsSensor() || b.IsSensor()});
    }
    void Clear()
    {
        ClearJoints();
        auto &bi = system.GetBodyInterface();
        for (auto &[e, b] : bodies)
        {
            bi.RemoveBody(b.id);
            bi.DestroyBody(b.id);
        }
        bodies.clear();
        contacts.clear();
        accumulator = 0;
        lifetime.reset();
    }
    ~Impl()
    {
        Clear();
    }
};
PhysicsWorld::PhysicsWorld()
{
    if (users++ == 0)
    {
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory;
        JPH::RegisterTypes();
    }
    mImpl = std::make_unique<Impl>();
}
PhysicsWorld::~PhysicsWorld()
{
    mImpl.reset();
    if (--users == 0)
    {
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
}
void PhysicsWorld::Reset()
{
    mImpl->Clear();
}
const std::vector<PhysicsContact> &PhysicsWorld::Contacts() const
{
    return mImpl->contacts;
}
void PhysicsWorld::Update(World &w, float dt)
{
    auto &i = *mImpl;
    auto current = w.GetLifetime().lock();
    if (i.lifetime.lock() != current)
    {
        i.Clear();
        i.lifetime = current;
    }
    i.contacts.clear();
    auto &bi = i.system.GetBodyInterface();
    std::set<Entity> alive;
    for (auto e : w.View<TransformComponent, ColliderComponent>())
    {
        auto &t = w.GetComponent<TransformComponent>(e);
        auto &c = w.GetComponent<ColliderComponent>(e);
        RigidBodyComponent defaultBody;
        defaultBody.motion = BodyMotion::Static;
        auto *component = w.TryGet<RigidBodyComponent>(e);
        auto &r = component ? *component : defaultBody;
        auto matrix = t.GetMatrix();
        Vector3 scale = matrix.GetScale(), pos = matrix.GetTrans();
        Quaternion rotation = matrix.GetRotation();
        auto signature = Signature(c, r, scale);
        auto *mesh = w.TryGet<MeshComponent>(e);
        if (c.shape == ColliderShape::Mesh || c.shape == ColliderShape::ConvexHull)
        {
            if (!mesh)
                throw std::runtime_error("Mesh collider requires a Mesh component");
            if (c.shape == ColliderShape::Mesh && r.motion != BodyMotion::Static)
                throw std::runtime_error("Triangle mesh collider must be static; use Convex Hull for moving bodies");
            signature += std::to_string(reinterpret_cast<uintptr_t>(mesh->GetModel().get()));
        }
        alive.insert(e);
        auto it = i.bodies.find(e);
        if (it != i.bodies.end() && it->second.signature != signature)
        {
            i.ClearJoints();
            bi.RemoveBody(it->second.id);
            bi.DestroyBody(it->second.id);
            i.bodies.erase(it);
            it = i.bodies.end();
        }
        if (it == i.bodies.end())
        {
            JPH::RefConst<JPH::Shape> shape;
            float maxScale = std::max({scale.x, scale.y, scale.z});
            if (c.shape == ColliderShape::Box)
                shape = new JPH::BoxShape(
                    V(Vector3(std::max(.001f, c.halfExtents.x * scale.x), std::max(.001f, c.halfExtents.y * scale.y),
                              std::max(.001f, c.halfExtents.z * scale.z))),
                    0.001f);
            else if (c.shape == ColliderShape::Sphere)
                shape = new JPH::SphereShape(std::max(.001f, c.radius * maxScale));
            else if (c.shape == ColliderShape::Capsule)
                shape = new JPH::CapsuleShape(std::max(.001f, c.halfHeight * scale.y),
                                              std::max(.001f, c.radius * std::max(scale.x, scale.z)));
            else
            {
                auto triangles = CollisionTriangles(*mesh);
                JPH::ShapeSettings::ShapeResult result;
                if (c.shape == ColliderShape::Mesh)
                {
                    JPH::TriangleList list;
                    for (size_t j = 0; j < triangles.size(); j += 3)
                        list.emplace_back(V(triangles[j] * scale), V(triangles[j + 1] * scale),
                                          V(triangles[j + 2] * scale));
                    result = JPH::MeshShapeSettings(list).Create();
                }
                else
                {
                    JPH::Array<JPH::Vec3> points;
                    for (auto v : triangles)
                        points.push_back(V(v * scale));
                    result = JPH::ConvexHullShapeSettings(points).Create();
                }
                if (result.HasError())
                    throw std::runtime_error(result.GetError().c_str());
                shape = result.Get();
            }
            shape = new JPH::RotatedTranslatedShape(V(c.center * scale), Q(c.rotation), shape);
            auto motion = r.motion == BodyMotion::Static      ? JPH::EMotionType::Static
                          : r.motion == BodyMotion::Kinematic ? JPH::EMotionType::Kinematic
                                                              : JPH::EMotionType::Dynamic;
            JPH::BodyCreationSettings settings(shape, V(pos), Q(rotation), motion,
                                               r.motion == BodyMotion::Static ? 0 : 1);
            settings.mIsSensor = c.trigger;
            settings.mFriction = r.friction;
            settings.mRestitution = r.restitution;
            settings.mGravityFactor = r.gravity ? 1.f : 0.f;
            settings.mLinearVelocity = V(r.linearVelocity);
            settings.mUserData = uint32_t(e);
            settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
            settings.mMassPropertiesOverride.mMass = std::max(.001f, r.mass);
            auto id = bi.CreateAndAddBody(settings, JPH::EActivation::Activate);
            if (id.IsInvalid())
                throw std::runtime_error("Physics body capacity exhausted");
            it = i.bodies.emplace(e, Impl::Body{id, signature, pos, r.linearVelocity, rotation}).first;
        }
        auto &body = it->second;
        auto rotationDelta = rotation - body.rotation, rotationSum = rotation + body.rotation;
        if (r.motion == BodyMotion::Kinematic)
            bi.MoveKinematic(body.id, V(pos), Q(rotation), std::max(dt, 1.f / 60));
        else if ((pos - body.position).LengthSqr() > 1e-8f ||
                 std::min(Quaternion::Dot(rotationDelta, rotationDelta), Quaternion::Dot(rotationSum, rotationSum)) >
                     1e-12f)
            bi.SetPositionAndRotation(body.id, V(pos), Q(rotation), JPH::EActivation::Activate);
        if ((r.linearVelocity - body.velocity).LengthSqr() > 1e-8f && r.motion != BodyMotion::Static)
            bi.SetLinearVelocity(body.id, V(r.linearVelocity));
    }
    for (auto it = i.bodies.begin(); it != i.bodies.end();)
        if (!alive.contains(it->first))
        {
            i.ClearJoints();
            bi.RemoveBody(it->second.id);
            bi.DestroyBody(it->second.id);
            it = i.bodies.erase(it);
        }
        else
            ++it;
    std::map<std::string, Entity> ids;
    for (auto e : w.View<PersistentIdComponent>())
        ids[w.GetComponent<PersistentIdComponent>(e).value] = e;
    std::set<Entity> activeJoints;
    for (auto e : w.View<JointComponent>())
    {
        const auto &j = w.GetComponent<JointComponent>(e);
        if (!j.enabled || !i.bodies.contains(e) || !ids.contains(j.connectedBody))
            continue;
        auto other = ids.at(j.connectedBody);
        if (other == e || !i.bodies.contains(other))
            continue;
        if (bi.GetMotionType(i.bodies.at(e).id) == JPH::EMotionType::Static &&
            bi.GetMotionType(i.bodies.at(other).id) == JPH::EMotionType::Static)
            continue;
        std::ostringstream sig;
        sig << int(j.type) << ',' << j.connectedBody << ',';
        for (auto v : {j.anchor, j.connectedAnchor, j.axis})
            sig << v.x << ',' << v.y << ',' << v.z << ',';
        sig << j.minDistance << ',' << j.maxDistance << ',' << j.minAngle << ',' << j.maxAngle;
        auto signature = sig.str();
        activeJoints.insert(e);
        if (auto old = i.joints.find(e); old != i.joints.end() && old->second.signature != signature)
        {
            i.system.RemoveConstraint(old->second.constraint);
            i.joints.erase(old);
        }
        if (i.joints.contains(e))
            continue;
        auto a = Math::TransformCoord(j.anchor, w.GetComponent<TransformComponent>(e).GetMatrix());
        auto b = Math::TransformCoord(j.connectedAnchor, w.GetComponent<TransformComponent>(other).GetMatrix());
        JPH::Ref<JPH::TwoBodyConstraintSettings> settings;
        if (j.type == JointType::Distance)
        {
            auto s = new JPH::DistanceConstraintSettings;
            s->mPoint1 = V(a);
            s->mPoint2 = V(b);
            s->mMinDistance = j.minDistance;
            s->mMaxDistance = j.maxDistance;
            settings = s;
        }
        else if (j.type == JointType::Hinge)
        {
            auto s = new JPH::HingeConstraintSettings;
            auto axis = V(w.GetComponent<TransformComponent>(e).GetMatrix().GetRotation() * j.axis).Normalized();
            auto normal = axis.GetNormalizedPerpendicular();
            s->mPoint1 = V(a);
            s->mPoint2 = V(b);
            s->mHingeAxis1 = s->mHingeAxis2 = axis;
            s->mNormalAxis1 = s->mNormalAxis2 = normal;
            s->mLimitsMin = j.minAngle * JPH::JPH_PI / 180;
            s->mLimitsMax = j.maxAngle * JPH::JPH_PI / 180;
            settings = s;
        }
        else
        {
            auto s = new JPH::FixedConstraintSettings;
            s->mAutoDetectPoint = true;
            settings = s;
        }
        auto constraint = bi.CreateConstraint(settings, i.bodies.at(e).id, i.bodies.at(other).id);
        i.system.AddConstraint(constraint);
        i.joints.emplace(e, Impl::Joint{signature, constraint});
    }
    for (auto it = i.joints.begin(); it != i.joints.end();)
        if (!activeJoints.contains(it->first))
        {
            i.system.RemoveConstraint(it->second.constraint);
            it = i.joints.erase(it);
        }
        else
            ++it;
    i.accumulator += std::clamp(dt, 0.f, .25f);
    constexpr float step = 1.f / 60;
    while (i.accumulator >= step)
    {
        i.system.Update(step, 1, &i.allocator, &i.jobs);
        i.accumulator -= step;
    }
    for (auto &[e, b] : i.bodies)
    {
        auto &t = w.GetComponent<TransformComponent>(e);
        RigidBodyComponent defaultBody;
        defaultBody.motion = BodyMotion::Static;
        auto *component = w.TryGet<RigidBodyComponent>(e);
        auto &r = component ? *component : defaultBody;
        auto p = bi.GetPosition(b.id);
        auto q = bi.GetRotation(b.id);
        auto v = bi.GetLinearVelocity(b.id);
        b.position = Vector3(p.GetX(), p.GetY(), p.GetZ());
        b.rotation = Quaternion(q.GetX(), q.GetY(), q.GetZ(), q.GetW());
        b.velocity = Vector3(v.GetX(), v.GetY(), v.GetZ());
        r.linearVelocity = b.velocity;
        if (r.motion == BodyMotion::Dynamic)
        {
            Transform world(b.position, b.rotation, t.GetMatrix().GetScale());
            auto local = t.parent ? world.GetMatrix() * t.parent->GetMatrix().Inverse() : world.GetMatrix();
            t.transition = local.GetTrans();
            t.rotation = local.GetRotation();
        }
    }
}
} // namespace AtomEngine
