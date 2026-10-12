#pragma once
#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <DirectXMath.h>
#undef max
#undef min

namespace AtomEngine
{
	using namespace DirectX;

	class Radian;
	class Angle;
	class Degree;

	class Vector2;
	class Vector3;
	class Vector4;
	class Matrix3x3;
	class Matrix4x4;
	class Quaternion;

	class Radian
	{
		float rad;

	public:
		explicit Radian(float r = 0) : rad(r) {}
		explicit Radian(const Degree& d);
		Radian& operator=(float f)
		{
			rad = f;
			return *this;
		}
		Radian& operator=(const Degree& d);
		operator float() const { return rad; }

		float GetRadians() const { return rad; }
		float GetDegrees() const;

		void SetValue(float f) { rad = f; }

		float* operator&() const { return (float*)&rad; }

		const Radian& operator+() const { return *this; }
		Radian        operator+(const Radian& r) const { return Radian(rad + r.rad); }
		Radian        operator+(const Degree& d) const;
		Radian& operator+=(const Radian& r)
		{
			rad += r.rad;
			return *this;
		}
		Radian& operator+=(const Degree& d);
		Radian  operator-() const { return Radian(-rad); }
		Radian  operator-(const Radian& r) const { return Radian(rad - r.rad); }
		Radian  operator-(const Degree& d) const;
		Radian& operator-=(const Radian& r)
		{
			rad -= r.rad;
			return *this;
		}
		Radian& operator-=(const Degree& d);
		Radian  operator*(float f) const { return Radian(rad * f); }
		Radian  operator*(const Radian& f) const { return Radian(rad * f.rad); }
		Radian& operator*=(float f)
		{
			rad *= f;
			return *this;
		}
		Radian  operator/(float f) const { return Radian(rad / f); }
		Radian& operator/=(float f)
		{
			rad /= f;
			return *this;
		}

		bool operator<(const Radian& r) const { return rad < r.rad; }
		bool operator<=(const Radian& r) const { return rad <= r.rad; }
		bool operator==(const Radian& r) const { return rad == r.rad; }
		bool operator!=(const Radian& r) const { return rad != r.rad; }
		bool operator>=(const Radian& r) const { return rad >= r.rad; }
		bool operator>(const Radian& r) const { return rad > r.rad; }
	};

	class Degree
	{
		float deg;

	public:
		explicit Degree(float d = 0) : deg(d) {}
		explicit Degree(const Radian& r) : deg(r.GetDegrees()) {}
		Degree& operator=(float f)
		{
			deg = f;
			return *this;
		}
		Degree& operator=(const Degree& d) = default;
		Degree& operator=(const Radian& r)
		{
			deg = r.GetDegrees();
			return *this;
		}
		operator float() const { return deg; }

		float GetDegrees() const { return deg; }
		float GetRadians() const;

		const Degree& operator+() const { return *this; }
		Degree        operator+(const Degree& d) const { return Degree(deg + d.deg); }
		Degree        operator+(const Radian& r) const { return Degree(deg + r.GetDegrees()); }
		Degree& operator+=(const Degree& d)
		{
			deg += d.deg;
			return *this;
		}
		Degree& operator+=(const Radian& r)
		{
			deg += r.GetDegrees();
			return *this;
		}
		Degree  operator-() const { return Degree(-deg); }
		Degree  operator-(const Degree& d) const { return Degree(deg - d.deg); }
		Degree  operator-(const Radian& r) const { return Degree(deg - r.GetDegrees()); }
		Degree& operator-=(const Degree& d)
		{
			deg -= d.deg;
			return *this;
		}
		Degree& operator-=(const Radian& r)
		{
			deg -= r.GetDegrees();
			return *this;
		}
		Degree  operator*(float f) const { return Degree(deg * f); }
		Degree  operator*(const Degree& f) const { return Degree(deg * f.deg); }
		Degree& operator*=(float f)
		{
			deg *= f;
			return *this;
		}
		Degree  operator/(float f) const { return Degree(deg / f); }
		Degree& operator/=(float f)
		{
			deg /= f;
			return *this;
		}

		bool operator<(const Degree& d) const { return deg < d.deg; }
		bool operator<=(const Degree& d) const { return deg <= d.deg; }
		bool operator==(const Degree& d) const { return deg == d.deg; }
		bool operator!=(const Degree& d) const { return deg != d.deg; }
		bool operator>=(const Degree& d) const { return deg >= d.deg; }
		bool operator>(const Degree& d) const { return deg > d.deg; }
	};

	class Math
	{
	public:

		static float Abs(float value) { return std::fabs(value); }
		static bool  IsNaN(float f) { return std::isnan(f); }
		static float Sqr(float value) { return value * value; }
		static float Sqrt(float fValue) { return std::sqrt(fValue); }
		static float InvSqrt(float value) { return 1.f / Sqrt(value); }
		static bool  Equal(float a, float b, float tolerance = std::numeric_limits<float>::epsilon());
		static float Clamp(float v, float min, float max) { return std::clamp(v, min, max); }
		static Vector3 Floor(const Vector3& v);

		static float DegreesToRadians(float degrees);
		static float RadiansToDegrees(float radians);
		static float  sin(const Radian& rad) { return std::sin(rad.GetRadians()); }
		static float  sin(float value) { return std::sin(value); }
		static float  cos(const Radian& rad) { return std::cos(rad.GetRadians()); }
		static float  cos(float value) { return std::cos(value); }
		static float  tan(const Radian& rad) { return std::tan(rad.GetRadians()); }
		static float  tan(float value) { return std::tan(value); }
		static float  Pow(float a, float b) { return std::pow(a, b); }
		static Radian acos(float value);
		static Radian asin(float value);
		static Radian atan(float value) { return Radian(std::atan(value)); }
		static Radian atan2(float y_v, float x_v) { return Radian(std::atan2(y_v, x_v)); }
		static Vector3 Normalize(const Vector3& v);

		static float Dot(const Vector3& lhs, const Vector3& rhs);
		static Vector3 Cross(const Vector3& lhs, const Vector3& rhs);

		static Vector3 Max(const Vector3& a, const Vector3& b);

		static Vector3 Min(const Vector3& a, const Vector3& b);

		static Vector3 Select(const Vector3& a, const Vector3& b, const Vector3& control);

		static Vector3 Select(const Vector3& a, const Vector3& b, bool control);

		static float Distance(const Vector3& a, const Vector3& b);

		static Vector3 Transform(const Vector3& v, const Matrix4x4& mat);

		static Vector3 TransformNormal(const Vector3& v, const Matrix4x4& mat);

		static Vector3 TransformCoord(const Vector3& v, const Matrix4x4& mat);

		static Matrix3x3 InverseTranspose(const Matrix3x3& mat);

		static Matrix4x4 InverseTranspose(const Matrix4x4& mat);

		static Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2);

		static Matrix4x4
			MakeViewMatrix(const Vector3& position, const Quaternion& orientation, const Matrix4x4* reflectMatrix = nullptr);

		static Matrix4x4
			MakeLookAtMatrix(const Vector3& eyePosition, const Vector3& targetPosition, const Vector3& upDir);

		static Matrix4x4 MakePerspectiveMatrix(Radian fovy, float aspect, float znear = 0.1f, float zfar = 100.0f);

		static Matrix4x4
			MakeOrthographicProjectionMatrix(float left, float right, float bottom, float top, float nearClip, float farClip);

		template <typename T>
		static T Lerp(const T& lhs, const T& rhs, float alpha)
		{
			return lhs + alpha * (rhs - lhs);
		}

		template <typename T>
		static T Clamp(const T& value, const T& min, const T& max)
		{
			return std::clamp(value, min, max);
		}

		template <typename T>
		static T SmoothStep(const T& lhs, const T& rhs, float alpha)
		{
			float t = Clamp((alpha - lhs) / (rhs - lhs), 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		}
		
		template <typename T>
		static T Max(const T& a, const T& b)
		{
			return std::max(a, b);
		}

		template <typename T>
        static T Min(const T& a, const T& b)
        {
            return std::min(a, b);
        }

	public:
		static constexpr float Infinity = std::numeric_limits<float>::infinity();
		static constexpr float NegInfinity = -std::numeric_limits<float>::infinity();
		static constexpr float PI = 3.14159265358979323846264338327950288f;
		static constexpr float OneOverPI = 1.0f / PI;
		static constexpr float TwoPI = 2.0f * PI;
		static constexpr float HalfPI = 0.5f * PI;
		static constexpr float PIDiv4 = PI / 4.0f;
		static constexpr float deg2Rad = PI / 180.0f;
		static constexpr float rad2Deg = 180.0f / PI;
		static constexpr float Epsilon = 1e-6f;
	};

	float ToFloat(const int8_t x);
	float ToFloat(const uint8_t x);
	float ToFloat(const int16_t x);
	float ToFloat(const uint16_t x);

	inline void Lerp3(float* Dest, const float* Key1, const float* Key2, float T)
	{
		Dest[0] = Math::Lerp(Key1[0], Key2[0], T);
		Dest[1] = Math::Lerp(Key1[1], Key2[1], T);
		Dest[2] = Math::Lerp(Key1[2], Key2[2], T);
	}

	inline Radian::Radian(const Degree& d) : rad(d.GetRadians()) {}
	inline Radian& Radian::operator=(const Degree& d)
	{
		rad = d.GetRadians();
		return *this;
	}
	inline Radian Radian::operator+(const Degree& d) const { return Radian(rad + d.GetRadians()); }
	inline Radian& Radian::operator+=(const Degree& d)
	{
		rad += d.GetRadians();
		return *this;
	}
	inline Radian Radian::operator-(const Degree& d) const { return Radian(rad - d.GetRadians()); }
	inline Radian& Radian::operator-=(const Degree& d)
	{
		rad -= d.GetRadians();
		return *this;
	}

	inline float Radian::GetDegrees() const { return Math::RadiansToDegrees(rad); }

	inline float Degree::GetRadians() const { return Math::DegreesToRadians(deg); }

	inline Radian operator*(float a, const Radian& b) { return Radian(a * b.GetRadians()); }

	inline Radian operator/(float a, const Radian& b) { return Radian(a / b.GetRadians()); }

	inline Degree operator*(float a, const Degree& b) { return Degree(a * b.GetDegrees()); }

	inline Degree operator/(float a, const Degree& b) { return Degree(a / b.GetDegrees()); }


	template <typename T> __forceinline T AlignUpWithMask(T value, size_t mask)
	{
		return (T)(((size_t)value + mask) & ~mask);
	}
	template <typename T> __forceinline T AlignDownWithMask(T value, size_t mask)
	{
		return (T)((size_t)value & ~mask);
	}
	template <typename T> __forceinline T AlignUp(T value, size_t alignment)
	{
		return AlignUpWithMask(value, alignment - 1);
	}

	template <typename T> __forceinline T AlignDown(T value, size_t alignment)
	{
		return AlignDownWithMask(value, alignment - 1);
	}
	template <typename T> __forceinline bool IsAligned(T value, size_t alignment)
	{
		return 0 == ((size_t)value & (alignment - 1));
	}
	template <typename T> __forceinline T DivideByMultiple(T value, size_t alignment)
	{
		return (T)((value + alignment - 1) / alignment);
	}
}
