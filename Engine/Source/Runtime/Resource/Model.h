#pragma once
#include "Mesh.h"
#include "Animation.h"
#include "Material.h"
#include "Skeleton.h"


#include "../Core/Math/BoundingBox.h"
#include "../Core/Math/BoundingSphere.h"

#include <map>
#include <memory>

namespace AtomEngine
{
	class ModelRenderResources;

	struct JointXform
	{
		Matrix4x4 posXform;
		Matrix4x4 nrmXform;
	};

	struct GraphNode
	{
		Matrix4x4 xform;
		Quaternion rotation;
        Vector3 scale;

		uint32_t matrixIdx : 28;
		uint32_t hasSibling : 1;
		uint32_t hasChildren : 1;
		uint32_t staleMatrix : 1;
		uint32_t skeletonRoot : 1;
	};

	class Model
	{
	public:
        ~Model();
        std::shared_ptr<ModelRenderResources> renderResources;

		std::shared_ptr<const MeshGeometry> mGeometry;
		BoundingSphere mBoundingSphere;
		AxisAlignedBox mBoundingBox;

        std::vector<Material> mMaterials;
		std::vector<Mesh> mMeshData;
		std::vector<uint8_t> mKeyFrameData;
		std::vector<AnimationClip> mAnimationData;
		std::vector<Matrix4x4>mJointIBMs;
		std::vector<GraphNode> mSceneGraph;

		Skeleton mSkeleton;
		uint32_t mNumJoints = 0;

	};
}


