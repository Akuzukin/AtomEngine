#include "TextureRef.h"
#include "AssetManager.h"

#include "Runtime/Platform/DirectX12/Core/DirectX12Core.h"
#include "Runtime/Platform/DirectX12/Context/CommandContext.h"
#include "DirectXTex.h"

#include <map>
#include <mutex>

namespace AtomEngine
{

	ManagedTexture::ManagedTexture(const std::wstring& FileName)
		: mMapKey(FileName), mIsValid(false), mIsLoading(true)
	{
		mCpuDescriptorHandle.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
	}

	void ManagedTexture::CreateFromMemory(std::shared_ptr<std::vector<byte>> ba, eDefaultTexture fallback, bool forceSRGB)
	{
		mIsLoading = false; // Loading is serialized by AssetManager. Every failure is terminal.
		if (!ba || ba->empty())
		{
			mCpuDescriptorHandle = GetDefaultTexture(fallback);
			mIsLoading = false;
			return;
		}

		mCpuDescriptorHandle = DX12Core::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		DirectX::TexMetadata metadata = {};
		DirectX::ScratchImage scratch = {};

		HRESULT hr = DirectX::LoadFromDDSMemory(ba->data(), ba->size(), DirectX::DDS_FLAGS_NONE, &metadata, scratch);
		if (FAILED(hr))
		{
			DX12Core::gDevice->CopyDescriptorsSimple(1, mCpuDescriptorHandle,
				GetDefaultTexture(fallback), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			return;
		}

		if (forceSRGB) metadata.format = DirectX::MakeSRGB(metadata.format);

		hr = DirectX::CreateTexture(DX12Core::gDevice.Get(), metadata, mResource.GetAddressOf());
		if (FAILED(hr))
		{
			DX12Core::gDevice->CopyDescriptorsSimple(1, mCpuDescriptorHandle,
				GetDefaultTexture(fallback), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			return;
		}

		std::vector<D3D12_SUBRESOURCE_DATA> subresources;
		hr = DirectX::PrepareUpload(DX12Core::gDevice.Get(),
			scratch.GetImages(), scratch.GetImageCount(), metadata, subresources);
		if (FAILED(hr)) return;

		GpuResource gpuTex(mResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST);
		CommandContext::InitializeTexture(gpuTex, (UINT)subresources.size(), subresources.data());

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = metadata.format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_UNKNOWN;

		switch (metadata.dimension)
		{
		case DirectX::TEX_DIMENSION_TEXTURE1D:
			if (metadata.arraySize > 1)
			{
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
				srvDesc.Texture1DArray.MipLevels = (UINT)metadata.mipLevels;
			}
			else
			{
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
				srvDesc.Texture1D.MipLevels = (UINT)metadata.mipLevels;
			}
			break;

		case DirectX::TEX_DIMENSION_TEXTURE2D:
			if (metadata.IsCubemap())
			{
				if (metadata.arraySize > 6)
				{
					srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
					srvDesc.TextureCubeArray.MipLevels = (UINT)metadata.mipLevels;
				}
				else
				{
					srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
					srvDesc.TextureCube.MipLevels = (UINT)metadata.mipLevels;
				}
			}
			else
			{
				if (metadata.arraySize > 1)
				{
					srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
					srvDesc.Texture2DArray.MipLevels = (UINT)metadata.mipLevels;
				}
				else
				{
					srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
					srvDesc.Texture2D.MipLevels = (UINT)metadata.mipLevels;
				}
			}
			break;

		case DirectX::TEX_DIMENSION_TEXTURE3D:
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
			srvDesc.Texture3D.MipLevels = (UINT)metadata.mipLevels;
			break;
		}

		DX12Core::gDevice->CreateShaderResourceView(mResource.Get(), &srvDesc, mCpuDescriptorHandle);

		mUsageState = D3D12_RESOURCE_STATE_GENERIC_READ;
		mIsValid = true;
		mWidth = (uint32_t)metadata.width;
		mHeight = (uint32_t)metadata.height;
		mDepth = (uint32_t)metadata.depth;
		mIsLoading = false;
	}


	void ManagedTexture::Unload()
	{
		AssetManager::DestroyTexture(mMapKey);
	}

	const void ManagedTexture::CopyGPU(ID3D12DescriptorHeap* heap, DescriptorHandle handle) const
	{
		DX12Core::gDevice->CopyDescriptorsSimple(1, handle, GetSRV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	TextureRef::TextureRef(const TextureRef&) = default;
	TextureRef::TextureRef(std::shared_ptr<ManagedTexture> tex) : mRef(std::move(tex)) {}
	TextureRef::~TextureRef() = default;
	TextureRef& TextureRef::operator=(const TextureRef&) = default;
	TextureRef& TextureRef::operator=(TextureRef&&) noexcept = default;

	bool TextureRef::IsValid() const
	{
		return mRef && mRef->IsValid();
	}

	const Texture* TextureRef::Get(void) const
	{
		return mRef.get();
	}

	const Texture* TextureRef::operator->(void) const
	{
		ASSERT(mRef != nullptr);
		return mRef.get();
	}

	D3D12_CPU_DESCRIPTOR_HANDLE TextureRef::GetSRV() const
	{
		if (mRef != nullptr)
			return mRef->GetSRV();
		else
			return GetDefaultTexture(kMagenta2D);
	}

}
