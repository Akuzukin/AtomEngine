#pragma once
#include "DescriptorAllocator.h"

namespace AtomEngine
{

	class ContextManager;
	class CommandListManager;

	namespace DX12Core
	{
		enum eResolution { k720p, k900p, k1080p, k1440p, k1800p, k2160p };

		extern Microsoft::WRL::ComPtr<ID3D12Device> gDevice;
		extern CommandListManager gCommandManager;
		extern ContextManager gContextManager;

		extern bool gTypedUAVLoadSupport_R11G11B10_FLOAT;
		extern bool gTypedUAVLoadSupport_R16G16B16A16_FLOAT;
		extern bool gEnableHDROutput;

		extern uint32_t gBackBufferWidth;
		extern uint32_t gBackBufferHeight;
		extern DXGI_FORMAT gBackBufferFormat;

		void InitializeDx12(bool RequireDXRSupport = false);
		void ShutdownDx12();
		uint32_t GetFrameIndexMod2();

		void OnResize(uint32_t width, uint32_t height);
		void Present();

		extern DescriptorAllocator gDescriptorAllocator[];
		inline D3D12_CPU_DESCRIPTOR_HANDLE AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT Count = 1)
		{
			return gDescriptorAllocator[Type].Allocate(Count);
		}
	}
}
