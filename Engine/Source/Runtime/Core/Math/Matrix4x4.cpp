#include "Matrix4x4.h"

namespace AtomEngine
{
	const Matrix4x4 Matrix4x4::ZERO(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	const Matrix4x4 Matrix4x4::ZEROAFFINE(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1);
	const Matrix4x4 Matrix4x4::IDENTITY(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);

	Matrix4x4 Matrix4x4::InverseAffine(void) const
	{
		assert(IsAffine());

		float m10 = mat[1][0], m11 = mat[1][1], m12 = mat[1][2];
		float m20 = mat[2][0], m21 = mat[2][1], m22 = mat[2][2];

		float t00 = m22 * m11 - m21 * m12;
		float t10 = m20 * m12 - m22 * m10;
		float t20 = m21 * m10 - m20 * m11;

		float m00 = mat[0][0], m01 = mat[0][1], m02 = mat[0][2];

		float inv_det = 1 / (m00 * t00 + m01 * t10 + m02 * t20);

		t00 *= inv_det;
		t10 *= inv_det;
		t20 *= inv_det;

		m00 *= inv_det;
		m01 *= inv_det;
		m02 *= inv_det;

		float r00 = t00;
		float r01 = m02 * m21 - m01 * m22;
		float r02 = m01 * m12 - m02 * m11;

		float r10 = t10;
		float r11 = m00 * m22 - m02 * m20;
		float r12 = m02 * m10 - m00 * m12;

		float r20 = t20;
		float r21 = m01 * m20 - m00 * m21;
		float r22 = m00 * m11 - m01 * m10;

		float m03 = mat[0][3], m13 = mat[1][3], m23 = mat[2][3];

		float r03 = -(r00 * m03 + r01 * m13 + r02 * m23);
		float r13 = -(r10 * m03 + r11 * m13 + r12 * m23);
		float r23 = -(r20 * m03 + r21 * m13 + r22 * m23);

		return Matrix4x4(r00, r01, r02, r03, r10, r11, r12, r13, r20, r21, r22, r23, 0, 0, 0, 1);
	}

	void Matrix4x4::MakeAffine(const Vector3& scale, const Quaternion& orientation, const Vector3& position)
	{
		Matrix4x4 rot(orientation);

		mat[0][0] = scale.x * rot.mat[0][0];
		mat[0][1] = scale.y * rot.mat[0][1];
		mat[0][2] = scale.z * rot.mat[0][2];
		mat[0][3] = 0.0f;

		mat[1][0] = scale.x * rot.mat[1][0];
		mat[1][1] = scale.y * rot.mat[1][1];
		mat[1][2] = scale.z * rot.mat[1][2];
		mat[1][3] = 0.0f;

		mat[2][0] = scale.x * rot.mat[2][0];
		mat[2][1] = scale.y * rot.mat[2][1];
		mat[2][2] = scale.z * rot.mat[2][2];
		mat[2][3] = 0.0f;

		mat[3][0] = position.x;
		mat[3][1] = position.y;
		mat[3][2] = position.z;
		mat[3][3] = 1.0f;
	}

	void Matrix4x4::MakeAffine(const Vector3& scale, const Vector3& orientation, const Vector3& position)
	{
		Matrix4x4 rot = MakeRotateMatrix(orientation);

		mat[0][0] = scale.x * rot.mat[0][0];
		mat[0][1] = scale.y * rot.mat[0][1];
		mat[0][2] = scale.z * rot.mat[0][2];
		mat[0][3] = 0.0f;

		mat[1][0] = scale.x * rot.mat[1][0];
		mat[1][1] = scale.y * rot.mat[1][1];
		mat[1][2] = scale.z * rot.mat[1][2];
		mat[1][3] = 0.0f;

		mat[2][0] = scale.x * rot.mat[2][0];
		mat[2][1] = scale.y * rot.mat[2][1];
		mat[2][2] = scale.z * rot.mat[2][2];
		mat[2][3] = 0.0f;

		mat[3][0] = position.x;
		mat[3][1] = position.y;
		mat[3][2] = position.z;
		mat[3][3] = 1.0f;
	}

	Matrix4x4 Matrix4x4::MakeAffineMatrix(const Vector3& scale, const Vector3& orientation, const Vector3& position)
	{
		return MakeScaleMatrix(scale) * MakeRotateMatrix(orientation) * MakeTransMatrix(position);
	}

	void Matrix4x4::MakeInverseTransform(const Vector3& scale, const Quaternion& orientation, const Vector3& position)
	{
		Vector3    inv_translate = -position;
		Vector3    inv_scale(1 / scale.x, 1 / scale.y, 1 / scale.z);
		Quaternion inv_rot = orientation.Inverse();

		inv_translate = inv_rot * inv_translate;
		inv_translate *= inv_scale;

		Matrix3x3 rot3x3(inv_rot);

		mat[0][0] = inv_scale.x * rot3x3.mat[0][0];
		mat[0][1] = inv_scale.x * rot3x3.mat[0][1];
		mat[0][2] = inv_scale.x * rot3x3.mat[0][2];
		mat[0][3] = 0;
		mat[1][0] = inv_scale.y * rot3x3.mat[1][0];
		mat[1][1] = inv_scale.y * rot3x3.mat[1][1];
		mat[1][2] = inv_scale.y * rot3x3.mat[1][2];
		mat[1][3] = 0;
		mat[2][0] = inv_scale.z * rot3x3.mat[2][0];
		mat[2][1] = inv_scale.z * rot3x3.mat[2][1];
		mat[2][2] = inv_scale.z * rot3x3.mat[2][2];
		mat[2][3] = 0;

		mat[3][0] = inv_translate.x;
		mat[3][1] = inv_translate.y;
		mat[3][2] = inv_translate.z;
		mat[3][3] = 1.0f;
	}

	Vector4 operator*(const Vector4& v, const Matrix4x4& mat)
	{
		return Vector4(v.x * mat[0][0] + v.y * mat[1][0] + v.z * mat[2][0] + v.w * mat[3][0],
			v.x * mat[0][1] + v.y * mat[1][1] + v.z * mat[2][1] + v.w * mat[3][1],
			v.x * mat[0][2] + v.y * mat[1][2] + v.z * mat[2][2] + v.w * mat[3][2],
			v.x * mat[0][3] + v.y * mat[1][3] + v.z * mat[2][3] + v.w * mat[3][3]);
	}
}