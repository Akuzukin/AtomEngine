#include "GpuBuffer.h"
#include "../Context/CommandContext.h"
#include "../Core/DirectX12Core.h"

namespace AtomEngine
{
	using namespace DX12Core;

	void GpuBuffer::Create(const std::wstring& name, uint32_t NumElements, uint32_t ElementSize, const void* initialData)
	{
		Destroy();

		mElementCount = NumElements;
		mElementSize = ElementSize;
		mBufferSize = NumElements * ElementSize;

		D3D12_RESOURCE_DESC ResourceDesc = DescribeBuffer();

		mUsageState = D3D12_RESOURCE_STATE_COMMON;

		D3D12_HEAP_PROPERTIES HeapProps;
		HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
		HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		HeapProps.CreationNodeMask = 1;
		HeapProps.VisibleNodeMask = 1;

		ThrowIfFailed(gDevice->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE,
			&ResourceDesc, mUsageState, nullptr, IID_PPV_ARGS(&mResource)));

		mGpuVirtualAddress = mResource->GetGPUVirtualAddress();

		if (initialData)
			CommandContext::InitializeBuffer(*this, initialData, mBufferSize);

#ifndef _DEBUG
		(name);
#else
		mResource->SetName(name.c_str());
#endif

		CreateDerivedViews();
	}

	void GpuBuffer::Create(const std::wstring& name, uint32_t NumElements, uint32_t ElementSize, const UploadBuffer& srcData, uint32_t srcOffset)
	{
		Destroy();

		mElementCount = NumElements;
		mElementSize = ElementSize;
		mBufferSize = NumElements * ElementSize;

		D3D12_RESOURCE_DESC ResourceDesc = DescribeBuffer();

		mUsageState = D3D12_RESOURCE_STATE_COMMON;

		D3D12_HEAP_PROPERTIES HeapProps;
		HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
		HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		HeapProps.CreationNodeMask = 1;
		HeapProps.VisibleNodeMask = 1;

		ThrowIfFailed(
			gDevice->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE,
				&ResourceDesc, mUsageState, nullptr, IID_PPV_ARGS(&mResource)));

		mGpuVirtualAddress = mResource->GetGPUVirtualAddress();

		CommandContext::InitializeBuffer(*this, srcData, srcOffset);

#ifdef _DEBUG
		mResource->SetName(name.c_str());
#else
		(name);
#endif

		CreateDerivedViews();
	}

	// 事前に割り当てられたヒープからバッファをサブ割り当てします。
	// 初期データが指定されている場合は、デフォルトのコマンドコンテキストを使用してバッファにコピーされます。
	void GpuBuffer::CreatePlaced(const std::wstring& name, ID3D12Heap* pBackingHeap, uint32_t HeapOffset, uint32_t NumElements, uint32_t ElementSize,
		const void* initialData)
	{
		mElementCount = NumElements;
		mElementSize = ElementSize;
		mBufferSize = NumElements * ElementSize;

		D3D12_RESOURCE_DESC ResourceDesc = DescribeBuffer();

		mUsageState = D3D12_RESOURCE_STATE_COMMON;

		ThrowIfFailed(gDevice->CreatePlacedResource(pBackingHeap, HeapOffset, &ResourceDesc, mUsageState, nullptr, IID_PPV_ARGS(&mResource)));

		mGpuVirtualAddress = mResource->GetGPUVirtualAddress();

		if (initialData)
			CommandContext::InitializeBuffer(*this, initialData, mBufferSize);

#ifdef _DEBUG
		mResource->SetName(name.c_str());
#else
		(name);
#endif

		CreateDerivedViews();

	}

	D3D12_CPU_DESCRIPTOR_HANDLE GpuBuffer::CreateConstantBufferView(uint32_t Offset, uint32_t Size) const
	{
		ASSERT(Offset + Size <= mBufferSize);

		Size = ((Size + 15) & ~15);

		D3D12_CONSTANT_BUFFER_VIEW_DESC CBVDesc;
		CBVDesc.BufferLocation = mGpuVirtualAddress + (size_t)Offset;
		CBVDesc.SizeInBytes = Size;

		D3D12_CPU_DESCRIPTOR_HANDLE hCBV = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		gDevice->CreateConstantBufferView(&CBVDesc, hCBV);
		return hCBV;
	}

	D3D12_RESOURCE_DESC GpuBuffer::DescribeBuffer(void)
	{
		ASSERT(mBufferSize != 0);

		D3D12_RESOURCE_DESC Desc = {};
		Desc.Alignment = 0;
		Desc.DepthOrArraySize = 1;
		Desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		Desc.Flags = mResourceFlags;
		Desc.Format = DXGI_FORMAT_UNKNOWN;
		Desc.Height = 1;
		Desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		Desc.MipLevels = 1;
		Desc.SampleDesc.Count = 1;
		Desc.SampleDesc.Quality = 0;
		Desc.Width = (UINT64)mBufferSize;
		return Desc;
	}

	void ByteAddressBuffer::CreateDerivedViews(void)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		SRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		SRVDesc.Buffer.NumElements = (UINT)mBufferSize / 4;
		SRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

		if (mSRV.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
			mSRV = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		gDevice->CreateShaderResourceView(mResource.Get(), &SRVDesc, mSRV);

		D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
		UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		UAVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		UAVDesc.Buffer.NumElements = (UINT)mBufferSize / 4;
		UAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

		if (mUAV.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
			mUAV = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		gDevice->CreateUnorderedAccessView(mResource.Get(), nullptr, &UAVDesc, mUAV);
	}

	void StructuredBuffer::CreateDerivedViews(void)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
		SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		SRVDesc.Buffer.NumElements = mElementCount;
		SRVDesc.Buffer.StructureByteStride = mElementSize;
		SRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		if (mSRV.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
			mSRV = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		gDevice->CreateShaderResourceView(mResource.Get(), &SRVDesc, mSRV);

		D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
		UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
		UAVDesc.Buffer.CounterOffsetInBytes = 0;
		UAVDesc.Buffer.NumElements = mElementCount;
		UAVDesc.Buffer.StructureByteStride = mElementSize;
		UAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		mCounterBuffer.Create(L"StructuredBuffer::Counter", 1, 4);

		if (mUAV.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
			mUAV = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		gDevice->CreateUnorderedAccessView(mResource.Get(), mCounterBuffer.GetResource(), &UAVDesc, mUAV);
	}

	void TypedBuffer::CreateDerivedViews(void)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		SRVDesc.Format = mDataFormat;
		SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		SRVDesc.Buffer.NumElements = mElementCount;
		SRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		if (mSRV.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
			mSRV = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		gDevice->CreateShaderResourceView(mResource.Get(), &SRVDesc, mSRV);

		D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
		UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		UAVDesc.Format = mDataFormat;
		UAVDesc.Buffer.NumElements = mElementCount;
		UAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		if (mUAV.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
			mUAV = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		gDevice->CreateUnorderedAccessView(mResource.Get(), nullptr, &UAVDesc, mUAV);
	}

	const D3D12_CPU_DESCRIPTOR_HANDLE& StructuredBuffer::GetCounterSRV(CommandContext& Context)
	{
		Context.TransitionResource(mCounterBuffer, D3D12_RESOURCE_STATE_GENERIC_READ);
		return mCounterBuffer.GetSRV();
	}

	const D3D12_CPU_DESCRIPTOR_HANDLE& StructuredBuffer::GetCounterUAV(CommandContext& Context)
	{
		Context.TransitionResource(mCounterBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		return mCounterBuffer.GetUAV();
	}

}
