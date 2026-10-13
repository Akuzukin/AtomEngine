#include "ShadowCamera.h"

namespace AtomEngine
{
void ShadowCamera::UpdateMatrix(Vector3 lightDir, Vector3 ShadowCenter, Vector3 ShadowBounds, uint32_t BufferWidth,
                                uint32_t BufferHeight)
{
    lightDir.Normalize();
    ShadowBounds = Math::Max(ShadowBounds, Vector3(1, 1, 1));
    SetLookDirection(lightDir, std::abs(lightDir.y) > .95f ? Vector3::FORWARD : Vector3::UP);

    // Converts world units to texel units so we can quantize the camera position to whole texel units
    Vector3 RcpDimensions = 1.0f / (ShadowBounds);
    Vector3 QuantizeScale = Vector3((float)BufferWidth, (float)BufferHeight, (float)((1 << 16) - 1)) * RcpDimensions;

    //
    // Recenter the camera at the quantized position
    //

    // Transform to view space
    ShadowCenter = GetRotation().Conjugate() * ShadowCenter;
    // Scale to texel units, truncate fractional part, and scale back to world units
    ShadowCenter = Math::Floor(ShadowCenter * QuantizeScale) / QuantizeScale;
    // Transform back into world space
    ShadowCenter = GetRotation() * ShadowCenter;

    SetPosition(ShadowCenter - lightDir * ShadowBounds.z * .5f);

    auto projection = Matrix4x4::MakeScale(Vector3(2.0f, 2.0f, -1.0f) * RcpDimensions);
    projection[3][2] = 1.f;
    SetProjMatrix(projection);

    Update();

    // Transform from clip space to texture space
    mShadowMatrix = mViewProjMatrix * Matrix4x4(Matrix3x3::MakeScale({0.5f, -0.5f, 1.0f}), Vector3(0.5f, 0.5f, 0.0f));
}
} // namespace AtomEngine
