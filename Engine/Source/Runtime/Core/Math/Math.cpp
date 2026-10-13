#include "Math.h"
#include "Vector3.h"
#include "Matrix4x4.h"

namespace AtomEngine
{

	bool Math::Equal(float a, float b, float tolerance)
	{
		return std::fabs(b - a) <= tolerance;
	}

	float Math::DegreesToRadians(float degrees) { return degrees * deg2Rad; }

	float Math::RadiansToDegrees(float radians) { return radians * rad2Deg; }

	Radian Math::acos(float value)
	{
		if (-1.0 < value)
		{
			if (value < 1.0)
				return Radian(::acos(value));

			return Radian(0.0);
		}

		return Radian(PI);
	}

	Radian Math::asin(float value)
	{
		if (-1.0 < value)
		{
			if (value < 1.0)
				return Radian(::asin(value));

			return Radian(HalfPI);
		}

		return Radian(-HalfPI);
	}

	Vector3 Math::Normalize(const Vector3& v)
	{
		if (v.IsZero())
			return Vector3::UNIT_SCALE;
		return v.NormalizedCopy();
	}

	float Math::Dot(const Vector3& lhs, const Vector3& rhs)
	{
		return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
	}

	Vector3 Math::Cross(const Vector3& lhs, const Vector3& rhs)
	{
		if (lhs.IsZero() || rhs.IsZero())
			return Vector3::ZERO;
		return Vector3(
			lhs.y * rhs.z - lhs.z * rhs.y,
			lhs.z * rhs.x - lhs.x * rhs.z,
			lhs.x * rhs.y - lhs.y * rhs.x
		);
	}

	Matrix4x4 Math::MakeViewMatrix(const Vector3& position, const Quaternion& orientation, const Matrix4x4* reflectMatrix)
	{
		return Matrix4x4();
	}

	Matrix4x4 Math::MakeLookAtMatrix(const Vector3& eyePosition, const Vector3& targetPosition, const Vector3& upDir)
	{
		XMVECTOR eye = XMVectorSet(eyePosition.x, eyePosition.y, eyePosition.z, 1.0f);
		XMVECTOR target = XMVectorSet(targetPosition.x, targetPosition.y, targetPosition.z, 1.0f);
		XMVECTOR up = XMVectorSet(upDir.x, upDir.y, upDir.z, 0.0f);

		XMMATRIX view = XMMatrixLookAtLH(eye, target, up);
		return Matrix4x4{
			Vector4(view.r[0].m128_f32),
			Vector4(view.r[1].m128_f32),
			Vector4(view.r[2].m128_f32),
			Vector4(view.r[3].m128_f32)
		};
	}

	Matrix4x4 Math::MakePerspectiveMatrix(Radian fovy, float aspect, float znear, float zfar)
	{
		Matrix4x4 result = {
			1.0f / (aspect * tan(fovy * 0.5f)), 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f / tan(fovy * 0.5f), 0.0f, 0.0f,
			0.0f, 0.0f, zfar / (zfar - znear), 1.0f,
			0.0f, 0.0f, -zfar * znear / (zfar - znear), 0.0f
		};
		return result;
	}

	Matrix4x4 Math::MakeOrthographicProjectionMatrix(float left, float right, float bottom, float top, float nearClip, float farClip)
	{
		Matrix4x4 result = {
			2 / (right - left), 0.0f, 0.0f, 0.0f,
			0.0f, 2 / (top - bottom), 0.0f, 0.0f,
			0.0f, 0.0f, 1 / (farClip - nearClip), 0.0f,
			(left + right) / (left - right), (top + bottom) / (bottom - top), nearClip / (nearClip - farClip), 1.0f
		};
		return result;
	}
	float ToFloat(const int8_t x) { return std::max(x / 127.0f, -1.0f); }
	float ToFloat(const uint8_t x) { return x / 255.0f; }
	float ToFloat(const int16_t x) { return std::max(x / 32767.0f, -1.0f); }
	float ToFloat(const uint16_t x) { return x / 65535.0f; }
}
