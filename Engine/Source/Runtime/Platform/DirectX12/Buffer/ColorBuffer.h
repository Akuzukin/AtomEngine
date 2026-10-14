#pragma once
#include "PixelBuffer.h"
#include "GpuBuffer.h"
#include "Runtime/Core/Color/Color.h"

namespace AtomEngine
{

    class ColorBuffer : public PixelBuffer
    {
    public:
        ColorBuffer(Color ClearColor = Color(0.0f, 0.0f, 0.0f, 0.0f))
            : mClearColor(ClearColor), mNumMipMaps(0), mFragmentCount(1), mSampleCount(1)
        {
            mRTVHandle.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
            mSRVHandle.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
            for (int i = 0; i < _countof(mUAVHandle); ++i)
                mUAVHandle[i].ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
        }

        // スワップチェーンバッファからカラーバッファを作成します。順序なしのアクセスは制限されています。
        void CreateFromSwapChain(const std::wstring& Name, ID3D12Resource* BaseResource);

        // カラーバッファを作成
        void Create(const std::wstring& Name, uint32_t Width, uint32_t Height, uint32_t NumMips,
            DXGI_FORMAT Format, D3D12_GPU_VIRTUAL_ADDRESS VidMemPtr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN);

        // カラーバッファを作成
        void CreateArray(const std::wstring& Name, uint32_t Width, uint32_t Height, uint32_t ArrayCount,
            DXGI_FORMAT Format, D3D12_GPU_VIRTUAL_ADDRESS VidMemPtr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN);
        // 六面 cubemap，支持逐 mip UAV 写入，用于环境光照预过滤。
        void CreateCube(const std::wstring& Name, uint32_t Size, uint32_t NumMips, DXGI_FORMAT Format);

        // CPUからスワップチェインのリソースを参照するためのハンドル
        const D3D12_CPU_DESCRIPTOR_HANDLE& GetSRV(void) const { return mSRVHandle; }
        const D3D12_CPU_DESCRIPTOR_HANDLE& GetRTV(void) const { return mRTVHandle; }
        const D3D12_CPU_DESCRIPTOR_HANDLE& GetUAV(uint32_t mip = 0) const { ASSERT(mip <= mNumMipMaps); return mUAVHandle[mip]; }

        void SetClearColor(Color ClearColor) { mClearColor = ClearColor; }

        void SetMsaaMode(uint32_t NumColorSamples, uint32_t NumCoverageSamples)
        {
            ASSERT(NumCoverageSamples >= NumColorSamples);
            mFragmentCount = NumColorSamples;
            mSampleCount = NumCoverageSamples;
        }

        Color GetClearColor(void) const { return mClearColor; }

        //void GenerateMipMaps(CommandContext& Context);

    protected:

        D3D12_RESOURCE_FLAGS CombineResourceFlags(void) const
        {
            D3D12_RESOURCE_FLAGS Flags = D3D12_RESOURCE_FLAG_NONE;

            if (Flags == D3D12_RESOURCE_FLAG_NONE && mFragmentCount == 1)
                Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

            return D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | Flags;
        }

        // 1x1 に縮小するために必要なテクスチャレベル数を計算します。
        // _BitScanReverse を使用して、最も高いセットビットを見つけます。各次元は
        // 半分に縮小され、ビットは切り捨てられます。 256 (0x100) には 9 つのミップレベルがあり、これは 511 (0x1FF) と同じです。
        static inline uint32_t ComputeNumMips(uint32_t Width, uint32_t Height)
        {
            uint32_t HighBit;
            _BitScanReverse((unsigned long*)&HighBit, Width | Height);
            return HighBit + 1;
        }

        void CreateDerivedViews(ID3D12Device* Device, DXGI_FORMAT Format, uint32_t ArraySize, uint32_t NumMips = 1);

        Color mClearColor;
        D3D12_CPU_DESCRIPTOR_HANDLE mSRVHandle;
        D3D12_CPU_DESCRIPTOR_HANDLE mRTVHandle;
        D3D12_CPU_DESCRIPTOR_HANDLE mUAVHandle[12];
        uint32_t mNumMipMaps; // number of texture sublevels
        uint32_t mFragmentCount;
        uint32_t mSampleCount;
    };
}
