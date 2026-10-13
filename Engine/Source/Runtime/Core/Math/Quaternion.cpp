#include "Quaternion.h"
#include "Vector3.h"
#include "Matrix3x3.h"
#include "Matrix4x4.h"

namespace AtomEngine
{
	const Quaternion Quaternion::ZERO(0, 0, 0, 0);
	const Quaternion Quaternion::IDENTITY(0, 0, 0, 1);

	Quaternion Quaternion::operator*(const Quaternion& q) const
	{
		Vector3 v1 = Vector3(x, y, z);
		Vector3 v2 = Vector3(q.x, q.y, q.z);

		float w1 = w;
		float w2 = q.w;

		float dot = Math::Dot(v1, v2);
		Vector3 cross = Math::Cross(v1, v2);

		float w_ = w1 * w2 - dot;
		Vector3 v = cross + v1 * w2 + v2 * w1;
		return Quaternion(v.x, v.y, v.z, w_);
	}

	void Quaternion::FromRotationMatrix(const Matrix3x3& rotation)
	{
		float trace = rotation.mat[0][0] + rotation.mat[1][1] + rotation.mat[2][2];
		float root;

		if (trace > 0.0)
		{
			root = std::sqrt(trace + 1.0f);
			w = 0.5f * root;
			root = 0.5f / root;
			x = (rotation.mat[1][2] - rotation.mat[2][1]) * root;
			y = (rotation.mat[2][0] - rotation.mat[0][2]) * root;
			z = (rotation.mat[0][1] - rotation.mat[1][0]) * root;
		}
		else
		{
			size_t s_iNext[3] = { 1, 2, 0 };
			size_t i = 0;
			if (rotation.mat[1][1] > rotation.mat[0][0])
				i = 1;
			if (rotation.mat[2][2] > rotation.mat[i][i])
				i = 2;
			size_t j = s_iNext[i];
			size_t k = s_iNext[j];

			root = std::sqrt(rotation.mat[i][i] - rotation.mat[j][j] - rotation.mat[k][k] + 1.0f);
			float* apkQuat[3] = { &x, &y, &z };
			*apkQuat[i] = 0.5f * root;
			root = 0.5f / root;
			w = (rotation.mat[j][k] - rotation.mat[k][j]) * root;
			*apkQuat[j] = (rotation.mat[j][i] + rotation.mat[i][j]) * root;
			*apkQuat[k] = (rotation.mat[k][i] + rotation.mat[i][k]) * root;
		}
	}

	void Quaternion::FromRotationMatrix(const Matrix4x4& rotation)
	{
		Matrix3x3 r(rotation);

		FromRotationMatrix(r);
	}

	void Quaternion::FromAngleAxis(const Radian& angle, const Vector3& axis)
	{
        Vector3 v = axis.NormalizedCopy();
		Radian halfAngle(0.5f * angle);
		float  sin_v = Math::sin(halfAngle);
		x = sin_v * v.x;
		y = sin_v * v.y;
		z = sin_v * v.z;
		w = Math::cos(halfAngle);
	}

	Quaternion Quaternion::GetQuaternionFromAngleAxis(const Radian& angle, const Vector3& axis)
	{
		Quaternion q;
		q.FromAngleAxis(angle, axis);
		return q;
	}

	void Quaternion::FromDirection(const Vector3& direction, const Vector3& upDir)
	{
		Vector3 forward = direction;
		forward.z = 0.0f;
		forward.Normalize();

		Vector3 rightDir = Math::Cross(upDir,forward);

		FromAxes(rightDir, forward, upDir);
		Normalize();
	}

	Quaternion Quaternion::GetQuaternionFromDirection(const Vector3& direction, const Vector3& upDir)
	{
		Quaternion orientation;
		orientation.FromDirection(direction, upDir);
		return orientation;
	}

	Quaternion Quaternion::Slerp(const Quaternion& q1, const Quaternion& q2, float t)
	{
		float dot = q1.Dot(q2);
		dot = std::clamp(dot, -1.0f, 1.0f);
		float theta = Math::acos(dot) * t;
		Quaternion rel = (q2 - q1 * dot).NormalizeCopy();
		return q1 * Math::cos(theta) + rel * Math::sin(theta);
	}

	Quaternion Quaternion::LookRotation(const Vector3& forward, const Vector3& up)
	{
		Vector3 f = forward.NormalizedCopy();

		Vector3 u = up;
		if (fabsf(Math::Dot(f, u)) > 0.999f)
			u = fabsf(f.y) < 0.999f ? Vector3::UP : Vector3::RIGHT;

		Vector3 r = Math::Cross(u, f).NormalizedCopy();
		u = Math::Cross(f, r).NormalizedCopy();

		Matrix3x3 rot(r, u, f);
		Quaternion q;
		q.FromRotationMatrix(rot);
		return q;
	}

	float Quaternion::Dot(const Quaternion& lhs, const Quaternion& rhs)
	{
		return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w;
	}

	void Quaternion::ToAngleAxis(Radian& angle, Vector3& axis) const
	{
		float sqr_len = x * x + y * y + z * z;
		if (sqr_len > 0.0)
		{
			angle = 2.0f * Math::acos(w);
			float inv_len = Math::InvSqrt(sqr_len);
			axis.x = x * inv_len;
			axis.y = y * inv_len;
			axis.z = z * inv_len;
		}
		else
		{
			angle = Radian(0.0);
			axis.x = 1.0;
			axis.y = 0.0;
			axis.z = 0.0;
		}
	}

	void Quaternion::FromAxes(const Vector3& xaxis, const Vector3& yaxis, const Vector3& zaxis)
	{
		Matrix3x3 rot(xaxis, yaxis, zaxis);
		FromRotationMatrix(rot);
	}

	Vector3 Quaternion::xAxis() const
	{
		float tx = 2.0f * x;
		float ty = 2.0f * y;
		float tz = 2.0f * z;
		float twy = ty * w;
		float twz = tz * w;
		float txy = ty * x;
		float txz = tz * x;
		float tyy = ty * y;
		float tzz = tz * z;

		return Vector3(1.0f - (tyy + tzz), txy + twz, txz - twy);
	}
	//-----------------------------------------------------------------------
	Vector3 Quaternion::yAxis() const
	{
		float tx = 2.0f * x;
		float ty = 2.0f * y;
		float tz = 2.0f * z;
		float twx = tx * w;
		float twz = tz * w;
		float txx = tx * x;
		float txy = ty * x;
		float tyz = tz * y;
		float tzz = tz * z;

		return Vector3(txy - twz, 1.0f - (txx + tzz), tyz + twx);
	}
	//-----------------------------------------------------------------------
	Vector3 Quaternion::zAxis() const
	{
		float tx = 2.0f * x;
		float ty = 2.0f * y;
		float tz = 2.0f * z;
		float twx = tx * w;
		float twy = ty * w;
		float txx = tx * x;
		float txz = tz * x;
		float tyy = ty * y;
		float tyz = tz * y;

		return Vector3(txz + twy, tyz - twx, 1.0f - (txx + tyy));
	}
	//-----------------------------------------------------------------------
	void Quaternion::ToAxes(Vector3& xaxis, Vector3& yaxis, Vector3& zaxis) const
	{
		Matrix3x3 rot(*this);

		xaxis.x = rot.mat[0][0];
		xaxis.y = rot.mat[0][1];
		xaxis.z = rot.mat[0][2];

		yaxis.x = rot.mat[1][0];
		yaxis.y = rot.mat[1][1];
		yaxis.z = rot.mat[1][2];

		zaxis.x = rot.mat[2][0];
		zaxis.y = rot.mat[2][1];
		zaxis.z = rot.mat[2][2];
	}

	Vector3 Quaternion::operator*(const Vector3& v) const
	{
		Quaternion vq = Quaternion(v.x, v.y, v.z, 0);

		Quaternion temp = (*this) * vq;
		Quaternion result = temp * this->Conjugate();

		return Vector3{ result.x, result.y, result.z };
	}

	Radian Quaternion::Yaw(bool reprojectAxis) const
	{
		if (reprojectAxis)
		{
			float ty = 2.0f * y;
			float tz = 2.0f * z;
			float txy = ty * x;
			float tyy = ty * y;
			float tzz = tz * z;
			float twz = tz * w;

			return Radian(Math::atan2(-(txy + twz), 1.0f - (tyy + tzz)));
		}
		else
		{
			return Radian(Math::atan2(-2 * (x * y + w * z), w * w + x * x - y * y - z * z));
		}
	}

	Radian Quaternion::Pitch(bool reprojectAxis) const
	{
		if (reprojectAxis)
		{
			float tx = 2.0f * x;
			float tz = 2.0f * z;
			float txx = tx * x;
			float tyz = tz * y;
			float tzz = tz * z;
			float twx = tx * w;

			return Radian(Math::atan2(tyz + twx, 1.0f - (txx + tzz)));
		}
		else
		{
			return Radian(Math::atan2(2 * (y * z + w * x), w * w - x * x - y * y + z * z));
		}
	}
	//-----------------------------------------------------------------------
	Radian Quaternion::Roll(bool reproject_axis) const
	{
		if (reproject_axis)
		{

			float tx = 2.0f * x;
			float ty = 2.0f * y;
			float tz = 2.0f * z;
			float twy = ty * w;
			float txx = tx * x;
			float txz = tz * x;
			float tyy = ty * y;

			return Radian(Math::atan2(txz + twy, 1.0f - (txx + tyy)));
		}
		else
		{
			return Radian(Math::asin(-2 * (x * z - w * y)));
		}
	}

	Quaternion Slerp(const Quaternion& kp, const Quaternion& kq, float t, bool shortestPath)
	{
		float      cos_v = kp.Dot(kq);
		Quaternion kt;

		if (cos_v < 0.0f && shortestPath)
		{
			cos_v = -cos_v;
			kt = -kq;
		}
		else
		{
			kt = kq;
		}

		if (Math::Abs(cos_v) < 1 - Math::Epsilon)
		{
			float  sin_v = Math::Sqrt(1 - Math::Sqr(cos_v));
			Radian angle = Math::atan2(sin_v, cos_v);
			float  inv_sin = 1.0f / sin_v;
			float  coeff0 = Math::sin((1.0f - t) * angle) * inv_sin;
			float  coeff1 = Math::sin(t * angle) * inv_sin;
			return coeff0 * kp + coeff1 * kt;
		}
		else
		{
			Quaternion r = (1.0f - t) * kp + t * kt;
			r.Normalize();
			return r;
		}
	}

	Quaternion Lerp(const Quaternion& kp, const Quaternion& kq, float t, bool shortestPath)
	{
		Quaternion result;
		float      cos_value = kp.Dot(kq);
		if (cos_value < 0.0f && shortestPath)
		{
			result = kp + t * ((-kq) - kp);
		}
		else
		{
			result = kp + t * (kq - kp);
		}
		result.Normalize();
		return result;
	}
}
