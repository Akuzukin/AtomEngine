#pragma once

namespace AtomEngine
{
	class ColorBuffer;
	class CommandContext;
	class Camera;
	class Matrix4x4;
	class MotionBlur
	{
	public:
		static void Initialize();
		static void Shutdown();

		static void GenerateCameraVelocityBuffer(CommandContext& Context, const Camera& camera, bool UseLinearZ = true);
		static void GenerateCameraVelocityBuffer(CommandContext& Context, const Matrix4x4& reprojectionMatrix, float nearClip, float farClip, bool UseLinearZ = true);
		static void RenderCameraBlur(CommandContext& Context, const Camera& camera, bool UseLinearZ = true);
		static void RenderCameraBlur(CommandContext& Context, const Matrix4x4& reprojectionMatrix, float nearClip, float farClip, bool UseLinearZ = true);

		static void RenderObjectBlur(CommandContext& Context, ColorBuffer& velocityBuffer);
		
		static void SedtEnableMotionBlur(bool Enable);
	};
}


