#pragma once
#include "Math.h"
#include "Quaternion.h"
#include "Vector3.h"
#include "Vector4.h"
#include "Matrix3x3.h"

namespace AtomEngine
{
	__declspec(align(16))class Matrix4x4
	{
	public:
		float mat[4][4];

	public:
		Matrix4x4(const Matrix4x4& m)
		{
			mat[0][0] = m.mat[0][0];
			mat[0][1] = m.mat[0][1];
			mat[0][2] = m.mat[0][2];
			mat[0][3] = m.mat[0][3];
			mat[1][0] = m.mat[1][0];
			mat[1][1] = m.mat[1][1];
			mat[1][2] = m.mat[1][2];
			mat[1][3] = m.mat[1][3];
			mat[2][0] = m.mat[2][0];
			mat[2][1] = m.mat[2][1];
			mat[2][2] = m.mat[2][2];
			mat[2][3] = m.mat[2][3];
			mat[3][0] = m.mat[3][0];
			mat[3][1] = m.mat[3][1];
			mat[3][2] = m.mat[3][2];
			mat[3][3] = m.mat[3][3];
		}

		Matrix4x4(const Matrix3x3& m)
		{
			mat[0][0] = m.mat[0][0];
			mat[0][1] = m.mat[0][1];
			mat[0][2] = m.mat[0][2];
			mat[0][3] = 0.0f;

			mat[1][0] = m.mat[1][0];
			mat[1][1] = m.mat[1][1];
			mat[1][2] = m.mat[1][2];
			mat[1][3] = 0.0f;

			mat[2][0] = m.mat[2][0];
			mat[2][1] = m.mat[2][1];
			mat[2][2] = m.mat[2][2];
			mat[2][3] = 0.0f;

			mat[3][0] = 0.0f;
			mat[3][1] = 0.0f;
			mat[3][2] = 0.0f;
			mat[3][3] = 1.0f;
		}

		Matrix4x4() { operator=(IDENTITY); }

		Matrix4x4(const float(&float_array)[16])
		{
			mat[0][0] = float_array[0];
			mat[0][1] = float_array[1];
			mat[0][2] = float_array[2];
			mat[0][3] = float_array[3];
			mat[1][0] = float_array[4];
			mat[1][1] = float_array[5];
			mat[1][2] = float_array[6];
			mat[1][3] = float_array[7];
			mat[2][0] = float_array[8];
			mat[2][1] = float_array[9];
			mat[2][2] = float_array[10];
			mat[2][3] = float_array[11];
			mat[3][0] = float_array[12];
			mat[3][1] = float_array[13];
			mat[3][2] = float_array[14];
			mat[3][3] = float_array[15];
		}

		Matrix4x4(const Vector3& x, const Vector3& y, const Vector3& z, const Vector3& w)
		{
			mat[0][0] = x.x;
			mat[0][1] = x.y;
			mat[0][2] = x.z;
			mat[0][3] = 0.0f;
			mat[1][0] = y.x;
			mat[1][1] = y.y;
			mat[1][2] = y.z;
			mat[1][3] = 0.0f;
			mat[2][0] = z.x;
			mat[2][1] = z.y;
			mat[2][2] = z.z;
			mat[2][3] = 0.0f;
			mat[3][0] = w.x;
			mat[3][1] = w.y;
			mat[3][2] = w.z;
			mat[3][3] = 1.0f;
		}

		Matrix4x4(float m00,
			float m01,
			float m02,
			float m03,
			float m10,
			float m11,
			float m12,
			float m13,
			float m20,
			float m21,
			float m22,
			float m23,
			float m30,
			float m31,
			float m32,
			float m33)
		{
			mat[0][0] = m00;
			mat[0][1] = m01;
			mat[0][2] = m02;
			mat[0][3] = m03;
			mat[1][0] = m10;
			mat[1][1] = m11;
			mat[1][2] = m12;
			mat[1][3] = m13;
			mat[2][0] = m20;
			mat[2][1] = m21;
			mat[2][2] = m22;
			mat[2][3] = m23;
			mat[3][0] = m30;
			mat[3][1] = m31;
			mat[3][2] = m32;
			mat[3][3] = m33;
		}

		Matrix4x4(const Vector4& row0, const Vector4& row1, const Vector4& row2, const Vector4& row3)
		{
			mat[0][0] = row0.x;
			mat[0][1] = row0.y;
			mat[0][2] = row0.z;
			mat[0][3] = row0.w;
			mat[1][0] = row1.x;
			mat[1][1] = row1.y;
			mat[1][2] = row1.z;
			mat[1][3] = row1.w;
			mat[2][0] = row2.x;
			mat[2][1] = row2.y;
			mat[2][2] = row2.z;
			mat[2][3] = row2.w;
			mat[3][0] = row3.x;
			mat[3][1] = row3.y;
			mat[3][2] = row3.z;
			mat[3][3] = row3.w;
		}

		Matrix4x4(float* float_array)
		{
			mat[0][0] = float_array[0];
			mat[0][1] = float_array[1];
			mat[0][2] = float_array[2];
			mat[0][3] = float_array[3];
			mat[1][0] = float_array[4];
			mat[1][1] = float_array[5];
			mat[1][2] = float_array[6];
			mat[1][3] = float_array[7];
			mat[2][0] = float_array[8];
			mat[2][1] = float_array[9];
			mat[2][2] = float_array[10];
			mat[2][3] = float_array[11];
			mat[3][0] = float_array[12];
			mat[3][1] = float_array[13];
			mat[3][2] = float_array[14];
			mat[3][3] = float_array[15];
		}

		Matrix4x4(const Vector3& scale, const Quaternion& rotation, const Vector3& position)
		{
			MakeAffine(scale, rotation, position);
		}

		Matrix4x4(const Vector3& x, const Vector3& y, const Vector3& z)
		{
			mat[0][0] = x.x;
			mat[0][1] = x.y;
			mat[0][2] = x.z;
			mat[0][3] = 0.0f;

			mat[1][0] = y.x;
			mat[1][1] = y.y;
			mat[1][2] = y.z;
			mat[1][3] = 0.0f;

			mat[2][0] = z.x;
			mat[2][1] = z.y;
			mat[2][2] = z.z;
			mat[2][3] = 0.0f;

			mat[3][0] = 0.0f;
			mat[3][1] = 0.0f;
			mat[3][2] = 0.0f;
			mat[3][3] = 1.0f;
		}

		Matrix4x4(const Matrix3x3& xyz, const Vector3& w)
		{
			mat[0][0] = xyz.mat[0][0];
			mat[0][1] = xyz.mat[0][1];
			mat[0][2] = xyz.mat[0][2];
			mat[0][3] = 0.0f;

			mat[1][0] = xyz.mat[1][0];
			mat[1][1] = xyz.mat[1][1];
			mat[1][2] = xyz.mat[1][2];
			mat[1][3] = 0.0f;

			mat[2][0] = xyz.mat[2][0];
			mat[2][1] = xyz.mat[2][1];
			mat[2][2] = xyz.mat[2][2];
			mat[2][3] = 0.0f;

			mat[3][0] = w.x;
			mat[3][1] = w.y;
			mat[3][2] = w.z;
			mat[3][3] = 1.0f;
		}

		Matrix4x4(const Quaternion& r, const Vector3& position)
		{

			float xx = r.x * r.x;
			float yy = r.y * r.y;
			float zz = r.z * r.z;
			float xy = r.x * r.y;
			float xz = r.x * r.z;
			float yz = r.y * r.z;
			float wx = r.w * r.x;
			float wy = r.w * r.y;
			float wz = r.w * r.z;
			float ww = r.w * r.w;

			mat[0][0] = ww + xx - yy - zz;
			mat[0][1] = 2.0f * (xy + wz);
			mat[0][2] = 2.0f * (xz - wy);
			mat[0][3] = 0.0f;

			mat[1][0] = 2.0f * (xy - wz);
			mat[1][1] = ww - xx + yy - zz;
			mat[1][2] = 2.0f * (yz + wx);
			mat[1][3] = 0.0f;

			mat[2][0] = 2.0f * (xz + wy);
			mat[2][1] = 2.0f * (yz - wx);
			mat[2][2] = ww - xx - yy + zz;
			mat[2][3] = 0.0f;

			mat[3][0] = position.x;
			mat[3][1] = position.y;
			mat[3][2] = position.z;
			mat[3][3] = 1.0f;
		}

		Matrix4x4(const Quaternion& rot)
		{
			*this = Matrix4x4(rot, Vector3::ZERO);
		}

		float* operator[](size_t row_index)
		{
			assert(row_index < 4);
			return mat[row_index];
		}

		const float* operator[](size_t row_index) const
		{
			assert(row_index < 4);
			return mat[row_index];
		}

		Matrix4x4 operator*(const Matrix4x4& m2) const
		{
			Matrix4x4 result;
			result.mat[0][0] = mat[0][0] * m2.mat[0][0] + mat[0][1] * m2.mat[1][0] + mat[0][2] * m2.mat[2][0] + mat[0][3] * m2.mat[3][0];
			result.mat[0][1] = mat[0][0] * m2.mat[0][1] + mat[0][1] * m2.mat[1][1] + mat[0][2] * m2.mat[2][1] + mat[0][3] * m2.mat[3][1];
			result.mat[0][2] = mat[0][0] * m2.mat[0][2] + mat[0][1] * m2.mat[1][2] + mat[0][2] * m2.mat[2][2] + mat[0][3] * m2.mat[3][2];
			result.mat[0][3] = mat[0][0] * m2.mat[0][3] + mat[0][1] * m2.mat[1][3] + mat[0][2] * m2.mat[2][3] + mat[0][3] * m2.mat[3][3];

			result.mat[1][0] = mat[1][0] * m2.mat[0][0] + mat[1][1] * m2.mat[1][0] + mat[1][2] * m2.mat[2][0] + mat[1][3] * m2.mat[3][0];
			result.mat[1][1] = mat[1][0] * m2.mat[0][1] + mat[1][1] * m2.mat[1][1] + mat[1][2] * m2.mat[2][1] + mat[1][3] * m2.mat[3][1];
			result.mat[1][2] = mat[1][0] * m2.mat[0][2] + mat[1][1] * m2.mat[1][2] + mat[1][2] * m2.mat[2][2] + mat[1][3] * m2.mat[3][2];
			result.mat[1][3] = mat[1][0] * m2.mat[0][3] + mat[1][1] * m2.mat[1][3] + mat[1][2] * m2.mat[2][3] + mat[1][3] * m2.mat[3][3];

			result.mat[2][0] = mat[2][0] * m2.mat[0][0] + mat[2][1] * m2.mat[1][0] + mat[2][2] * m2.mat[2][0] + mat[2][3] * m2.mat[3][0];
			result.mat[2][1] = mat[2][0] * m2.mat[0][1] + mat[2][1] * m2.mat[1][1] + mat[2][2] * m2.mat[2][1] + mat[2][3] * m2.mat[3][1];
			result.mat[2][2] = mat[2][0] * m2.mat[0][2] + mat[2][1] * m2.mat[1][2] + mat[2][2] * m2.mat[2][2] + mat[2][3] * m2.mat[3][2];
			result.mat[2][3] = mat[2][0] * m2.mat[0][3] + mat[2][1] * m2.mat[1][3] + mat[2][2] * m2.mat[2][3] + mat[2][3] * m2.mat[3][3];

			result.mat[3][0] = mat[3][0] * m2.mat[0][0] + mat[3][1] * m2.mat[1][0] + mat[3][2] * m2.mat[2][0] + mat[3][3] * m2.mat[3][0];
			result.mat[3][1] = mat[3][0] * m2.mat[0][1] + mat[3][1] * m2.mat[1][1] + mat[3][2] * m2.mat[2][1] + mat[3][3] * m2.mat[3][1];
			result.mat[3][2] = mat[3][0] * m2.mat[0][2] + mat[3][1] * m2.mat[1][2] + mat[3][2] * m2.mat[2][2] + mat[3][3] * m2.mat[3][2];
			result.mat[3][3] = mat[3][0] * m2.mat[0][3] + mat[3][1] * m2.mat[1][3] + mat[3][2] * m2.mat[2][3] + mat[3][3] * m2.mat[3][3];

			return result;
		}

		Vector3 operator*(const Vector3& v) const
		{
			Vector3 r;

			float inv_w = 1.0f / (mat[3][0] * v.x + mat[3][1] * v.y + mat[3][2] * v.z + mat[3][3]);

			r.x = (mat[0][0] * v.x + mat[0][1] * v.y + mat[0][2] * v.z + mat[0][3]) * inv_w;
			r.y = (mat[1][0] * v.x + mat[1][1] * v.y + mat[1][2] * v.z + mat[1][3]) * inv_w;
			r.z = (mat[2][0] * v.x + mat[2][1] * v.y + mat[2][2] * v.z + mat[2][3]) * inv_w;

			return r;
		}

		Vector4 operator*(const Vector4& v) const
		{
			return Vector4(mat[0][0] * v.x + mat[0][1] * v.y + mat[0][2] * v.z + mat[0][3] * v.w,
				mat[1][0] * v.x + mat[1][1] * v.y + mat[1][2] * v.z + mat[1][3] * v.w,
				mat[2][0] * v.x + mat[2][1] * v.y + mat[2][2] * v.z + mat[2][3] * v.w,
				mat[3][0] * v.x + mat[3][1] * v.y + mat[3][2] * v.z + mat[3][3] * v.w);
		}


		Matrix4x4 operator+(const Matrix4x4& m2) const
		{
			Matrix4x4 r;

			r.mat[0][0] = mat[0][0] + m2.mat[0][0];
			r.mat[0][1] = mat[0][1] + m2.mat[0][1];
			r.mat[0][2] = mat[0][2] + m2.mat[0][2];
			r.mat[0][3] = mat[0][3] + m2.mat[0][3];

			r.mat[1][0] = mat[1][0] + m2.mat[1][0];
			r.mat[1][1] = mat[1][1] + m2.mat[1][1];
			r.mat[1][2] = mat[1][2] + m2.mat[1][2];
			r.mat[1][3] = mat[1][3] + m2.mat[1][3];

			r.mat[2][0] = mat[2][0] + m2.mat[2][0];
			r.mat[2][1] = mat[2][1] + m2.mat[2][1];
			r.mat[2][2] = mat[2][2] + m2.mat[2][2];
			r.mat[2][3] = mat[2][3] + m2.mat[2][3];

			r.mat[3][0] = mat[3][0] + m2.mat[3][0];
			r.mat[3][1] = mat[3][1] + m2.mat[3][1];
			r.mat[3][2] = mat[3][2] + m2.mat[3][2];
			r.mat[3][3] = mat[3][3] + m2.mat[3][3];

			return r;
		}


		Matrix4x4 operator-(const Matrix4x4& m2) const
		{
			Matrix4x4 r;
			r.mat[0][0] = mat[0][0] - m2.mat[0][0];
			r.mat[0][1] = mat[0][1] - m2.mat[0][1];
			r.mat[0][2] = mat[0][2] - m2.mat[0][2];
			r.mat[0][3] = mat[0][3] - m2.mat[0][3];

			r.mat[1][0] = mat[1][0] - m2.mat[1][0];
			r.mat[1][1] = mat[1][1] - m2.mat[1][1];
			r.mat[1][2] = mat[1][2] - m2.mat[1][2];
			r.mat[1][3] = mat[1][3] - m2.mat[1][3];

			r.mat[2][0] = mat[2][0] - m2.mat[2][0];
			r.mat[2][1] = mat[2][1] - m2.mat[2][1];
			r.mat[2][2] = mat[2][2] - m2.mat[2][2];
			r.mat[2][3] = mat[2][3] - m2.mat[2][3];

			r.mat[3][0] = mat[3][0] - m2.mat[3][0];
			r.mat[3][1] = mat[3][1] - m2.mat[3][1];
			r.mat[3][2] = mat[3][2] - m2.mat[3][2];
			r.mat[3][3] = mat[3][3] - m2.mat[3][3];

			return r;
		}

		Matrix4x4 operator*(float scalar) const
		{
			return Matrix4x4(scalar * mat[0][0],
				scalar * mat[0][1],
				scalar * mat[0][2],
				scalar * mat[0][3],
				scalar * mat[1][0],
				scalar * mat[1][1],
				scalar * mat[1][2],
				scalar * mat[1][3],
				scalar * mat[2][0],
				scalar * mat[2][1],
				scalar * mat[2][2],
				scalar * mat[2][3],
				scalar * mat[3][0],
				scalar * mat[3][1],
				scalar * mat[3][2],
				scalar * mat[3][3]);
		}

		Matrix4x4 Transpose() const
		{
			return Matrix4x4(mat[0][0],
				mat[1][0],
				mat[2][0],
				mat[3][0],
				mat[0][1],
				mat[1][1],
				mat[2][1],
				mat[3][1],
				mat[0][2],
				mat[1][2],
				mat[2][2],
				mat[3][2],
				mat[0][3],
				mat[1][3],
				mat[2][3],
				mat[3][3]);
		}

		void setTrans(const Vector3& v)
		{
			mat[3][0] = v.x;
			mat[3][1] = v.y;
			mat[3][2] = v.z;
		}

		Vector3 GetTrans() const { return Vector3(mat[3][0], mat[3][1], mat[3][2]); }

		Matrix3x3 Get3x3()const
		{
			return Matrix3x3(*this);
		}

		Quaternion GetRotation() const
		{
			return Quaternion(*this);
		}

		Vector3 GetScale() const
		{
			Vector3 result;
			result.x = mat[0][0];
			result.y = mat[1][1];
			result.z = mat[2][2];
			return result;
		}

		static Matrix4x4 RotationMatrix(Vector3 normal)
		{
			Vector3 up = Vector3(0, 0, 1);
			if (fabs(normal.z) > 0.999f)
			{
				up = Vector3(0, 1, 0);
			}

			Vector3 right = up.Cross(normal);
			up = normal.Cross(right);

			right.Normalize();
			up.Normalize();

			return Matrix3x3(right, up, normal);
		}

		void MakeTrans(const Vector3& v)
		{
			mat[0][0] = 1.0f;
			mat[0][1] = 0.0f;
			mat[0][2] = 0.0f;
			mat[0][3] = 0.0f;

			mat[1][0] = 0.0f;
			mat[1][1] = 1.0f;
			mat[1][2] = 0.0f;
			mat[1][3] = 0.0f;

			mat[2][0] = 0.0f;
			mat[2][1] = 0.0f;
			mat[2][2] = 1.0f;
			mat[2][3] = 0.0f;

			mat[3][0] = v.x;
			mat[3][1] = v.y;
			mat[3][2] = v.y;
			mat[3][3] = 1.0;
		}

		void MakeTrans(float tx, float ty, float tz)
		{
			mat[0][0] = 1.0f;
			mat[0][1] = 0.0f;
			mat[0][2] = 0.0f;
			mat[0][3] = 0.0f;

			mat[1][0] = 0.0f;
			mat[1][1] = 1.0f;
			mat[1][2] = 0.0f;
			mat[1][3] = 0.0f;

			mat[2][0] = 0.0f;
			mat[2][1] = 0.0f;
			mat[2][2] = 1.0f;
			mat[2][3] = 0.0f;

			mat[3][0] = tx;
			mat[3][1] = ty;
			mat[3][2] = tz;
			mat[3][3] = 1.0;
		}

		static Matrix4x4 MakeTransMatrix(const Vector3& v)
		{
			Matrix4x4 r;

			r.mat[0][0] = 1.0f;
			r.mat[0][1] = 0.0f;
			r.mat[0][2] = 0.0f;
			r.mat[0][3] = 0.0f;

			r.mat[1][0] = 0.0f;
			r.mat[1][1] = 1.0f;
			r.mat[1][2] = 0.0f;
			r.mat[1][3] = 0.0f;

			r.mat[2][0] = 0.0f;
			r.mat[2][1] = 0.0f;
			r.mat[2][2] = 1.0f;
			r.mat[2][3] = 0.0f;

			r.mat[3][0] = v.x;
			r.mat[3][1] = v.y;
			r.mat[3][2] = v.z;
			r.mat[3][3] = 1.0f;

			return r;
		}

		static Matrix4x4 GetTrans(float t_x, float t_y, float t_z)
		{
			Matrix4x4 r;

			r.mat[0][0] = 1.0;
			r.mat[0][1] = 0.0;
			r.mat[0][2] = 0.0;
			r.mat[0][3] = t_x;
			r.mat[1][0] = 0.0;
			r.mat[1][1] = 1.0;
			r.mat[1][2] = 0.0;
			r.mat[1][3] = t_y;
			r.mat[2][0] = 0.0;
			r.mat[2][1] = 0.0;
			r.mat[2][2] = 1.0;
			r.mat[2][3] = t_z;
			r.mat[3][0] = 0.0;
			r.mat[3][1] = 0.0;
			r.mat[3][2] = 0.0;
			r.mat[3][3] = 1.0;

			return r;
		}

		void SetScale(const Vector3& v)
		{
			mat[0][0] = v.x;
			mat[1][1] = v.y;
			mat[2][2] = v.z;
		}

		static Matrix4x4 MakeScaleMatrix(const Vector3& v)
		{
			Matrix4x4 r;
			r.mat[0][0] = v.x;
			r.mat[0][1] = 0.0;
			r.mat[0][2] = 0.0;
			r.mat[0][3] = 0.0;
			r.mat[1][0] = 0.0;
			r.mat[1][1] = v.y;
			r.mat[1][2] = 0.0;
			r.mat[1][3] = 0.0;
			r.mat[2][0] = 0.0;
			r.mat[2][1] = 0.0;
			r.mat[2][2] = v.z;
			r.mat[2][3] = 0.0;
			r.mat[3][0] = 0.0;
			r.mat[3][1] = 0.0;
			r.mat[3][2] = 0.0;
			r.mat[3][3] = 1.0;

			return r;
		}

		static Matrix4x4 MakeScaleMatrix(float s_x, float s_y, float s_z)
		{
			Matrix4x4 r;
			r.mat[0][0] = s_x;
			r.mat[0][1] = 0.0;
			r.mat[0][2] = 0.0;
			r.mat[0][3] = 0.0;
			r.mat[1][0] = 0.0;
			r.mat[1][1] = s_y;
			r.mat[1][2] = 0.0;
			r.mat[1][3] = 0.0;
			r.mat[2][0] = 0.0;
			r.mat[2][1] = 0.0;
			r.mat[2][2] = s_z;
			r.mat[2][3] = 0.0;
			r.mat[3][0] = 0.0;
			r.mat[3][1] = 0.0;
			r.mat[3][2] = 0.0;
			r.mat[3][3] = 1.0;

			return r;
		}

		static Matrix4x4 MakeRotateXMatrix(Radian radian)
		{
			float cos = Math::cos(radian);
			float sin = Math::sin(radian);

			return Matrix4x4(
				1.0, 0.0, 0.0, 0.0,
				0.0, cos, sin, 0.0,
				0.0, -sin, cos, 0.0,
				0.0, 0.0, 0.0, 1.0
			);
		}

		static Matrix4x4 MakeRotateYMatrix(Radian radian)
		{
			float cos = Math::cos(radian);
			float sin = Math::sin(radian);

			return Matrix4x4(
				cos, 0.0, -sin, 0.0,
				0.0, 1.0, 0.0, 0.0,
				sin, 0.0, cos, 0.0,
				0.0, 0.0, 0.0, 1.0
			);
		}

		static Matrix4x4 MakeRotateZMatrix(Radian radian)
		{
			float cos = Math::cos(radian);
			float sin = Math::sin(radian);

			return Matrix4x4(
				cos, sin, 0.0, 0.0,
				-sin, cos, 0.0, 0.0,
				0.0, 0.0, 1.0, 0.0,
				0.0, 0.0, 0.0, 1.0
			);
		}

		static Matrix4x4 MakeRotateMatrix(const Vector3& axis)
		{
			return MakeRotateXMatrix(Radian(axis.x)) * MakeRotateYMatrix(Radian(axis.y)) * MakeRotateZMatrix(Radian(axis.z));
		}

		void Extract3x3Matrix(Matrix3x3& m3x3) const
		{
			m3x3.mat[0][0] = mat[0][0];
			m3x3.mat[0][1] = mat[0][1];
			m3x3.mat[0][2] = mat[0][2];
			m3x3.mat[1][0] = mat[1][0];
			m3x3.mat[1][1] = mat[1][1];
			m3x3.mat[1][2] = mat[1][2];
			m3x3.mat[2][0] = mat[2][0];
			m3x3.mat[2][1] = mat[2][1];
			m3x3.mat[2][2] = mat[2][2];
		}

		void MakeAffine(const Vector3& scale, const Quaternion& orientation, const Vector3& position);

		void MakeAffine(const Vector3& scale, const Vector3& orientation, const Vector3& position);

		static Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& orientation, const Vector3& position);

		void MakeInverseTransform(const Vector3& scale, const Quaternion& orientation, const Vector3& position);

		bool IsAffine(void) const
		{
			return mat[0][3] == 0 && mat[1][3] == 0 && mat[2][3] == 0 && mat[3][3] == 1;
		}

		Matrix4x4 InverseAffine() const;

		Vector3 TransformAffine(const Vector3& v) const
		{
			assert(IsAffine());

			return Vector3(
				mat[0][0] * v.x + mat[1][0] * v.y + mat[2][0] * v.z + mat[3][0],
				mat[0][1] * v.x + mat[1][1] * v.y + mat[2][1] * v.z + mat[3][1],
				mat[0][2] * v.x + mat[1][2] * v.y + mat[2][2] * v.z + mat[3][2]);
		}

		Vector4 TransformAffine(const Vector4& v) const
		{
			assert(IsAffine());
			return Vector4(
				v.x * mat[0][0] + v.y * mat[1][0] + v.z * mat[2][0] + v.w * mat[3][0],
				v.x * mat[0][1] + v.y * mat[1][1] + v.z * mat[2][1] + v.w * mat[3][1],
				v.x * mat[0][2] + v.y * mat[1][2] + v.z * mat[2][2] + v.w * mat[3][2],
				v.w
			);
		}

		friend Vector3 operator*(const Vector3& v, const Matrix4x4& m)
		{
			return {
				m.mat[0][0] * v.x + m.mat[1][0] * v.y + m.mat[2][0] * v.z,
				m.mat[0][1] * v.x + m.mat[1][1] * v.y + m.mat[2][1] * v.z,
				m.mat[0][2] * v.x + m.mat[1][2] * v.y + m.mat[2][2] * v.z
			};

		}

		Matrix4x4 Inverse() const
		{
			float m00 = mat[0][0], m01 = mat[0][1], m02 = mat[0][2], m03 = mat[0][3];
			float m10 = mat[1][0], m11 = mat[1][1], m12 = mat[1][2], m13 = mat[1][3];
			float m20 = mat[2][0], m21 = mat[2][1], m22 = mat[2][2], m23 = mat[2][3];
			float m30 = mat[3][0], m31 = mat[3][1], m32 = mat[3][2], m33 = mat[3][3];

			float v0 = m20 * m31 - m21 * m30;
			float v1 = m20 * m32 - m22 * m30;
			float v2 = m20 * m33 - m23 * m30;
			float v3 = m21 * m32 - m22 * m31;
			float v4 = m21 * m33 - m23 * m31;
			float v5 = m22 * m33 - m23 * m32;

			float t00 = +(v5 * m11 - v4 * m12 + v3 * m13);
			float t10 = -(v5 * m10 - v2 * m12 + v1 * m13);
			float t20 = +(v4 * m10 - v2 * m11 + v0 * m13);
			float t30 = -(v3 * m10 - v1 * m11 + v0 * m12);

			float invDet = 1 / (t00 * m00 + t10 * m01 + t20 * m02 + t30 * m03);

			float d00 = t00 * invDet;
			float d10 = t10 * invDet;
			float d20 = t20 * invDet;
			float d30 = t30 * invDet;

			float d01 = -(v5 * m01 - v4 * m02 + v3 * m03) * invDet;
			float d11 = +(v5 * m00 - v2 * m02 + v1 * m03) * invDet;
			float d21 = -(v4 * m00 - v2 * m01 + v0 * m03) * invDet;
			float d31 = +(v3 * m00 - v1 * m01 + v0 * m02) * invDet;

			v0 = m10 * m31 - m11 * m30;
			v1 = m10 * m32 - m12 * m30;
			v2 = m10 * m33 - m13 * m30;
			v3 = m11 * m32 - m12 * m31;
			v4 = m11 * m33 - m13 * m31;
			v5 = m12 * m33 - m13 * m32;

			float d02 = +(v5 * m01 - v4 * m02 + v3 * m03) * invDet;
			float d12 = -(v5 * m00 - v2 * m02 + v1 * m03) * invDet;
			float d22 = +(v4 * m00 - v2 * m01 + v0 * m03) * invDet;
			float d32 = -(v3 * m00 - v1 * m01 + v0 * m02) * invDet;

			v0 = m21 * m10 - m20 * m11;
			v1 = m22 * m10 - m20 * m12;
			v2 = m23 * m10 - m20 * m13;
			v3 = m22 * m11 - m21 * m12;
			v4 = m23 * m11 - m21 * m13;
			v5 = m23 * m12 - m22 * m13;

			float d03 = -(v5 * m01 - v4 * m02 + v3 * m03) * invDet;
			float d13 = +(v5 * m00 - v2 * m02 + v1 * m03) * invDet;
			float d23 = -(v4 * m00 - v2 * m01 + v0 * m03) * invDet;
			float d33 = +(v3 * m00 - v1 * m01 + v0 * m02) * invDet;

			return Matrix4x4(d00, d01, d02, d03, d10, d11, d12, d13, d20, d21, d22, d23, d30, d31, d32, d33);
		}

		const Vector3 GetX() const { return Vector3(mat[0]); }
		const Vector3 GetY() const { return Vector3(mat[1]); }
		const Vector3 GetZ() const { return Vector3(mat[2]); }
		const Vector3 GetW() const { return Vector3(mat[3]); }

		static Matrix4x4 MakeScale(const Vector3& scale)
		{
			Matrix4x4 m = Matrix4x4::IDENTITY;
			m.mat[0][0] = scale.x;
			m.mat[1][1] = scale.y;
			m.mat[2][2] = scale.z;
			return m;
		}

		Vector3 TransformCoord(const Vector3& v) const
		{
			Vector4 temp(v, 1.0f);
			Vector4 ret = (*this) * temp;

			if (fabsf(ret.w) < 1e-6f)
				return Vector3::ZERO;

			float invW = 1.0f / ret.w;
			return Vector3(ret.x * invW, ret.y * invW, ret.z * invW);
		}

		static const Matrix4x4 ZERO;
		static const Matrix4x4 ZEROAFFINE;
		static const Matrix4x4 IDENTITY;
	};

	Vector4 operator*(const Vector4& v, const Matrix4x4& mat);

	inline Vector3 Math::Transform(const Vector3& v, const Matrix4x4& m)
	{
		return v * m;
	}


	inline Vector3 Math::TransformNormal(const Vector3& v, const Matrix4x4& mat)
	{
		Matrix3x3 normalMat = mat.Inverse().Transpose().Get3x3();
		return (normalMat * v).NormalizedCopy();
	}

	inline Vector3 Math::TransformCoord(const Vector3& v, const Matrix4x4& mat)
	{
		Vector3 result{};
		result.x = v.x * mat.mat[0][0] + v.y * mat.mat[1][0] + v.z * mat.mat[2][0] + mat.mat[3][0];
		result.y = v.x * mat.mat[0][1] + v.y * mat.mat[1][1] + v.z * mat.mat[2][1] + mat.mat[3][1];
		result.z = v.x * mat.mat[0][2] + v.y * mat.mat[1][2] + v.z * mat.mat[2][2] + mat.mat[3][2];
		float w = v.x * mat.mat[0][3] + v.y * mat.mat[1][3] + v.z * mat.mat[2][3] + mat.mat[3][3];
		assert(w != 0);
		result /= w;

		return result;
	}

	inline Matrix4x4 Math::InverseTranspose(const Matrix4x4& mat)
	{
		const Vector3 x = mat.GetX();
		const Vector3 y = mat.GetY();
		const Vector3 z = mat.GetZ();

		const Vector3 inv0 = Cross(y, z);
		const Vector3 inv1 = Cross(z, x);
		const Vector3 inv2 = Cross(x, y);
		const float  rDet = 1.0f / (Dot(z, inv2));

		return Matrix4x4(inv0, inv1, inv2) * rDet;
	}

	inline Matrix4x4 Math::Multiply(const Matrix4x4& m1, const Matrix4x4& m2)
	{
		Matrix4x4 result;
		result.mat[0][0] = m1.mat[0][0] * m2.mat[0][0] + m1.mat[0][1] * m2.mat[1][0] + m1.mat[0][2] * m2.mat[2][0] + m1.mat[0][3] * m2.mat[3][0];
		result.mat[0][1] = m1.mat[0][0] * m2.mat[0][1] + m1.mat[0][1] * m2.mat[1][1] + m1.mat[0][2] * m2.mat[2][1] + m1.mat[0][3] * m2.mat[3][1];
		result.mat[0][2] = m1.mat[0][0] * m2.mat[0][2] + m1.mat[0][1] * m2.mat[1][2] + m1.mat[0][2] * m2.mat[2][2] + m1.mat[0][3] * m2.mat[3][2];
		result.mat[0][3] = m1.mat[0][0] * m2.mat[0][3] + m1.mat[0][1] * m2.mat[1][3] + m1.mat[0][2] * m2.mat[2][3] + m1.mat[0][3] * m2.mat[3][3];

		result.mat[1][0] = m1.mat[1][0] * m2.mat[0][0] + m1.mat[1][1] * m2.mat[1][0] + m1.mat[1][2] * m2.mat[2][0] + m1.mat[1][3] * m2.mat[3][0];
		result.mat[1][1] = m1.mat[1][0] * m2.mat[0][1] + m1.mat[1][1] * m2.mat[1][1] + m1.mat[1][2] * m2.mat[2][1] + m1.mat[1][3] * m2.mat[3][1];
		result.mat[1][2] = m1.mat[1][0] * m2.mat[0][2] + m1.mat[1][1] * m2.mat[1][2] + m1.mat[1][2] * m2.mat[2][2] + m1.mat[1][3] * m2.mat[3][2];
		result.mat[1][3] = m1.mat[1][0] * m2.mat[0][3] + m1.mat[1][1] * m2.mat[1][3] + m1.mat[1][2] * m2.mat[2][3] + m1.mat[1][3] * m2.mat[3][3];

		result.mat[2][0] = m1.mat[2][0] * m2.mat[0][0] + m1.mat[2][1] * m2.mat[1][0] + m1.mat[2][2] * m2.mat[2][0] + m1.mat[2][3] * m2.mat[3][0];
		result.mat[2][1] = m1.mat[2][0] * m2.mat[0][1] + m1.mat[2][1] * m2.mat[1][1] + m1.mat[2][2] * m2.mat[2][1] + m1.mat[2][3] * m2.mat[3][1];
		result.mat[2][2] = m1.mat[2][0] * m2.mat[0][2] + m1.mat[2][1] * m2.mat[1][2] + m1.mat[2][2] * m2.mat[2][2] + m1.mat[2][3] * m2.mat[3][2];
		result.mat[2][3] = m1.mat[2][0] * m2.mat[0][3] + m1.mat[2][1] * m2.mat[1][3] + m1.mat[2][2] * m2.mat[2][3] + m1.mat[2][3] * m2.mat[3][3];

		result.mat[3][0] = m1.mat[3][0] * m2.mat[0][0] + m1.mat[3][1] * m2.mat[1][0] + m1.mat[3][2] * m2.mat[2][0] + m1.mat[3][3] * m2.mat[3][0];
		result.mat[3][1] = m1.mat[3][0] * m2.mat[0][1] + m1.mat[3][1] * m2.mat[1][1] + m1.mat[3][2] * m2.mat[2][1] + m1.mat[3][3] * m2.mat[3][1];
		result.mat[3][2] = m1.mat[3][0] * m2.mat[0][2] + m1.mat[3][1] * m2.mat[1][2] + m1.mat[3][2] * m2.mat[2][2] + m1.mat[3][3] * m2.mat[3][2];
		result.mat[3][3] = m1.mat[3][0] * m2.mat[0][3] + m1.mat[3][1] * m2.mat[1][3] + m1.mat[3][2] * m2.mat[2][3] + m1.mat[3][3] * m2.mat[3][3];
		return result;
	}
};


