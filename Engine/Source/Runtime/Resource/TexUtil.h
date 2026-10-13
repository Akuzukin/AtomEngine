//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard
//

#pragma once

#include <cstdint>
#include <string>
#include <DirectXTex.h>
#include "../Platform/DirectX12/D3dUtility/d3dInclude.h"

namespace AtomEngine
{
    enum TexConversionFlags
    {
        kSRGB = 1,          // テクスチャはsRGBカラー
        kPreserveAlpha = 2, // アルファチャンネルがある
        kNormalMap = 4,     // テクスチャには法線が含まれています
        kBumpToNormal = 8,  // バンプマップから法線マップを生成する
        kDefaultBC = 16,    // 標準ブロック圧縮を適用する（BC1-5）
        kQualityBC = 32,    // 高品質ブロック圧縮を適用する（BC6H/7）
        kFlipVertical = 64, // テクスチャを上下反転する
    };

    inline uint8_t TextureOptions(bool sRGB, bool hasAlpha = false, bool invertY = false)
    {
        return (sRGB ? kSRGB : 0) | (hasAlpha ? kPreserveAlpha : 0) | (invertY ? kFlipVertical : 0);
    }

    // 指定されたテクスチャの DDS バージョンが存在しないか、ソーステクスチャよりも古い場合は、再変換します。
    void CompileTextureOnDemand(const std::wstring& originalFile, uint32_t flags);

    // TGA、PNG、JPGなどの非DDSテクスチャを読み込み、より最適な
    // ミップチェーン全体を含むDDS形式に変換します。結果のファイルは、ファイル拡張子が「DDS」に変更された同じパスになります。
    bool ConvertToDDS(
        const std::wstring& filePath,	// UTF8でエンコードされたソースファイルへのパス
        uint32_t Flags                  // フラグをOR演算して指定
    );

}
