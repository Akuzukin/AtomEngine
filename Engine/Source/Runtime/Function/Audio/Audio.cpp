#include "Audio.h"
#include "Runtime/Core/Utility/Utility.h"
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <limits>
#include "Runtime/Resource/AssetArchive.h"
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
namespace AtomEngine {
std::unordered_map<uint32_t, Audio::SoundData> Audio::soundDatas;
uint32_t Audio::soundHandles = 0;
Audio *Audio::GetInstance() { static Audio audio; return &audio; }
uint32_t Audio::Load(const std::string &filename) {
    auto &audio = *GetInstance(); std::lock_guard lock(audio.audioMutex);
    auto key = std::filesystem::absolute(UTF8ToWString(filename)).lexically_normal();
    auto canonical = WStringToUTF8(key.wstring());
    for (auto &[id, data] : soundDatas) if (data.filename == canonical) return id;
    auto data = Decode(canonical);
    uint32_t id = soundHandles++; soundDatas.emplace(id, std::move(data)); return id;
}
Audio::SoundData Audio::Decode(const std::string &filename) {
    using Microsoft::WRL::ComPtr;
    ComPtr<IMFSourceReader> reader;
    ComPtr<IStream> memoryStream; ComPtr<IMFByteStream> byteStream;
    if (GameArchiveMounted()) {
        auto bytes = ReadAssetBytes(UTF8ToWString(filename));
        if (bytes->size() > UINT_MAX) throw std::runtime_error("Audio asset too large");
        memoryStream.Attach(SHCreateMemStream(bytes->data(), UINT(bytes->size())));
        if (!memoryStream) throw std::runtime_error("Cannot create audio memory stream");
        ThrowIfFailed(MFCreateMFByteStreamOnStream(memoryStream.Get(), &byteStream));
        ComPtr<IMFAttributes> attributes; ThrowIfFailed(byteStream.As(&attributes));
        auto extension = std::filesystem::path(UTF8ToWString(filename)).extension();
        ThrowIfFailed(attributes->SetString(MF_BYTESTREAM_CONTENT_TYPE, extension == L".mp3" ? L"audio/mpeg" : L"audio/wav"));
        ThrowIfFailed(MFCreateSourceReaderFromByteStream(byteStream.Get(), nullptr, &reader));
    } else ThrowIfFailed(MFCreateSourceReaderFromURL(UTF8ToWString(filename).c_str(), nullptr, &reader));
    ComPtr<IMFMediaType> desired, actual;
    ThrowIfFailed(MFCreateMediaType(&desired));
    ThrowIfFailed(desired->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio));
    ThrowIfFailed(desired->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM));
    ThrowIfFailed(desired->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16));
    ThrowIfFailed(reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, desired.Get()));
    ThrowIfFailed(reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &actual));
    WAVEFORMATEX *format = nullptr; UINT32 formatSize = 0;
    ThrowIfFailed(MFCreateWaveFormatExFromMFMediaType(actual.Get(), &format, &formatSize));
    std::unique_ptr<WAVEFORMATEX, decltype(&CoTaskMemFree)> cleanup(format, CoTaskMemFree);
    SoundData result; result.filename = filename; result.wfex = *format;
    result.format.assign(reinterpret_cast<BYTE *>(format), reinterpret_cast<BYTE *>(format) + formatSize);
    std::vector<BYTE> pcm;
    for (;;) {
        DWORD flags = 0; ComPtr<IMFSample> sample;
        ThrowIfFailed(reader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, nullptr, &sample));
        if (flags & MF_SOURCE_READERF_ERROR) throw std::runtime_error("Audio decode failed: " + filename);
        if (sample) {
            ComPtr<IMFMediaBuffer> buffer; ThrowIfFailed(sample->ConvertToContiguousBuffer(&buffer));
            BYTE *bytes = nullptr; DWORD count = 0; ThrowIfFailed(buffer->Lock(&bytes, nullptr, &count));
            try { pcm.insert(pcm.end(), bytes, bytes + count); } catch (...) { buffer->Unlock(); throw; }
            ThrowIfFailed(buffer->Unlock());
        }
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;
    }
    if (pcm.empty() || pcm.size() > XAUDIO2_MAX_BUFFER_BYTES) throw std::runtime_error("Invalid audio length: " + filename);
    result.bufferSize = uint32_t(pcm.size()); result.pBuffer = std::make_unique<BYTE[]>(pcm.size());
    std::copy(pcm.begin(), pcm.end(), result.pBuffer.get()); return result;
}
void Audio::Initialize() {
    std::lock_guard lock(audioMutex);
    ThrowIfFailed(XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR));
    ThrowIfFailed(xAudio2->CreateMasteringVoice(&masterVoice));
}
void Audio::Play(uint32_t handle, bool loop) {
    std::lock_guard lock(audioMutex); auto it = soundDatas.find(handle); if (it == soundDatas.end()) return; auto &s = it->second;
    if (s.isPaused && s.pSourceVoice) { ThrowIfFailed(s.pSourceVoice->Start()); s.isPaused=false; s.isPlaying=true; return; }
    Stop(handle);
    if (!s.pSourceVoice) ThrowIfFailed(xAudio2->CreateSourceVoice(&s.pSourceVoice, reinterpret_cast<WAVEFORMATEX *>(s.format.data())));
    XAUDIO2_BUFFER buffer{}; buffer.pAudioData=s.pBuffer.get(); buffer.AudioBytes=s.bufferSize;
    buffer.Flags=XAUDIO2_END_OF_STREAM; buffer.LoopCount=loop ? XAUDIO2_LOOP_INFINITE : 0;
    ThrowIfFailed(s.pSourceVoice->SetVolume(s.volume));
    ThrowIfFailed(s.pSourceVoice->SubmitSourceBuffer(&buffer)); ThrowIfFailed(s.pSourceVoice->Start());
    s.isPlaying=true; s.isPaused=false;
}
bool Audio::IsPlaying(uint32_t handle) {
    std::lock_guard lock(audioMutex); auto it=soundDatas.find(handle);
    if (it==soundDatas.end() || !it->second.pSourceVoice || it->second.isPaused) return false;
    XAUDIO2_VOICE_STATE state{}; it->second.pSourceVoice->GetState(&state); return state.BuffersQueued != 0;
}
bool Audio::Pause(uint32_t handle) {
    std::lock_guard lock(audioMutex); auto it=soundDatas.find(handle);
    if (it==soundDatas.end() || !it->second.pSourceVoice) return false;
    ThrowIfFailed(it->second.pSourceVoice->Stop()); it->second.isPaused=true; it->second.isPlaying=false; return true;
}
bool Audio::Stop(uint32_t handle) {
    std::lock_guard lock(audioMutex); auto it=soundDatas.find(handle);
    if (it==soundDatas.end()) return false;
    if (it->second.pSourceVoice) { ThrowIfFailed(it->second.pSourceVoice->Stop()); ThrowIfFailed(it->second.pSourceVoice->FlushSourceBuffers()); }
    it->second.isPaused=it->second.isPlaying=false; return true;
}
void Audio::SetVolume(uint32_t handle, float volume) {
    if (!std::isfinite(volume)) throw std::runtime_error("Invalid audio volume");
    std::lock_guard lock(audioMutex); auto it=soundDatas.find(handle); if(it==soundDatas.end()) return; auto &s=it->second; s.volume=std::clamp(volume,0.f,1.f);
    if (s.pSourceVoice) ThrowIfFailed(s.pSourceVoice->SetVolume(s.volume));
}
float Audio::GetVolume(uint32_t handle) { std::lock_guard lock(audioMutex); auto it=soundDatas.find(handle); return it==soundDatas.end() ? 0 : it->second.volume; }
void Audio::SetMasterVolume(float volume) {if(!std::isfinite(volume)||volume<0||volume>1)throw std::runtime_error("Invalid master volume");std::lock_guard lock(audioMutex);masterVolume=volume;if(masterVoice)ThrowIfFailed(masterVoice->SetVolume(volume));}
float Audio::GetBusVolume(int bus) const {if(bus<0||bus>=3)throw std::runtime_error("Invalid audio bus");return busVolumes[bus];}
void Audio::SetBusVolume(int bus,float volume) {if(bus<0||bus>=3||!std::isfinite(volume)||volume<0||volume>1)throw std::runtime_error("Invalid bus volume");std::lock_guard lock(audioMutex);busVolumes[bus]=volume;for(auto &v:voices)if(v.bus==bus)ThrowIfFailed(v.voice->SetVolume(v.gain*volume));}
void Audio::PlayClip(const std::string &path, float volume, bool loop, uint64_t owner, int bus) {
    if(bus<0||bus>=3)throw std::runtime_error("Invalid audio bus");
    if (!std::isfinite(volume)) throw std::runtime_error("Invalid audio volume");
    std::lock_guard lock(audioMutex); if (!xAudio2) throw std::runtime_error("Audio is not initialized");
    Update(); auto id=Load(path); auto &s=soundDatas.at(id);
    if (loop && owner) StopOwner(owner);
    // Bounded effect voices; repeated kills do not restart a shared voice.
    if (voices.size() >= 32) {
        auto oldest=std::find_if(voices.begin(),voices.end(),[](const Voice &v){return !v.loop;});
        if(oldest==voices.end()) throw std::runtime_error("Audio voice limit reached");
        oldest->voice->DestroyVoice(); voices.erase(oldest);
    }
    IXAudio2SourceVoice *voice=nullptr;
    ThrowIfFailed(xAudio2->CreateSourceVoice(&voice, reinterpret_cast<WAVEFORMATEX *>(s.format.data())));
    try {
        ThrowIfFailed(voice->SetVolume(std::clamp(volume,0.f,1.f)*busVolumes[bus]));
        XAUDIO2_BUFFER buffer{}; buffer.pAudioData=s.pBuffer.get(); buffer.AudioBytes=s.bufferSize;
        buffer.Flags=XAUDIO2_END_OF_STREAM; buffer.LoopCount=loop ? XAUDIO2_LOOP_INFINITE : 0;
        ThrowIfFailed(voice->SubmitSourceBuffer(&buffer)); if (!paused) ThrowIfFailed(voice->Start());
        voices.push_back({voice,id,owner,loop,false,bus,std::clamp(volume,0.f,1.f)});
    } catch (...) { voice->DestroyVoice(); throw; }
}
void Audio::Update() {
    std::lock_guard lock(audioMutex);
    std::erase_if(voices,[](const Voice &v) { XAUDIO2_VOICE_STATE s{}; v.voice->GetState(&s); if (s.BuffersQueued) return false; v.voice->DestroyVoice(); return true; });
}
void Audio::StopOwner(uint64_t owner) {
    std::lock_guard lock(audioMutex);
    std::erase_if(voices,[&](const Voice &v) { if (v.owner != owner) return false; v.voice->DestroyVoice(); return true; });
}
void Audio::StopAll() { std::lock_guard lock(audioMutex); for(auto &v:voices) v.voice->DestroyVoice(); voices.clear(); for(auto &[id,s]:soundDatas) Stop(id); paused=false; }
void Audio::SetPaused(bool value) {
    std::lock_guard lock(audioMutex); if (paused==value) return;
    for(auto &v:voices) { if(value) ThrowIfFailed(v.voice->Stop()); else if(!v.ownerPaused) ThrowIfFailed(v.voice->Start()); } paused=value;
}
void Audio::SetOwnerPaused(uint64_t owner,bool value) {
    std::lock_guard lock(audioMutex);for(auto &v:voices)if(v.owner==owner && v.ownerPaused!=value){v.ownerPaused=value;if(value)ThrowIfFailed(v.voice->Stop());else if(!paused)ThrowIfFailed(v.voice->Start());}
}
void Audio::Finalize() {
    std::lock_guard lock(audioMutex); StopAll();
    for(auto &[id,s]:soundDatas) if(s.pSourceVoice) s.pSourceVoice->DestroyVoice();
    soundDatas.clear(); if(masterVoice) { masterVoice->DestroyVoice(); masterVoice=nullptr; } xAudio2.Reset();
}
}
