#include "ModelLoader.h"
#include "Runtime/Core/Utility/Utility.h"
#include <functional>
#include <map>
#include "AssetArchive.h"
#include <assimp/IOSystem.hpp>
#include <assimp/IOStream.hpp>

namespace AtomEngine
{
    class ArchiveModelStream final : public Assimp::IOStream
    {
        std::shared_ptr<std::vector<uint8_t>> bytes;
        size_t position = 0;
    public:
        explicit ArchiveModelStream(std::shared_ptr<std::vector<uint8_t>> data) : bytes(std::move(data)) {}
        size_t Read(void* output, size_t size, size_t count) override
        {
            if (!size) return 0;
            auto records=std::min(count,(bytes->size()-position)/size); auto length=records*size;
            if (length) memcpy(output,bytes->data()+position,length); position+=length; return records;
        }
        size_t Write(const void*,size_t,size_t) override { return 0; }
        aiReturn Seek(size_t offset, aiOrigin origin) override
        {
            auto base = origin == aiOrigin_CUR ? position : origin == aiOrigin_END ? bytes->size() : size_t(0);
            auto delta = static_cast<int64_t>(offset); size_t next;
            if (delta < 0) { auto distance=uint64_t(-(delta+1))+1; if (distance > base) return aiReturn_FAILURE; next=base-size_t(distance); }
            else { if (uint64_t(delta) > bytes->size()-base) return aiReturn_FAILURE; next=base+size_t(delta); }
            position=next; return aiReturn_SUCCESS;
        }
        size_t Tell() const override { return position; }
        size_t FileSize() const override { return bytes->size(); }
        void Flush() override {}
    };
    class ArchiveModelIO final : public Assimp::IOSystem
    {
    public:
        bool Exists(const char* path) const override { return AssetFileExists(UTF8ToWString(path)); }
        char getOsSeparator() const override { return '/'; }
        Assimp::IOStream* Open(const char* path, const char* mode = "rb") override
        {
            if (!path || !mode || mode[0] != 'r' || !Exists(path)) return nullptr;
            return new ArchiveModelStream(ReadAssetBytes(UTF8ToWString(path)));
        }
        void Close(Assimp::IOStream* stream) override { delete stream; }
    };

	struct TextureMapping
	{
		aiTextureType aiType;
		TextureSlot slot;
	};

	static const TextureMapping textureMap[] =
	{
		{ aiTextureType_BASE_COLOR,					TextureSlot::kBaseColor			},
		{ aiTextureType_DIFFUSE,					TextureSlot::kBaseColor			},
		{ aiTextureType_NORMALS,					TextureSlot::kNormal			},
		{ aiTextureType_HEIGHT,						TextureSlot::kNormal			},
		{ aiTextureType_LIGHTMAP,					TextureSlot::kOcclusion			},
		{ aiTextureType_AMBIENT_OCCLUSION,			TextureSlot::kOcclusion			},
		{ aiTextureType_SPECULAR, TextureSlot::kSpecular },
		{ aiTextureType_EMISSIVE,					TextureSlot::kEmissive			}
	};

	bool IsTransparentMaterial(const Material& mat)
	{
		if (mat.baseColorFactor.w < 1.0f)
			return true;

		if (mat.hasAlphaBlend)
			return true;

		return false;
	}

	bool SetTextures(Material& material, aiMaterial* mat, aiTextureType type, TextureSlot slot)
	{
		if (material.textureMask & slot)
			return false;

		aiString texPath;
		if (mat->GetTexture(type, 0, &texPath) == AI_SUCCESS && texPath.length > 0)
		{
			MaterialTexture texData;
			texData.name = UTF8ToWString(texPath.C_Str());
			texData.slot = slot;

			material.textures.push_back(std::move(texData));
			material.textureMask |= slot;
			return true;
		}
		return false;
	}

	inline static Transform ConvertTransform(const aiMatrix4x4& matrix)
	{
		aiVector3D scale, translate;
		aiQuaternion rotate;
		matrix.Decompose(scale, rotate, translate);

		Transform transform;
		transform.scale = Vector3(scale.x, scale.y, scale.z);
		transform.rotation = Quaternion(rotate.x, rotate.y, rotate.z, rotate.w);
		transform.transition = Vector3(translate.x, translate.y, translate.z);

		return transform;
	}

	bool ModelLoader::LoadModel(ModelData& model, const std::string& filePath)
	{
		uint32_t flag =
			aiProcess_MakeLeftHanded |
			aiProcess_FlipUVs |
			aiProcess_FlipWindingOrder |
			aiProcess_Triangulate |
			aiProcess_RemoveRedundantMaterials |
			aiProcess_CalcTangentSpace |
			aiProcess_GenSmoothNormals |
			aiProcess_JoinIdenticalVertices |
			aiProcess_PopulateArmatureData |
			aiProcess_GenUVCoords | 
			aiProcess_OptimizeGraph | 
			aiProcess_OptimizeMeshes;

		Assimp::Importer importer;
		if (GameArchiveMounted()) importer.SetIOHandler(new ArchiveModelIO());
		//いらない情報を削除
		importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS,
			aiComponent_COLORS | aiComponent_LIGHTS | aiComponent_CAMERAS);
		importer.SetPropertyInteger(AI_CONFIG_PP_SLM_TRIANGLE_LIMIT, INT_MAX);
		importer.SetPropertyInteger(AI_CONFIG_PP_SLM_VERTEX_LIMIT, 0xfffe);
		importer.SetPropertyFloat(AI_CONFIG_PP_CT_MAX_SMOOTHING_ANGLE, 45.0f);

		const aiScene* scene = importer.ReadFile(filePath, flag);
		if (!scene || !scene->mRootNode || !scene->HasMeshes()) throw std::runtime_error(std::string("Model import failed: ") + importer.GetErrorString());
		ASSERT(scene && scene->mRootNode && scene->HasMeshes(), "Load Model Failed to scene");
		//マテリアルをプロセス
		for (uint32_t i = 0; i < scene->mNumMaterials; i++)
			ProcessMaterial(model, scene->mMaterials[i]);
		//ノードをプロセス
		model.vertices.clear();
		model.indices.clear();
		model.rootNode = ProcessNode(model, scene->mRootNode, scene, nullptr);
		if (model.armatureNode)
			model.skeleton = ProcessSkeleton(model, model.armatureNode);

		//アニメーションをプロセス
		if (scene->HasAnimations())
			ProcessAnimations(model, scene);
		//バウンディングボックスを計算
		ComputeBoundingVolumes(model);

		return true;
	}

	std::unique_ptr<Node> ModelLoader::ProcessNode(ModelData& model, aiNode* node, const aiScene* scene, Node* parent)
	{
		auto outNode = std::make_unique<Node>();
		outNode->name = node->mName.C_Str();
		outNode->parent = parent;
		outNode->transform = ConvertTransform(node->mTransformation);

		auto localTransform = outNode->transform.GetMatrix();
		if (parent)
			outNode->globalTransform = localTransform * parent->globalTransform;
		else
			outNode->globalTransform = localTransform;

		GraphNode gnode{};
		gnode.xform = localTransform;
		gnode.rotation = outNode->transform.rotation;
		gnode.scale = outNode->transform.scale;
		gnode.matrixIdx = static_cast<uint32_t>(model.graphNodes.size());
		gnode.hasChildren = (node->mNumChildren > 0);
		gnode.hasSibling = (parent && parent->children.size() > 0);
		gnode.staleMatrix = true;
		gnode.skeletonRoot = scene->HasSkeletons() ? (node->mParent ? false : true) : false;
		model.graphNodes.push_back(gnode);

		for (uint32_t i = 0; i < node->mNumMeshes; i++)
		{
			uint32_t meshIndex = static_cast<uint32_t>(model.meshes.size());
			aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

			Mesh newMesh = ProcessMesh(model, mesh);
			for (auto& sub : newMesh.subMeshes)
			{
				sub.meshCbvIndex = gnode.matrixIdx;
			}
			model.meshes.push_back(std::move(newMesh));
			outNode->meshIndices.push_back(meshIndex);
		}

		for (uint32_t i = 0; i < node->mNumChildren; i++)
		{
			auto childNode = ProcessNode(model, node->mChildren[i], scene, outNode.get());
			if (childNode)
				outNode->children.push_back(std::move(childNode));
		}

		return outNode;
	}

	std::map<std::string, uint16_t> mJointNameToIndex;
	Mesh ModelLoader::ProcessMesh(ModelData& model, aiMesh* mesh)
	{
		Mesh outMesh;
		outMesh.vertexCount = mesh->mNumVertices;
		outMesh.indexCount = mesh->mNumFaces * 3;

		uint32_t vertexBase = static_cast<uint32_t>(model.vertices.size());
		uint32_t indexBase = static_cast<uint32_t>(model.indices.size());

		model.vertices.reserve(model.vertices.size() + outMesh.vertexCount);
		model.indices.reserve(model.indices.size() + outMesh.indexCount);

		AxisAlignedBox aabb;
		for (uint32_t i = 0; i < mesh->mNumVertices; i++)
		{
			Vertex v = {};
			v.position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };

			if (mesh->mTextureCoords[0])
				v.texcoord = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
			else
				v.texcoord = { 0.0f, 0.0f };

			if (mesh->mNormals)
				v.normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
			else
				v.normal = { 0.0f, 0.0f, 0.0f };

			if (mesh->mTangents && mesh->mBitangents)
			{
				v.tangent = { mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z };
				v.bitangent = { mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z };
			}
			else
			{
				v.tangent = { 0.0f, 0.0f, 0.0f };
				v.bitangent = { 0.0f, 0.0f, 0.0f };
			}

			aabb.AddPoint(v.position);
			model.vertices.push_back(v);
		}

		for (uint32_t i = 0; i < mesh->mNumFaces; i++)
		{
			aiFace face = mesh->mFaces[i];
			ASSERT(face.mNumIndices == 3);
			for (uint32_t j = 0; j < face.mNumIndices; j++)
				model.indices.push_back(static_cast<uint32_t>(face.mIndices[j]));
		}

		if (mesh->HasBones())
		{
			outMesh.numJoints = mesh->mNumBones;
			outMesh.psoFlags |= kHasSkin;

			for (uint32_t b = 0; b < mesh->mNumBones; ++b)
			{
				aiBone* bone = mesh->mBones[b];
				aiNode* node = bone->mNode;
				model.armatureNode = bone->mArmature;

				std::string jointName = node->mName.C_Str();
				JointWeightData& jwd = model.skinClusterData[jointName];
				// inverse bind pose
				aiMatrix4x4 m = bone->mOffsetMatrix;
				aiVector3D s, t; aiQuaternion r;
				m.Decompose(s, r, t);
				jwd.inverseBindPoseMatrix.MakeAffine(
					{ s.x, s.y, s.z }, { r.x, r.y, r.z, r.w }, { t.x, t.y, t.z });

				for (uint32_t w = 0; w < bone->mNumWeights; ++w)
				{
					uint32_t localId = bone->mWeights[w].mVertexId;
					float weight = bone->mWeights[w].mWeight;
					// ローカル頂点IDをグローバル頂点IDへ変換
					uint32_t globalId = vertexBase + localId;
					jwd.vertexWeights.push_back({ weight, globalId });
				}
			}
		}

		outMesh.boundingBox = aabb;

		if (mesh->mMaterialIndex >= 0 && mesh->mMaterialIndex < model.materials.size())
		{
			const Material& mat = model.materials[mesh->mMaterialIndex];
			if (IsTransparentMaterial(mat))
				outMesh.psoFlags |= kAlphaBlend;
		}

		SubMesh sub{};
		sub.indexOffset = indexBase;
		sub.indexCount = outMesh.indexCount;
		sub.vertexOffset = vertexBase;
		sub.vertexCount = outMesh.vertexCount;
		sub.materialIndex = mesh->mMaterialIndex;

		AxisAlignedBox subBounds;
		for (uint32_t i = 0; i < sub.indexCount; ++i)
		{
			uint32_t index = model.indices[sub.indexOffset + i];
			subBounds.AddPoint(model.vertices[index].position);
		}
		sub.bounds = subBounds;
		outMesh.subMeshes.push_back(sub);
		outMesh.boundingBox.AddBoundingBox(subBounds);

		return outMesh;
	}

	void ModelLoader::ProcessMaterial(ModelData& model, aiMaterial* mat)
	{
		Material material = {};
		aiString name;
		if (AI_SUCCESS == mat->Get(AI_MATKEY_NAME, name))
			material.name = name.C_Str();

		aiColor4D baseColor;
		if (AI_SUCCESS == mat->Get(AI_MATKEY_COLOR_DIFFUSE, baseColor))
		{
			material.baseColorFactor[0] = baseColor.r;
			material.baseColorFactor[1] = baseColor.g;
			material.baseColorFactor[2] = baseColor.b;
			material.baseColorFactor[3] = baseColor.a;
		}

		float metallic = 0.0f, roughness = 0.5f;
		mat->Get(AI_MATKEY_METALLIC_FACTOR, metallic);
		mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);
		material.metallicFactor = metallic;
		material.roughnessFactor = roughness;

		aiColor3D emission = { 0,0,0 };
		if (AI_SUCCESS == mat->Get(AI_MATKEY_COLOR_EMISSIVE, emission))
		{
			material.emissiveFactor[0] = emission.r;
			material.emissiveFactor[1] = emission.g;
			material.emissiveFactor[2] = emission.b;
		}

		float opacity = 1.0f;
		if (AI_SUCCESS == mat->Get(AI_MATKEY_OPACITY, opacity) ||
			AI_SUCCESS == mat->Get(AI_MATKEY_COLOR_TRANSPARENT, opacity))
		{
			if (opacity < 1.0f)
				material.hasAlphaBlend = true;
		}

		material.textures.clear();
		for (auto& entry : textureMap)
			SetTextures(material, mat, entry.aiType, entry.slot);

		if (!SetTextures(material, mat, aiTextureType_GLTF_METALLIC_ROUGHNESS, TextureSlot::kMetallicRoughness))
		{
			SetTextures(material, mat, aiTextureType_METALNESS, TextureSlot::kMetallic);
			SetTextures(material, mat, aiTextureType_DIFFUSE_ROUGHNESS, TextureSlot::kRoughness);
		}

		model.materials.push_back(std::move(material));
	}

	void ModelLoader::ProcessAnimations(ModelData& model, const aiScene* scene)
	{
		for (uint32_t i = 0; i < scene->mNumAnimations; ++i)
		{
			const aiAnimation* anim = scene->mAnimations[i];
			AnimationClip clip{};
			clip.name = anim->mName.C_Str();
			double ticksPerSecond = anim->mTicksPerSecond != 0.0 ? anim->mTicksPerSecond : 25.0;
			clip.duration = static_cast<float>(anim->mDuration / ticksPerSecond);

			for (uint32_t c = 0; c < anim->mNumChannels; ++c)
			{
				const aiNodeAnim* channel = anim->mChannels[c];
				AnimationCurve curve{};
				auto it = model.skeleton.jointMap.find(channel->mNodeName.C_Str());
				if (it == model.skeleton.jointMap.end())
				{
					continue;
				}
				curve.targetJoint = it->second;

				for (uint32_t k = 0; k < channel->mNumPositionKeys; ++k)
				{
					auto& key = channel->mPositionKeys[k];
					curve.translation.push_back({ float(key.mTime / ticksPerSecond),
						{ key.mValue.x, key.mValue.y, key.mValue.z } });
				}

				for (uint32_t k = 0; k < channel->mNumRotationKeys; ++k)
				{
					auto& key = channel->mRotationKeys[k];
					curve.rotation.push_back({ float(key.mTime / ticksPerSecond),
						{ key.mValue.x, key.mValue.y, key.mValue.z, key.mValue.w } });
				}

				for (uint32_t k = 0; k < channel->mNumScalingKeys; ++k)
				{
					auto& key = channel->mScalingKeys[k];
					curve.scale.push_back({ float(key.mTime / ticksPerSecond),
						{ key.mValue.x, key.mValue.y, key.mValue.z } });
				}

				clip.curves.push_back(std::move(curve));
			}

			model.animations.push_back(std::move(clip));
		}
	}

	Skeleton ModelLoader::ProcessSkeleton(ModelData& model, const aiNode* rootNode)
	{
		Skeleton skeleton;

		// rootNode 自体がボーンでない場合もあるので、CreateJoint は該当ノードがボーンでなければ -1 を返します。
		int32_t rootIdx = CreateJoint(model, rootNode, std::nullopt, skeleton.joints);
		skeleton.rootJoint = rootIdx;

		for (int32_t i = 0; i < (int32_t)skeleton.joints.size(); ++i)
		{
			skeleton.jointMap[skeleton.joints[i].name] = i;
		}

		return skeleton;
	}

	int32_t ModelLoader::CreateJoint(
		ModelData& model,
		const aiNode* node,
		const std::optional<int32_t>& parentJointIndex,
		std::vector<Joint>& joints)
	{
		std::string nodeName = node->mName.C_Str();
		bool isBone = (model.skinClusterData.find(nodeName) != model.skinClusterData.end());

		int32_t myIndex = -1;

		if (isBone)
		{
			Joint joint;

			Transform nodeTrans = ConvertTransform(node->mTransformation);
			joint.name = nodeName;
			joint.index = static_cast<int32_t>(joints.size()); // push 前のサイズがこのジョイントのインデックス
			joint.parent = parentJointIndex;
			joint.localMatrix = nodeTrans.GetMatrix();
			joint.skeletonSpaceMatrix = Matrix4x4::IDENTITY;
			joint.transform = nodeTrans;

			joints.push_back(joint);
			myIndex = joint.index;
		}

		std::optional<int32_t> childParent = isBone ? std::optional<int32_t>(myIndex) : parentJointIndex;

		for (uint32_t i = 0; i < node->mNumChildren; ++i)
		{
			const aiNode* child = node->mChildren[i];
			int32_t childIndex = CreateJoint(model, child, childParent, joints);
			if (isBone && childIndex != -1)
			{
				joints[myIndex].children.push_back(childIndex);
			}
		}

		return myIndex;
	}

	float ComputeBoundingSphereRadius(const AxisAlignedBox& box, const std::vector<Vector3>& vertices)
	{
		Vector3 center = box.GetCenter();
		float radius = 0.0f;
		for (const auto& v : vertices)
		{
			float dist = (v - center).Length();
			radius = std::max(radius, dist);
		}
		return radius;
	}

	void ModelLoader::ComputeBoundingVolumes(ModelData& model)
	{
		std::function<AxisAlignedBox(const Node*, const Matrix4x4&)> computeBounds =
			[&](const Node* node, const Matrix4x4& parentTransform) -> AxisAlignedBox
			{
				Matrix4x4 localTransform = node->transform.GetMatrix() * parentTransform;
				AxisAlignedBox bounds;

				for (auto meshIdx : node->meshIndices)
				{
					const auto& mesh = model.meshes[meshIdx];
					AxisAlignedBox transformedBox = mesh.boundingBox;
					transformedBox.Transform(localTransform);
					bounds.AddBoundingBox(transformedBox);
				}

				for (auto& child : node->children)
					bounds.AddBoundingBox(computeBounds(child.get(), localTransform));

				return bounds;
			};

		if (model.rootNode)
		{
			model.boundingBox = computeBounds(model.rootNode.get(), Matrix4x4::IDENTITY);
			std::vector<Vector3> allVertices;
			for (auto& v : model.vertices)
				allVertices.push_back(v.position);
			float radius = ComputeBoundingSphereRadius(model.boundingBox, allVertices);

			model.boundingSphere = BoundingSphere(model.boundingBox.GetCenter(), radius);
		}
	}
}
