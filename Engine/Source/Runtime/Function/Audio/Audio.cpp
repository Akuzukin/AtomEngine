#include "Audio.h"
#include <filesystem>
#include <Runtime/Core/Utility/Utility.h>

#pragma comment(lib,"xaudio2.lib")
#pragma comment(lib, "Mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "Mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

namespace AtomEngine
{
	std::unordered_map<uint32_t, Audio::SoundData> Audio::soundDatas;
	uint32_t Audio::soundHandles = 0;
	using namespace Microsoft::WRL;

	Audio* Audio::GetInstance()
	{
		static Audio instance;
		return &instance;
	}

	uint32_t Audio::Load(const std::string& filename)
	{
		std::filesystem::path path(filename);
		std::string ext = path.extension().string();

		SoundData soundData;
		//  wav
		if (ext == ".wav")
		{
			soundData = SoundLoadWave(filename);
		}
		//ほかの拡張名
		else
		{
			soundData = SoundLoadMediaFoundation(filename);
		}
		// ハンドルを返す
		uint32_t handle = soundHandles;

		for (auto& [index, data] : soundDatas)
		{
			if (soundDatas[index].filename == filename)
			{
				return index;
			}
		}

		soundDatas[handle] = std::move(soundData);

		soundHandles++;

		return handle;
	}

	void Audio::Play(uint32_t soundHandle, bool loop)
	{
		std::lock_guard<std::mutex> lock(audioMutex);

		auto it = soundDatas.find(soundHandle);
		if (it == soundDatas.end()) return;

		auto& soundData = it->second;

		if (soundData.isPaused && !soundData.isPlaying)
		{
			ThrowIfFailed(soundData.pSourceVoice->Start());
			soundData.isPlaying = true;
			soundData.isPaused = false;
			return;
		}

		if (!soundData.pSourceVoice)
		{
			ThrowIfFailed(xAudio2->CreateSourceVoice(
				&soundData.pSourceVoice, &soundData.wfex));
		}

		XAUDIO2_VOICE_STATE state;
		soundData.pSourceVoice->GetState(&state);
		if (state.BuffersQueued > 0)
		{
			soundData.pSourceVoice->Stop();
			soundData.pSourceVoice->FlushSourceBuffers();
		}

		XAUDIO2_BUFFER buffer = {};
		buffer.pAudioData = soundData.pBuffer.get();
		buffer.AudioBytes = soundData.bufferSize;
		buffer.Flags = XAUDIO2_END_OF_STREAM;
		buffer.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;

		ThrowIfFailed(soundData.pSourceVoice->SubmitSourceBuffer(&buffer));
		ThrowIfFailed(soundData.pSourceVoice->Start());

		soundData.isPlaying = true;
		soundData.isPaused = false;
	}

	bool Audio::IsPlaying(uint32_t soundHandle)
	{
		auto it = soundDatas.find(soundHandle);
		if (it != soundDatas.end())
		{
			auto& soundData = it->second;
			XAUDIO2_VOICE_STATE state;
			if (soundData.pSourceVoice)
			{
				soundData.pSourceVoice->GetState(&state);
				return state.BuffersQueued > 0;
			}
		}

		return false;
	}

	bool Audio::Pause(uint32_t soundHandle)
	{
		auto it = soundDatas.find(soundHandle);
		if (it != soundDatas.end())
		{
			auto& soundData = it->second;
			XAUDIO2_VOICE_STATE state;
			soundData.pSourceVoice->GetState(&state);
			if (state.BuffersQueued > 0 && soundData.isPlaying)
			{
				ThrowIfFailed(soundData.pSourceVoice->Stop());
				soundData.isPlaying = false;
				soundData.isPaused = true;
				return true;
			}
		}
		return false;
	}

	bool Audio::Stop(uint32_t soundHandle)
	{
		auto it = soundDatas.find(soundHandle);
		if (it != soundDatas.end())
		{
			auto& soundData = it->second;
			ThrowIfFailed(soundData.pSourceVoice->Stop());
			ThrowIfFailed(soundData.pSourceVoice->FlushSourceBuffers());
			soundData.isPlaying = false;
			soundData.isPaused = false;
			return true;
		}
		return false;
	}

	void Audio::SetVolume(uint32_t soundHandle, float volume)
	{
		std::lock_guard<std::mutex> lock(audioMutex);

		auto it = soundDatas.find(soundHandle);
		if (it != soundDatas.end() && it->second.pSourceVoice)
		{
			float clamped = std::clamp(volume, 0.0f, 1.0f);
			it->second.pSourceVoice->SetVolume(clamped);
		}
	}

	float Audio::GetVolume(uint32_t soundHandle)
	{
		auto it = soundDatas.find(soundHandle);
		if (it != soundDatas.end() && it->second.pSourceVoice)
		{
			float volume = 1.0f;
			it->second.pSourceVoice->GetVolume(&volume);
			return volume;
		}
		return 0.0f;
	}

	void Audio::Initialize()
	{
		// XAudioエンジンのインスタンスを生成
		ThrowIfFailed(XAudio2Create(xAudio2.GetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR));
		// マスターボイスの作成
		ThrowIfFailed(xAudio2->CreateMasteringVoice(&masterVoice));
	}

	void Audio::Finalize()
	{
		// 音声データを全て破棄
		for (auto& pair : soundDatas)
		{
			if (pair.second.pSourceVoice)
			{
				pair.second.pSourceVoice->DestroyVoice();
				pair.second.pSourceVoice = nullptr;
			}
		}
		soundDatas.clear();
		if (masterVoice) { masterVoice->DestroyVoice(); masterVoice = nullptr; }
		xAudio2.Reset();
	}

	Audio::SoundData Audio::SoundLoadWave(const std::string& filename)
	{
		//ファイルオープン
		std::ifstream file;
		file.open(filename, std::ios::binary);
		assert(file.is_open());

		//.wavデータ読み込み

		RiffHeader riff;
		//RIFFヘッダの読み込み
		file.read((char*)&riff, sizeof(riff));

		if (strncmp(riff.chunk.id, "RIFF", 4) != 0)
		{
			assert(0);
		}

		if (strncmp(riff.type, "WAVE", 4) != 0)
		{
			assert(0);
		}
		ChunkHeader chunk;
		bool foundFmt = false;
		while (file.read(reinterpret_cast<char*>(&chunk), sizeof(chunk)))
		{
			if (strncmp(chunk.id, "fmt ", 4) == 0)
			{
				foundFmt = true;
				break;
			}
			if (!file.seekg(chunk.size, std::ios::cur)) break;
		}

		if (!foundFmt)
		{
			Log("Missing fmt chunk");
		}

		std::vector<uint8_t> fmtData(chunk.size);
		file.read(reinterpret_cast<char*>(fmtData.data()), fmtData.size());
		WAVEFORMATEX wfex = *reinterpret_cast<WAVEFORMATEX*>(fmtData.data());

		ChunkHeader dataChunk{};
		int chunkCount = 0;
		while (file)
		{
			file.read(reinterpret_cast<char*>(&dataChunk), sizeof(dataChunk));
			if (file.gcount() != sizeof(dataChunk)) break;

			if (strncmp(dataChunk.id, "data", 4) == 0) break;

			if (!file.seekg(dataChunk.size, std::ios::cur)) break;

			if (++chunkCount > 100)
			{
				Log("Excessive non-data chunks");
			}
		}

		if (strncmp(dataChunk.id, "data", 4) != 0)
		{
			Log("Missing data chunk");
		}

		//読み込んだデータを返す
		std::unique_ptr<BYTE[]> buffer(new BYTE[dataChunk.size]);
		file.read(reinterpret_cast<char*>(buffer.get()), dataChunk.size);

		return { filename,wfex, std::move(buffer), dataChunk.size };
	}

	Audio::SoundData Audio::SoundLoadMediaFoundation(const std::string& filename)
	{
		Microsoft::WRL::ComPtr<IMFSourceReader> reader;

		ThrowIfFailed(MFCreateSourceReaderFromURL(std::wstring(filename.begin(), filename.end()).c_str(), nullptr, &reader));

		// PCMを指定
		Microsoft::WRL::ComPtr<IMFMediaType> mediaTypeOut;
		ThrowIfFailed(MFCreateMediaType(&mediaTypeOut));
		mediaTypeOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
		mediaTypeOut->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
		ThrowIfFailed(reader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, mediaTypeOut.Get()));

		// メディアタイプを取得
		Microsoft::WRL::ComPtr<IMFMediaType> actualMediaType;
		ThrowIfFailed(reader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &actualMediaType));

		// メディアタイプからWAVEFORMATEXを生成
		UINT32 size = 0;
		WAVEFORMATEX* wfex = nullptr;
		ThrowIfFailed(MFCreateWaveFormatExFromMFMediaType(actualMediaType.Get(), &wfex, &size));

		std::unique_ptr<BYTE[]> pcmData;
		UINT32 pcmSize = 0;

		std::vector<BYTE> data;
		while (true)
		{
			DWORD flags = 0;
			IMFSample* sample = nullptr;
			ComPtr<IMFMediaBuffer> buffer;
			ThrowIfFailed(reader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, nullptr, &sample));

			if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;
			if (!sample) continue;

			ThrowIfFailed(sample->ConvertToContiguousBuffer(&buffer));

			BYTE* pData = nullptr;
			DWORD cbData = 0;
			ThrowIfFailed(buffer->Lock(&pData, nullptr, &cbData));

			data.insert(data.end(), pData, pData + cbData);

			buffer->Unlock();
		}

		pcmSize = (UINT32)data.size();
		pcmData = std::make_unique<BYTE[]>(pcmSize);
		memcpy(pcmData.get(), data.data(), pcmSize);

		SoundData soundData;
		soundData.filename = filename;
		soundData.wfex = *wfex;
		soundData.pBuffer = std::move(pcmData);
		soundData.bufferSize = pcmSize;
		CoTaskMemFree(wfex);

		return soundData;
	}
}
