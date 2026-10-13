#include "Frustum.h"

namespace AtomEngine
{

	void Frustum::ConstructPerspectiveFrustum(float HTan, float VTan, float NearClip, float FarClip)
	{
		const float NearX = HTan * NearClip;
		const float NearY = VTan * NearClip;
		const float FarX = HTan * FarClip;
		const float FarY = VTan * FarClip;

		m_FrustumCorners[kNearLowerLeft] = Vector3(-NearX, -NearY, NearClip);
		m_FrustumCorners[kNearUpperLeft] = Vector3(-NearX, NearY, NearClip);
		m_FrustumCorners[kNearLowerRight] = Vector3(NearX, -NearY, NearClip);
		m_FrustumCorners[kNearUpperRight] = Vector3(NearX, NearY, NearClip);
		m_FrustumCorners[kFarLowerLeft] = Vector3(-FarX, -FarY, FarClip);
		m_FrustumCorners[kFarUpperLeft] = Vector3(-FarX, FarY, FarClip);
		m_FrustumCorners[kFarLowerRight] = Vector3(FarX, -FarY, FarClip);
		m_FrustumCorners[kFarUpperRight] = Vector3(FarX, FarY, FarClip);

		const float NHx = 1.0f / std::sqrt(1.0f + HTan * HTan);
		const float NHz = NHx * HTan;
		const float NVy = 1.0f / std::sqrt(1.0f + VTan * VTan);
		const float NVz = NVy * VTan;

		m_FrustumPlanes[kNearPlane] = BoundingPlane(0.0f, 0.0f, -1.0f, NearClip);
		m_FrustumPlanes[kFarPlane] = BoundingPlane(0.0f, 0.0f, 1.0f, -FarClip);
		m_FrustumPlanes[kLeftPlane] = BoundingPlane(NHx, 0.0f, NHz, 0.0f);
		m_FrustumPlanes[kRightPlane] = BoundingPlane(-NHx, 0.0f, NHz, 0.0f);
		m_FrustumPlanes[kTopPlane] = BoundingPlane(0.0f, -NVy, NVz, 0.0f);
		m_FrustumPlanes[kBottomPlane] = BoundingPlane(0.0f, NVy, NVz, 0.0f);
	}

	void Frustum::ConstructOrthographicFrustum(float Left, float Right, float Top, float Bottom, float Front, float Back)
	{
		m_FrustumCorners[kNearLowerLeft] = Vector3(Left, Bottom, Front);
		m_FrustumCorners[kNearUpperLeft] = Vector3(Left, Top, Front);
		m_FrustumCorners[kNearLowerRight] = Vector3(Right, Bottom, Front);
		m_FrustumCorners[kNearUpperRight] = Vector3(Right, Top, Front);
		m_FrustumCorners[kFarLowerLeft] = Vector3(Left, Bottom, Back);
		m_FrustumCorners[kFarUpperLeft] = Vector3(Left, Top, Back);
		m_FrustumCorners[kFarLowerRight] = Vector3(Right, Bottom, Back);
		m_FrustumCorners[kFarUpperRight] = Vector3(Right, Top, Back);

		m_FrustumPlanes[kNearPlane] = BoundingPlane(0.0f, 0.0f, 1.0f, -Front);
		m_FrustumPlanes[kFarPlane] = BoundingPlane(0.0f, 0.0f, -1.0f, Back);
		m_FrustumPlanes[kLeftPlane] = BoundingPlane(1.0f, 0.0f, 0.0f, -Left);
		m_FrustumPlanes[kRightPlane] = BoundingPlane(-1.0f, 0.0f, 0.0f, Right);
		m_FrustumPlanes[kBottomPlane] = BoundingPlane(0.0f, 1.0f, 0.0f, -Bottom);
		m_FrustumPlanes[kTopPlane] = BoundingPlane(0.0f, -1.0f, 0.0f, Top);
	}

	Frustum::Frustum(const Matrix4x4& ProjMat)
	{
		const float* M = (const float*)&ProjMat;

		const float RcpXX = 1.0f / M[0];
		const float RcpYY = 1.0f / M[5];
		const float RcpZZ = 1.0f / M[10];

		if (M[3] == 0.0f && M[7] == 0.0f && M[11] == 0.0f && M[15] == 1.0f)
		{
			// Orthographic
			float Left = (-1.0f - M[12]) * RcpXX;
			float Right = (1.0f - M[12]) * RcpXX;
			float Top = (1.0f - M[13]) * RcpYY;
			float Bottom = (-1.0f - M[13]) * RcpYY;
			float Front = (0.0f - M[14]) * RcpZZ;
			float Back = (1.0f - M[14]) * RcpZZ;

			if (Front < Back)
				ConstructOrthographicFrustum(Left, Right, Top, Bottom, Front, Back);
			else
				ConstructOrthographicFrustum(Left, Right, Top, Bottom, Back, Front);
		}
		else
		{
			float NearClip, FarClip;

			float Z_far_val = std::abs(M[14] * RcpZZ);

			float Z_near_val = std::abs(Z_far_val / (1.0f - RcpZZ));

			if (RcpZZ > 0.0f)
			{
				NearClip = std::min(Z_near_val, Z_far_val);
				FarClip = std::max(Z_near_val, Z_far_val);
			}
			else
			{
				NearClip = std::max(Z_near_val, Z_far_val);
				FarClip = std::min(Z_near_val, Z_far_val);
			}

			ConstructPerspectiveFrustum(RcpXX, RcpYY, NearClip, FarClip);
		}
	}
	Frustum::Frustum(float fovY, float aspect, float nearZ, float farZ)
	{
		float vTan = tanf(fovY * 0.5f);
		float hTan = vTan * aspect;
		ConstructPerspectiveFrustum(hTan, vTan, nearZ, farZ);
	}
	void Frustum::DebugDraw(const Matrix4x4& viewProj, const Color& color) const
	{
		// Near rect
		DebugDraw::DrawLine(m_FrustumCorners[kNearLowerLeft], m_FrustumCorners[kNearUpperLeft], color, viewProj);
		DebugDraw::DrawLine(m_FrustumCorners[kNearUpperLeft], m_FrustumCorners[kNearUpperRight], color, viewProj);
		DebugDraw::DrawLine(m_FrustumCorners[kNearUpperRight], m_FrustumCorners[kNearLowerRight], color, viewProj);
		DebugDraw::DrawLine(m_FrustumCorners[kNearLowerRight], m_FrustumCorners[kNearLowerLeft], color, viewProj);

		// Far rect
		DebugDraw::DrawLine(m_FrustumCorners[kFarLowerLeft], m_FrustumCorners[kFarUpperLeft], color, viewProj);
		DebugDraw::DrawLine(m_FrustumCorners[kFarUpperLeft], m_FrustumCorners[kFarUpperRight], color, viewProj);
		DebugDraw::DrawLine(m_FrustumCorners[kFarUpperRight], m_FrustumCorners[kFarLowerRight], color, viewProj);
		DebugDraw::DrawLine(m_FrustumCorners[kFarLowerRight], m_FrustumCorners[kFarLowerLeft], color, viewProj);

		// Sides
		DebugDraw::DrawLine(m_FrustumCorners[kNearLowerLeft], m_FrustumCorners[kFarLowerLeft], color, viewProj);
		DebugDraw::DrawLine(m_FrustumCorners[kNearUpperLeft], m_FrustumCorners[kFarUpperLeft], color, viewProj);
		DebugDraw::DrawLine(m_FrustumCorners[kNearLowerRight], m_FrustumCorners[kFarLowerRight], color, viewProj);
		DebugDraw::DrawLine(m_FrustumCorners[kNearUpperRight], m_FrustumCorners[kFarUpperRight], color, viewProj);
	}
}
