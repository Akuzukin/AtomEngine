#include "Primitive.h"
#include "Runtime/Function/Render/ModelGpuBuilder.h"
#include <array>
#include <cmath>
#include <algorithm>
namespace AtomEngine
{
namespace
{
static void FinishGeometry(ModelData &data)
{
    for (const auto &vertex : data.vertices)
        data.boundingBox.AddPoint(vertex.position);
    data.boundingSphere = BoundingSphere(data.boundingBox);
    Material material;
    material.emissiveFactor = Vector3(0, 0, 0);
    material.metallicFactor = 0;
    material.roughnessFactor = 0.5f;
    data.materials.push_back(material);
    Mesh mesh;
    mesh.vertexCount = static_cast<uint32_t>(data.vertices.size());
    mesh.indexCount = static_cast<uint32_t>(data.indices.size());
    mesh.boundingBox = data.boundingBox;
    SubMesh sub;
    sub.vertexCount = mesh.vertexCount;
    sub.indexCount = mesh.indexCount;
    sub.bounds = data.boundingBox;
    mesh.subMeshes.push_back(sub);
    data.meshes.push_back(mesh);
    GraphNode node{};
    node.xform = Matrix4x4::IDENTITY;
    node.rotation = Quaternion::IDENTITY;
    node.scale = Vector3::UNIT_SCALE;
    data.graphNodes.push_back(node);
}

static ModelData MakeSphere(uint32_t slices, uint32_t stacks)
{
    ModelData data;

    constexpr float pi = 3.14159265358979323846f;
    for (uint32_t y = 0; y <= stacks; ++y)
        for (uint32_t x = 0; x <= slices; ++x)
        {
            const float theta = pi * y / stacks, phi = 2 * pi * x / slices;
            Vertex v{};
            v.position = Vector3(std::sin(theta) * std::cos(phi), std::cos(theta), std::sin(theta) * std::sin(phi));
            v.normal = v.position;
            v.tangent = Vector3(-std::sin(phi), 0, std::cos(phi));
            v.bitangent = Vector3(std::cos(theta) * std::cos(phi), -std::sin(theta), std::cos(theta) * std::sin(phi));
            v.texcoord = Vector2(float(x) / slices, float(y) / stacks);
            data.vertices.push_back(v);
        }
    for (uint32_t y = 0; y < stacks; ++y)
        for (uint32_t x = 0; x < slices; ++x)
        {
            const uint32_t a = y * (slices + 1) + x, b = a + slices + 1;
            data.indices.insert(data.indices.end(), {a, a + 1, b, a + 1, b + 1, b});
        }
    FinishGeometry(data);
    return data;
}

static ModelData MakePlane()
{
    ModelData data;
    for (const auto &p : {Vector3(-12, 0, -12), Vector3(-12, 0, 12), Vector3(12, 0, 12), Vector3(12, 0, -12)})
    {
        Vertex v{};
        v.position = p;
        v.normal = Vector3(0, 1, 0);
        v.tangent = Vector3(1, 0, 0);
        v.bitangent = Vector3(0, 0, 1);
        v.texcoord = Vector2((p.x + 12) / 24, (p.z + 12) / 24);
        data.vertices.push_back(v);
    }
    data.indices = {0, 1, 2, 0, 2, 3};
    FinishGeometry(data);
    return data;
}

std::array<std::weak_ptr<Model>, 5> cache;
ModelData MakeCube()
{
    ModelData d;
    const Vector3 normals[] = {Vector3(1, 0, 0),  Vector3(-1, 0, 0), Vector3(0, 1, 0),
                               Vector3(0, -1, 0), Vector3(0, 0, 1),  Vector3(0, 0, -1)};
    for (auto n : normals)
    {
        Vector3 t = std::abs(n.y) > 0.9f ? Vector3(1, 0, 0) : Vector3(0, 1, 0);
        Vector3 b = n.Cross(t);
        uint32_t start = uint32_t(d.vertices.size());
        for (auto uv : {Vector2(0, 0), Vector2(0, 1), Vector2(1, 1), Vector2(1, 0)})
        {
            Vertex v{};
            v.position = (n + t * (uv.x * 2 - 1) + b * (uv.y * 2 - 1)) * 0.5f;
            v.normal = n;
            v.tangent = t;
            v.bitangent = b;
            v.texcoord = uv;
            d.vertices.push_back(v);
        }
        // b = n x t; the outward-facing triangle therefore runs t before b.
        d.indices.insert(d.indices.end(), {start, start + 2, start + 1, start, start + 3, start + 2});
    }
    FinishGeometry(d);
    return d;
}
} // namespace
ModelData Primitive::Generate(BuiltinMesh t, uint32_t slices, uint32_t stacks)
{
    if (t == BuiltinMesh::Sphere)
        return MakeSphere(std::clamp(slices, 3u, 256u), std::clamp(stacks, 2u, 128u));
    if (t == BuiltinMesh::Cube)
        return MakeCube();
    auto d = MakePlane();
    for (auto &v : d.vertices)
    {
        v.position = v.position / 24.f;
    }
    if (t == BuiltinMesh::Quad)
        for (auto &v : d.vertices)
        {
            v.position = Vector3(v.position.x, v.position.z, 0);
            v.normal = Vector3(0, 0, -1);
            v.bitangent = Vector3(0, 1, 0);
        }
    d.meshes.clear();
    d.materials.clear();
    d.graphNodes.clear();
    d.boundingBox = AxisAlignedBox();
    FinishGeometry(d);
    return d;
}
std::shared_ptr<Model> Primitive::GetModel(BuiltinMesh t)
{
    if (t == BuiltinMesh::None)
        return nullptr;
    auto &entry = cache.at(size_t(t));
    if (auto m = entry.lock())
        return m;
    auto d = Generate(t);
    auto m = BuildModelGpuResources(d, L"Builtin/Primitive");
    entry = m;
    return m;
}
void Primitive::ClearCache()
{
    for (auto &m : cache)
        m.reset();
}
} // namespace AtomEngine
