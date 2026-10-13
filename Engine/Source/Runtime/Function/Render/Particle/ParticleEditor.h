#pragma once
#include "Runtime/Function/Render/Particle/ParticleCommon.h"
#include "Runtime/Function/Render/Particle/ParticleSystem.h"
#include <string>
#include <vector>
#include "Runtime/Platform/DirectX12/Core/GraphicsCommon.h"

namespace AtomEngine
{
	class ParticleEditor
	{
	public:
		static ParticleEditor& Get();

		// 必要なら初期化処理を追加
		void Initialize() {}

		// ImGui フレーム内で呼ぶ
		void Render();

		// 保存/読み込み（ファイルパスは utf8 受け取り。内部で std::wstring に変換）
		bool SaveToFile(const std::string& utf8Path);
		bool LoadFromFile(const std::string& utf8Path);

		// 現在編集中のプロパティ取得（必要なら）
		ParticleProperty& GetCurrentProperty() { return mCurrent; }

	private:
		ParticleEditor();
		~ParticleEditor() = default;

		// JSON入出力ヘルパ
		std::string ToJson() const;
		bool FromJson(const std::string& json);

		ParticleProperty mCurrent;

		// デバッグ用に GPU から読み取ったパーティクル（UI 表示）
		std::vector<ParticleVertex> mDebugParticles;
		BlendMode mBlendMode = BlendMode::kBlendAlphaAdd;

		// UI ヘルパ
		void DrawEmitterUI(EmitterProperty& e);
		void DrawColor4(const char* label, Vector4& v);

		// gui state
		char mPathBuf[260];
		bool mOpen = true;
	};
}
