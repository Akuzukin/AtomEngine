#pragma once
#include "Runtime/Platform/DirectX12/Buffer/BufferManager.h"
#include "Runtime/Platform/DirectX12/Pipeline/PipelineState.h"
#include "Runtime/Platform/DirectX12/Context/GraphicsContext.h"
#include "Runtime/Core/Math/Matrix4x4.h"

namespace AtomEngine
{

	struct DebugDrawVertex
	{
		Vector3 pos;
		Vector4 color;
	};

	class DebugDraw
	{
	public:
		static void Initialize();
		static void Shutdown();

		/// <summary>
		/// ライン描画
		/// </summary>
		/// <param name="a">ポイントA</param>
		/// <param name="b">ポイントB</param>
		/// <param name="color">色</param>
		/// <param name="viewProj">カメラ行列</param>
		static void DrawLine(const Vector3& a, const Vector3& b, const Color& color,
			const Matrix4x4& viewProj);

		/// <summary>
		/// ボックス描画
		/// </summary>
		/// <param name="center">中心座標</param>
		/// <param name="size">サイズ</param>
		/// <param name="color">色</param>
		/// <param name="viewProj">カメラ行列</param>
		static void DrawCube(const Vector3& center, const Vector3& size,
			const Color& color, const Matrix4x4& viewProj);

		/// <summary>
		/// 球の描画
		/// </summary>
		/// <param name="center">中心座標</param>
		/// <param name="radius">半径</param>
		/// <param name="color">色</param>
		/// <param name="viewProj">カメラ行列</param>
		/// <param name="slices">分割数(x)</param>
		/// <param name="stacks">分割数(y)</param>
		static void DrawSphere(const Vector3& center, float radius, const Color& color,
			const Matrix4x4& viewProj, uint32_t slices = 16, uint32_t stacks = 16);

		/// <summary>
		/// 三角形描画
		/// </summary>
		/// <param name="a">ポイントA</param>
		/// <param name="b">ポイントB</param>
		/// <param name="c">ポイントC</param>
		/// <param name="color">色</param>
		/// <param name="viewProj">カメラ行列</param>
		static void DrawTriangle(const Vector3& a, const Vector3& b, const Vector3& c, const Color& color,
			const Matrix4x4& viewProj);

		/// <summary>
		/// 描画
		/// </summary>
		static void Render(GraphicsContext& ctx);

	private:
		static void AddLineInternal(const Vector3& a, const Vector3& b, const Vector4& color);

		static RootSignature sRootSig;
		static GraphicsPSO sPSO;

		static Matrix4x4 sViewProj;

		static std::vector<DebugDrawVertex> sVertices;
	};
}

