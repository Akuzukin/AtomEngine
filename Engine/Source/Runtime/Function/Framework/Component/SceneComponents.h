#pragma once
#include "Runtime/Core/Math/MathInclude.h"
#include <map>
#include <string>
#include <vector>
namespace AtomEngine
{
	enum class ComponentKind { Mesh, Collider, RigidBody, Script, UI, Audio, Camera, Light, Joint };
	struct ObjectStateComponent { bool activeSelf = true; uint32_t disabledComponents = 0; };
	struct CameraComponent
	{
		float fieldOfView = 45, nearClip = .1f, farClip = 1000;
		bool primary = true, enabled = true;
	};
	struct DirectionalLightComponent
	{
		Color color = Color::White;
		float intensity = 1;
		bool enabled = true, shadows = true, autoFitShadow = true;
	};
	struct PersistentIdComponent
	{
		std::string value;
	};
	struct HierarchyComponent
	{
		std::string parentId;
	};
	enum class BuiltinMesh
	{
		None,
		Plane,
		Cube,
		Sphere,
		Quad
	};
	struct MeshSourceComponent
	{
		BuiltinMesh primitive = BuiltinMesh::None;
		std::string asset;
		bool visible = true;
	};
	struct PrefabInstanceComponent
	{
		std::string source;
		std::string sourceId, instanceId, baseline;
		std::map<std::string, std::string> members;
	};
	enum class ColliderShape
	{
		Box,
		Sphere,
		Capsule,
		Mesh,
		ConvexHull
	};
	struct ColliderComponent
	{
		uint32_t collisionLayer = 0, collisionMask = 0xffffffffu;
		ColliderShape shape = ColliderShape::Box;
		Vector3 center = Vector3::ZERO;
		Quaternion rotation = Quaternion::IDENTITY;
		Vector3 halfExtents = Vector3(0.5f, 0.5f, 0.5f);
		float radius = 0.5f, halfHeight = 0.5f;
		bool trigger = false, visible = true;
	};
	enum class BodyMotion
	{
		Static,
		Kinematic,
		Dynamic
	};
	struct RigidBodyComponent
	{
		BodyMotion motion = BodyMotion::Dynamic;
		float mass = 1, friction = 0.5f, restitution = 0.1f;
		bool gravity = true;
		Vector3 linearVelocity = Vector3::ZERO;
	};
	enum class JointType
	{
		Fixed,
		Distance,
		Hinge
	};
	struct JointComponent
	{
		JointType type = JointType::Fixed;
		std::string connectedBody;
		Vector3 anchor = Vector3::ZERO, connectedAnchor = Vector3::ZERO, axis = Vector3::UP;
		float minDistance = 0, maxDistance = 1, minAngle = -180, maxAngle = 180;
		bool enabled = true;
	};
	struct SceneSettings
	{
		std::string environment;
		int skyMode = 0; // 0：程序化天空，1：纹理天空。
		Vector3 skyZenith{ .08f, .25f, .65f }, skyHorizon{ .65f, .8f, 1.f }, skyGround{ .12f, .14f, .18f };
		float skyIntensity = 1, skyRotation = 0, skySunSize = 1, skySunIntensity = 3;
		std::string diffuseIBL = "Asset/Textures/Environment/NightSkyHDRI004_4K/NightSkyHDRI004_4KDiffuseHDR.dds";
		std::string specularIBL = "Asset/Textures/Environment/NightSkyHDRI004_4K/NightSkyHDRI004_4KSpecularHDR.dds";
		std::string brdfLut = "Asset/Textures/Environment/NightSkyHDRI004_4K/NightSkyHDRI004_4KBrdf.dds";
		float iblIntensity = 1, iblBias = 0, exposure = 1;
		bool skyEnabled = true, fxaa = true, autoExposure = true;
		Vector3 editorPosition = Vector3(10, 10, -15);
		Quaternion editorRotation = Quaternion::IDENTITY;
		float editorFov = 45, editorNear = .1f, editorFar = 1000;
		bool hasEditorCamera = false;
	};
	inline SceneSettings ProjectSceneDefaults;
	struct AudioSourceComponent
	{
		std::string clip;
		float volume = 1;
		bool loop = false, playOnAwake = false;
	};
	struct ScriptAttachment
	{
		std::string assembly, type;
		bool enabled = true;
		std::map<std::string, std::string> fields;
	};
	struct ScriptComponent
	{
		std::vector<ScriptAttachment> scripts;
	};
	enum class UiLayout { None, Horizontal, Vertical };
	struct UiElementComponent
	{
		Vector2 position{ 20, 20 }, size{ 200, 40 };
		bool responsive = false, progressBar = false;
		Vector2 anchorMin{ 0,0 }, anchorMax{ 0,0 }, pivot{ 0,0 };
		UiLayout layout = UiLayout::None;
		float padding = 8, spacing = 8, progress = 1;
		int layoutOrder = 0;
		Color color = Color::White;
		std::string text = "Text", texture;
		float fontSize = 20;
		bool visible = true;
		bool interactable = false;
		int layer = 0;
		Color background = Color(.15f, .2f, .3f, .85f);
	};
} // namespace AtomEngine
