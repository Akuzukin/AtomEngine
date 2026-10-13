#pragma once
#include "Runtime/Core/Math/MathInclude.h"
#include "Runtime/Core/Math/BoundingBox.h"
#include "Skeleton.h"

namespace AtomEngine
{
	struct Vertex
	{
		Vector3 position;
		Vector2 texcoord;
		Vector3 normal;
		Vector3 tangent;
		Vector3 bitangent;
	};

	struct MeshGeometry { std::vector<Vertex> vertices; std::vector<uint32_t> indices; std::vector<JointVertex> skin; };

	enum PSOFlags : uint16_t
	{
		kHasSkin		= 1 << 0,
		kHasMRTexture	= 1 << 1,
		kAlphaBlend		= 1 << 2,
	};

	struct SubMesh
	{
		uint32_t indexOffset = 0;
		uint32_t indexCount = 0;
		uint32_t vertexOffset = 0;
		uint32_t vertexCount = 0;

		uint32_t meshCbvIndex = 0;
		uint32_t materialIndex = 0;
		uint32_t srvTableIndex = 0;
		AxisAlignedBox bounds;
	};

	struct Mesh
	{
		AxisAlignedBox boundingBox;

		uint32_t vertexCount = 0;
		uint32_t indexCount = 0;
		uint16_t srvTable = 0;
		uint16_t pso = 0;
		uint16_t psoFlags = 0;
		uint32_t numJoints = 0;
		uint16_t startJoint = 0;

		std::vector<SubMesh> subMeshes;
	};
}
