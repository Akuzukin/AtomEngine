#pragma once
#include "Runtime/Function/Framework/Component/MeshComponent.h"
#include <stdexcept>
namespace AtomEngine
{
// A collision mesh is the model's bind pose. Animated deformation is not a collider update.
inline std::vector<Vector3> CollisionTriangles(MeshComponent &mesh)
{
    const auto &model = *mesh.GetModel();
    if (!model.mGeometry)
        throw std::runtime_error("Collider mesh has no CPU geometry");
    std::vector<Matrix4x4> nodes(model.mSceneGraph.size());
    std::vector<Matrix4x4> stack;
    Matrix4x4 parent = mesh.GetTransform().GetMatrix();
    for (const auto &node : model.mSceneGraph)
    {
        auto matrix = node.xform * parent;
        nodes.at(node.matrixIdx) = matrix;
        if (node.hasChildren)
        {
            if (node.hasSibling)
                stack.push_back(parent);
            parent = matrix;
        }
        else if (!node.hasSibling && !stack.empty())
        {
            parent = stack.back();
            stack.pop_back();
        }
    }
    std::vector<Vector3> result;
    for (const auto &part : model.mMeshData)
        for (const auto &sub : part.subMeshes)
            for (size_t i = sub.indexOffset; i < size_t(sub.indexOffset) + sub.indexCount; ++i)
                result.push_back(Math::TransformCoord(
                    model.mGeometry->vertices.at(model.mGeometry->indices.at(i) + sub.vertexOffset).position,
                    nodes.at(sub.meshCbvIndex)));
    if (result.empty() || result.size() % 3)
        throw std::runtime_error("Collider mesh has no triangles");
    return result;
}
std::vector<Vector3> CollisionWireEdges(MeshComponent &mesh, bool convex);
} // namespace AtomEngine
