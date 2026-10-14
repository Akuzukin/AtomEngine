#pragma once
#include "Runtime/Function/Scene/SceneView.h"
#include "Runtime/Function/Render/DebugDraw.h"
namespace AtomEngine
{
	class CameraGizmos
	{
	public:
		static Frustum BuildFrustum(const CameraComponent& settings, const Matrix4x4& world, float widthOverHeight)
		{
			Transform pose(world.GetTrans(), world.GetRotation(), Vector3::UNIT_SCALE);
			return pose *
				Frustum(settings.fieldOfView * Math::PI / 180.f, widthOverHeight, settings.nearClip, settings.farClip);
		}
		static void Draw(World& world, const Camera& view, Entity selected = entt::null)
		{
			const auto& projection = view.GetProjMatrix();
			float aspect = projection[1][1] / projection[0][0];
			for (auto entity : world.View<CameraComponent, TransformComponent>())
			{
				const auto& settings = world.GetComponent<CameraComponent>(entity);
				auto matrix = world.GetComponent<TransformComponent>(entity).GetMatrix();
				auto color = entity == selected ? Color::Yellow
					: settings.enabled ? Color::Cyan
					: Color(.45f, .45f, .45f, 1);
				auto frustum = BuildFrustum(settings, matrix, aspect);
				frustum.DebugDraw(view.GetViewProjMatrix(), color);
				// A compact camera marker remains readable when the far clip is very distant.
				auto position = matrix.GetTrans();
				auto rotation = matrix.GetRotation();
				auto point = [&](Vector3 local) { return position + rotation * local; };
				float height = tanf(settings.fieldOfView * Math::PI / 360.f) * .6f, width = height * aspect;
				Vector3 corners[] = { point(Vector3(-width, -height, .6f)), point(Vector3(-width, height, .6f)),
									 point(Vector3(width, height, .6f)), point(Vector3(width, -height, .6f)) };
				for (int i = 0; i < 4; ++i)
				{
					DebugDraw::DrawLine(position, corners[i], color, view.GetViewProjMatrix());
					DebugDraw::DrawLine(corners[i], corners[(i + 1) % 4], color, view.GetViewProjMatrix());
				}
				DebugDraw::DrawTriangle(corners[1], corners[2], point(Vector3(0, height + .25f, .6f)), color,
					view.GetViewProjMatrix());
				DebugDraw::DrawLine(position, point(Vector3(0, 0, 1.2f)), color, view.GetViewProjMatrix());
			}
		}
	};
} // namespace AtomEngine
