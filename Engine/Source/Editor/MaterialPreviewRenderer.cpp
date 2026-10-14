#include "MaterialPreviewRenderer.h"
#include "Runtime/Resource/Primitive.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/ModelRenderResources.h"
#include "Runtime/Platform/DirectX12/Core/DirectX12Core.h"
#include "Runtime/Platform/DirectX12/Context/GraphicsContext.h"
#include "Runtime/Platform/DirectX12/Shader/ShaderCompiler.h"
#include "Runtime/Platform/DirectX12/Shader/ConstantBufferStructures.h"
namespace AtomEngine
{
	MaterialPreviewRenderer::~MaterialPreviewRenderer()
	{
		auto& heap = Renderer::GetTextureHeap();
		for (auto h : { mDisplay, mTextures, mEnvironment })
			if (!h.IsNull())
				heap.Free(h);
	}
	void MaterialPreviewRenderer::Initialize()
	{
		mColor.SetClearColor(Color(.045f, .055f, .075f, 1));
		mColor.Create(L"Material sphere preview", 256, 256, 1, DXGI_FORMAT_R10G10B10A2_UNORM);
		mDepth.Create(L"Material preview depth", 256, 256, DXGI_FORMAT_D32_FLOAT);
		mSphere = Primitive::GetModel(BuiltinMesh::Sphere);
		auto& heap = Renderer::GetTextureHeap();
		mDisplay = heap.Alloc();
		mEnvironment = heap.Alloc(3);
		DX12Core::gDevice->CopyDescriptorsSimple(1, mDisplay, mColor.GetSRV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		const wchar_t* paths[] = { L"Asset/Textures/Environment/NightSkyHDRI004_4K/NightSkyHDRI004_4KSpecularHDR.dds",
								  L"Asset/Textures/Environment/NightSkyHDRI004_4K/NightSkyHDRI004_4KDiffuseHDR.dds",
								  L"Asset/Textures/Environment/NightSkyHDRI004_4K/NightSkyHDRI004_4KBrdf.dds" };
		for (int i = 0; i < 3; ++i)
		{
			mEnvironmentRefs.push_back(AssetManager::LoadTextureFile(paths[i]));
			DX12Core::gDevice->CopyDescriptorsSimple(1, mEnvironment + int(i * heap.GetDescriptorSize()),
				mEnvironmentRefs.back().GetSRV(),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}
		auto vs = ShaderCompiler::CompileBlob(L"Object3D.VS.hlsl", L"vs_6_2");
		auto ps = ShaderCompiler::CompileBlob(L"MaterialPreview.PS.hlsl", L"ps_6_2");
		const D3D12_INPUT_ELEMENT_DESC input[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"BITTANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0} };
		mPSO.SetRootSignature(Renderer::GetRootSignature());
		mPSO.SetInputLayout(5, input);
		mPSO.SetVertexShader(vs.Get());
		mPSO.SetPixelShader(ps.Get());
		mPSO.SetRasterizerState(RasterizerDefaultCw);
		mPSO.SetBlendState(BlendDisable);
		mPSO.SetDepthStencilState(DepthStateReadWrite);
		mPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		auto format = mColor.GetFormat();
		mPSO.SetRenderTargetFormats(1, &format, mDepth.GetFormat());
		mPSO.Finalize();
		mMRPSO = mPSO;
		auto mr = ShaderCompiler::CompileBlob(L"MaterialPreviewMR.PS.hlsl", L"ps_6_2");
		mMRPSO.SetPixelShader(mr.Get());
		mMRPSO.Finalize();
	}
	void MaterialPreviewRenderer::Render(const MaterialInstance& material, float yaw, float pitch,
		std::shared_ptr<Model> source, int slot)
	{
		if (material.overrideModelTextures)
			source = nullptr;
		auto& heap = Renderer::GetTextureHeap();
		std::string signature = std::to_string(uintptr_t(source.get())) + ":" + std::to_string(slot);
		for (auto& path : material.texturePaths)
		{
			signature += path;
			signature += '\0';
		}
		if (mTextures.IsNull() || signature != mTextureSignature)
		{
			auto textures = heap.Alloc(7);
			std::vector<TextureRef> refs;
			for (size_t i = 0; i < 7; ++i)
			{
				auto srv = GetDefaultTexture(i == 3 ? kDefaultNormalMap : kWhiteOpaque2D);
				if (source)
					srv = source->renderResources->MaterialTextureSRV(*source, uint32_t(slot), i);
				if (!material.texturePaths[i].empty())
				{
					refs.push_back(AssetManager::LoadCovertTexture(
						material.texturePaths[i], i == 3 ? kDefaultNormalMap : kWhiteOpaque2D, i == 0 || i == 5));
					srv = refs.back().GetSRV();
				}
				DX12Core::gDevice->CopyDescriptorsSimple(1, textures + int(i * heap.GetDescriptorSize()), srv,
					D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			}
			if (!mTextures.IsNull())
				heap.Free(mTextures);
			mTextures = textures;
			mTextureRefs = std::move(refs);
			mTextureSignature = std::move(signature);
			mSource = source;
		}
		Camera camera;
		camera.SetPerspectiveMatrix(Math::PIDiv4, 1, .03f, 100.f);
		camera.SetLookAt(Vector3(3.5f * sinf(yaw) * cosf(pitch), 3.5f * sinf(pitch), 3.5f * cosf(yaw) * cosf(pitch)),
			Vector3::ZERO, Vector3::UP);
		camera.Update();
		MeshConstants mesh{};
		mesh.worldMatrix = Matrix4x4::IDENTITY;
		mesh.worldInverseTranspose = Matrix4x4::IDENTITY;
		MaterialConstants mat{};
		mat.uvTransform = material.uvTransform.GetMatrix();
		mat.baseColorFactor = material.mBaseColor.ToVector4();
		mat.emissiveFactor = material.mEmissive;
		mat.metallicFactor = material.mMetallic;
		mat.roughnessFactor = material.mRoughness;
		mat.normalTextureScale = material.mNormalScale;
		mat.FresnelF0 = material.mFresnelF0;
		mat.aoFactor = material.mAO;
		mat.uselight = material.useLight;
		mat.textureOverrides = (material.overrideModelTextures ? 127 : 0) | (material.packedTextureChannels << 7);
		for (size_t i = 0; i < 7; ++i)
			if (!material.texturePaths[i].empty())
				mat.textureOverrides |= 1u << i;
		GlobalConstants globals{};
		globals.ViewProjMatrix = camera.GetViewProjMatrix();
		globals.CameraPos = camera.GetPosition();
		globals.SunShadowMatrix = Matrix4x4::IDENTITY;
		globals.SunDirection = Vector3(-.6f, -1, -.4f).NormalizedCopy();
		globals.SunIntensity = Vector3(3, 3, 3);
		globals.IBLFactor = .6f;
		globals.IBLRange =
			float(const_cast<ID3D12Resource*>(mEnvironmentRefs[0].Get()->GetResource())->GetDesc().MipLevels - 1);
		auto& context = GraphicsContext::Begin(L"Material preview PBR");
		context.TransitionResource(mColor, D3D12_RESOURCE_STATE_RENDER_TARGET);
		context.TransitionResource(mDepth, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
		context.ClearColor(mColor);
		context.ClearDepth(mDepth);
		context.SetRenderTarget(mColor.GetRTV(), mDepth.GetDSV());
		context.SetViewportAndScissor(0, 0, 256, 256);
		context.SetRootSignature(Renderer::GetRootSignature());
		context.SetPipelineState(source && (source->mMaterials.at(slot).textureMask & kMetallicRoughness) ? mMRPSO : mPSO);
		context.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, heap.GetHeapPointer());
		context.SetDynamicConstantBufferView(kMeshConstants, sizeof(mesh), &mesh);
		context.SetDynamicConstantBufferView(kMaterialConstants, sizeof(mat), &mat);
		context.SetDynamicConstantBufferView(kCommonCBV, sizeof(globals), &globals);
		context.SetDescriptorTable(kMaterialSRVs, mTextures);
		context.SetDescriptorTable(kCommonSRVs, mEnvironment);
		context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		auto& resources = *mSphere->renderResources;
		context.SetVertexBuffer(0, resources.vertexBuffer.VertexBufferView());
		context.SetIndexBuffer(resources.indexBuffer.IndexBufferView());
		context.DrawIndexed(uint32_t(mSphere->mMeshData[0].subMeshes[0].indexCount));
		context.TransitionResource(mColor, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		context.Finish();
	}
} // namespace AtomEngine
