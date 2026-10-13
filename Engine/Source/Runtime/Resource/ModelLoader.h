#pragma once
#include "Model.h"
#include "Animation.h"
#include "Material.h"
#include "Skeleton.h"

#include <map>
#include <optional>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace AtomEngine
{
    struct Node
    {
        std::string name;

        Transform transform;
        Matrix4x4 globalTransform;
        std::vector<uint32_t> meshIndices;

        std::vector<std::unique_ptr<Node>> children;
        Node* parent = nullptr;
    };

    struct ModelData
    {
        AxisAlignedBox boundingBox;
        BoundingSphere boundingSphere;
        std::vector<Material> materials;
        std::vector<Mesh> meshes;

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        std::vector<AnimationClip> animations;
        std::map<std::string, JointWeightData> skinClusterData;
        std::unique_ptr<Node> rootNode = nullptr;
        
        aiNode* armatureNode = nullptr;
        uint32_t numJoints = 0;
        Skeleton skeleton;
        
        std::vector<GraphNode> graphNodes;
    };

    class ModelLoader
    {
    public:

        static bool LoadModel(ModelData& model, const std::string& filePath);

    private:
        static std::unique_ptr<Node> ProcessNode(ModelData& model, aiNode* node, const aiScene* scene, Node* parent);
        static Mesh ProcessMesh(ModelData& model, aiMesh* mesh);
        static void ProcessMaterial(ModelData& model, aiMaterial* mat);
        static void ProcessAnimations(ModelData& model, const aiScene* scene);
        static Skeleton ProcessSkeleton(ModelData& model,const aiNode* rootNode);
        static int32_t CreateJoint(
            ModelData& model,
            const aiNode* node,
            const std::optional<int32_t>& parent,
            std::vector<Joint>& joints);

        static void ComputeBoundingVolumes(ModelData& model);

    };
}
