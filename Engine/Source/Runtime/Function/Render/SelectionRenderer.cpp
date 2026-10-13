#include "SelectionRenderer.h"
#include "RenderSystem.h"
#include "DebugDraw.h"
#include "Runtime/Function/Framework/Component/MeshComponent.h"
#include "Runtime/Function/Framework/Component/SceneComponents.h"
#include "Runtime/Platform/DirectX12/Shader/ShaderCompiler.h"
#include "Runtime/Platform/DirectX12/Shader/ConstantBufferStructures.h"
#include <cmath>
#include <limits>
namespace AtomEngine
{
namespace
{
ColorBuffer mask;
RootSignature outlineRoot;
GraphicsPSO maskPso, skinPso, outlinePso;
std::weak_ptr<void> selectedWorld;
Entity selected = entt::null;
uint64_t maskRevision = 0, outlineRevision = 0;
void Pipelines()
{
    D3D12_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"BITTANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"WEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 56, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"INDEX", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, 72, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
    maskPso.SetRootSignature(Renderer::GetRootSignature());
    maskPso.SetRasterizerState(RasterizerTwoSided);
    maskPso.SetBlendState(BlendDisable);
    maskPso.SetDepthStencilState(DepthStateReadOnly);
    maskPso.SetInputLayout(5, layout);
    maskPso.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    maskPso.SetRenderTargetFormat(DXGI_FORMAT_R8_UNORM, gSceneDepthBuffer.GetFormat());
    maskPso.SetVertexShader(ShaderCompiler::CompileBlob(L"DepthOnlyVS.hlsl", L"vs_6_2").Get());
    maskPso.SetPixelShader(ShaderCompiler::CompileBlob(L"SelectionMaskPS.hlsl", L"ps_6_2").Get());
    maskPso.Finalize();
    skinPso = maskPso;
    skinPso.SetInputLayout(7, layout);
    skinPso.SetVertexShader(ShaderCompiler::CompileBlob(L"DepthOnlySkinVS.hlsl", L"vs_6_2").Get());
    skinPso.Finalize();
    outlinePso.SetRootSignature(outlineRoot);
    outlinePso.SetRasterizerState(RasterizerTwoSided);
    outlinePso.SetBlendState(BlendTraditional);
    outlinePso.SetDepthStencilState(DepthStateDisabled);
    outlinePso.SetInputLayout(0, nullptr);
    outlinePso.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    outlinePso.SetRenderTargetFormat(gSceneColorBuffer.GetFormat(), DXGI_FORMAT_UNKNOWN);
    outlinePso.SetVertexShader(ShaderCompiler::CompileBlob(L"SelectionOutline.hlsl", L"vs_6_2", L"VS").Get());
    outlinePso.SetPixelShader(ShaderCompiler::CompileBlob(L"SelectionOutline.hlsl", L"ps_6_2", L"PS").Get());
    outlinePso.Finalize();
}
bool Triangle(const Vector3 &o, const Vector3 &d, const Vector3 &a, const Vector3 &b, const Vector3 &c, float &hit)
{
    auto e1 = b - a, e2 = c - a, p = d.Cross(e2);
    float det = e1.Dot(p);
    if (std::abs(det) < 1e-7f)
        return false;
    auto t = o - a;
    float u = t.Dot(p) / det;
    if (u < 0 || u > 1)
        return false;
    auto q = t.Cross(e1);
    float v = d.Dot(q) / det;
    if (v < 0 || u + v > 1)
        return false;
    hit = e2.Dot(q) / det;
    return hit > 0;
}
} // namespace
void SelectionRenderer::Initialize()
{
    outlineRoot.Reset(1, 0);
    outlineRoot[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);
    outlineRoot.Finalize(L"Selection Outline");
    Pipelines();
}
void SelectionRenderer::Shutdown()
{
    mask.Destroy();
    selectedWorld.reset();
    selected = entt::null;
}
void SelectionRenderer::Select(World *w, Entity e)
{
    selectedWorld = w ? w->GetLifetime() : std::weak_ptr<void>();
    selected = e;
}
void SelectionRenderer::Render(GraphicsContext &ctx, World &w, const Camera &camera)
{
    if (selectedWorld.lock() != w.GetLifetime().lock() || !w.IsValid(selected))
        return;
    auto mesh = w.TryGet<MeshComponent>(selected);
    if (!mesh || !mesh->IsRender())
        return;
    if (mask.GetWidth() != gSceneColorBuffer.GetWidth() || mask.GetHeight() != gSceneColorBuffer.GetHeight())
        mask.Create(L"Selection Mask", gSceneColorBuffer.GetWidth(), gSceneColorBuffer.GetHeight(), 1,
                    DXGI_FORMAT_R8_UNORM);
    ctx.TransitionResource(mask, D3D12_RESOURCE_STATE_RENDER_TARGET);
    ctx.TransitionResource(gSceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_READ, true);
    ctx.ClearColor(mask);
    ctx.SetRenderTarget(mask.GetRTV(), gSceneDepthBuffer.GetDSV_DepthReadOnly());
    ctx.SetViewportAndScissor(0, 0, mask.GetWidth(), mask.GetHeight());
    ctx.SetRootSignature(Renderer::GetRootSignature());
    ctx.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx.SetPipelineState(mesh->GetModel()->mNumJoints ? skinPso : maskPso);
    alignas(256) GlobalConstants constants{};
    constants.ViewProjMatrix = camera.GetViewProjMatrix();
    ctx.SetDynamicConstantBufferView(kCommonCBV, sizeof(constants), &constants);
    mesh->DrawSelection(ctx);
    ctx.TransitionResource(mask, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    ctx.TransitionResource(gSceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
    ctx.SetRenderTarget(gSceneColorBuffer.GetRTV());
    ctx.SetRootSignature(outlineRoot);
    ctx.SetPipelineState(outlinePso);
    ctx.SetDynamicDescriptor(0, 0, mask.GetSRV());
    ctx.Draw(3);
}
void SelectionRenderer::DrawColliders(World &w, const Camera &camera)
{
    for (auto e : w.View<ColliderComponent, TransformComponent>())
    {
        auto &c = w.GetComponent<ColliderComponent>(e);
        if (!c.visible)
            continue;
        auto &t = w.GetComponent<TransformComponent>(e);
        Color color = c.trigger ? Color(1, .7f, .1f) : Color(.1f, 1, .3f);
        auto world = t.GetMatrix();
        auto scale = world.GetScale();
        auto matrix = Transform(c.center * scale, c.rotation, Vector3::UNIT_SCALE).GetMatrix() *
                      Transform(world.GetTrans(), world.GetRotation(), Vector3::UNIT_SCALE).GetMatrix();
        auto extents = c.halfExtents * scale;
        float radius = c.radius * (c.shape == ColliderShape::Sphere ? std::max({scale.x, scale.y, scale.z})
                                                                    : std::max(scale.x, scale.z));
        auto line = [&](Vector3 a, Vector3 b) {
            DebugDraw::DrawLine(Math::TransformCoord(a, matrix), Math::TransformCoord(b, matrix), color,
                                camera.GetViewProjMatrix());
        };
        if (c.shape == ColliderShape::Box)
        {
            Vector3 corners[8];
            for (int i = 0; i < 8; ++i)
                corners[i] = Vector3((i & 1 ? 1.f : -1.f) * extents.x, (i & 2 ? 1.f : -1.f) * extents.y,
                                     (i & 4 ? 1.f : -1.f) * extents.z);
            for (int i = 0; i < 8; ++i)
                for (int bit : {1, 2, 4})
                    if (!(i & bit))
                        line(corners[i], corners[i | bit]);
        }
        else
        {
            constexpr int segments = 32;
            float h = c.shape == ColliderShape::Capsule ? c.halfHeight * scale.y : 0;
            for (int axis = 0; axis < 3; ++axis)
                for (int j = 0; j < segments; ++j)
                {
                    float a = j * 6.2831853f / segments, b = (j + 1) * 6.2831853f / segments;
                    auto p = [&](float angle) {
                        Vector3 v;
                        if (axis == 0)
                            v = Vector3(0, std::cos(angle), std::sin(angle));
                        else if (axis == 1)
                            v = Vector3(std::cos(angle), 0, std::sin(angle));
                        else
                            v = Vector3(std::cos(angle), std::sin(angle), 0);
                        v *= radius;
                        if (axis != 1)
                            v.y += v.y >= 0 ? h : -h;
                        return v;
                    };
                    if (axis == 1 && h > 0)
                    {
                        line(p(a) + Vector3(0, h, 0), p(b) + Vector3(0, h, 0));
                        line(p(a) - Vector3(0, h, 0), p(b) - Vector3(0, h, 0));
                    }
                    else
                        line(p(a), p(b));
                }
        }
    }
}
Entity SelectionRenderer::Pick(World &w, const Camera &camera, float x, float y, float width, float height)
{
    if (width <= 0 || height <= 0)
        return entt::null;
    auto inv = camera.GetViewProjMatrix().Inverse();
    auto point = Vector4(2 * x / width - 1, 1 - 2 * y / height, .5f, 1) * inv;
    if (std::abs(point.w) < 1e-8f)
        return entt::null;
    Vector3 origin = camera.GetPosition(), end(point.x / point.w, point.y / point.w, point.z / point.w);
    auto direction = (end - origin).NormalizedCopy();
    origin += direction * camera.GetNearClip();
    float best = std::numeric_limits<float>::max();
    Entity result = entt::null;
    for (auto e : w.View<MeshComponent, TransformComponent>())
    {
        auto &mesh = w.GetComponent<MeshComponent>(e);
        if (!mesh.IsRender() || !mesh.GetModel()->mGeometry)
            continue;
        auto &g = *mesh.GetModel()->mGeometry;
        for (auto &m : mesh.GetModel()->mMeshData)
            for (auto &s : m.subMeshes)
            {
                if (s.meshCbvIndex >= mesh.NodeTransforms().size())
                    continue;
                auto matrix = mesh.NodeTransforms()[s.meshCbvIndex];
                for (size_t i = s.indexOffset; i + 2 < size_t(s.indexOffset) + s.indexCount && i + 2 < g.indices.size();
                     i += 3)
                {
                    auto v = [&](size_t j) {
                        return Math::TransformCoord(mesh.PickVertex(g.indices[j] + s.vertexOffset, m.startJoint),
                                                    matrix);
                    };
                    float hit;
                    if (Triangle(origin, direction, v(i), v(i + 1), v(i + 2), hit) && hit < best)
                    {
                        best = hit;
                        result = e;
                    }
                }
            }
    }
    return result;
}
} // namespace AtomEngine
