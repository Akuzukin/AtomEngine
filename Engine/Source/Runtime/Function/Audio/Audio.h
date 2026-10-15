#pragma once
#include <xaudio2.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
namespace AtomEngine
{
class Audio
{
  public:
    struct SoundData {
        std::string filename;
        WAVEFORMATEX wfex{};
        std::vector<BYTE> format;
        std::unique_ptr<BYTE[]> pBuffer;
        uint32_t bufferSize = 0;
        IXAudio2SourceVoice *pSourceVoice = nullptr;
        bool isPlaying = false, isPaused = false;
        float volume = 1;
    };
    static Audio *GetInstance();
    static uint32_t Load(const std::string &filename);
    void Play(uint32_t handle, bool loop = false);
    bool IsPlaying(uint32_t handle);
    bool Pause(uint32_t handle);
    bool Stop(uint32_t handle);
    void SetVolume(uint32_t handle, float volume);
    float GetVolume(uint32_t handle);
    // Independent voices allow overlapping effects; owner 0 means unowned one-shot.
    void PlayClip(const std::string &path, float volume = 1, bool loop = false, uint64_t owner = 0, int bus = 0);
    void SetMasterVolume(float volume);
    float GetMasterVolume() const { return masterVolume; }
    void SetBusVolume(int bus,float volume);
    float GetBusVolume(int bus) const;
    void StopOwner(uint64_t owner);
    void StopAll();
    void SetPaused(bool paused);
    void SetOwnerPaused(uint64_t owner,bool paused);
    bool IsPaused() const { return paused; }
    void Update();
    size_t ActiveVoiceCount() const { return voices.size(); }
    void Initialize();
    void Finalize();
  private:
    struct Voice { IXAudio2SourceVoice *voice; uint32_t sound; uint64_t owner; bool loop; bool ownerPaused=false; int bus=0; float gain=1; };
    float masterVolume=1;
    float busVolumes[3]={1,1,1};
    std::vector<Voice> voices;
    bool paused = false;
    Microsoft::WRL::ComPtr<IXAudio2> xAudio2;
    IXAudio2MasteringVoice *masterVoice = nullptr;
    static std::unordered_map<uint32_t, SoundData> soundDatas;
    static uint32_t soundHandles;
    std::recursive_mutex audioMutex;
    static SoundData Decode(const std::string &filename);
};
}
