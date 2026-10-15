namespace Atom;
/// <summary>原生音频混音总线。</summary>
public enum AudioBus
{
    /// <summary>游戏音效。</summary>
    Effects,
    /// <summary>背景音乐。</summary>
    Music,
    /// <summary>界面音效。</summary>
    UI
}
/// <summary>控制当前应用的全局音量与总线，变更立即影响正在播放和新播放的声部。</summary>
public sealed class AudioMixer
{
    private readonly GameObject owner;
    internal AudioMixer(GameObject owner)=>this.owner=owner;
    /// <summary>全局音量，范围0到1。</summary>
    public float MasterVolume {get=>owner.Request(new{op="mixer",action="master"}).GetSingle();set=>owner.Request(new{op="mixer",action="master",value});}
    /// <summary>读取总线音量。</summary>
    /// <param name="bus">声音总线。</param>
    public float GetVolume(AudioBus bus)=>owner.Request(new{op="mixer",action="bus",bus=(int)bus}).GetSingle();
    /// <summary>设置总线音量，范围0到1。</summary>
    /// <param name="bus">声音总线。</param>
    /// <param name="volume">目标音量。</param>
    public void SetVolume(AudioBus bus,float volume)=>owner.Request(new{op="mixer",action="bus",bus=(int)bus,value=volume});
    /// <summary>在指定原生总线上播放音效。</summary>
    /// <param name="clip">音频资源。</param>
    /// <param name="bus">声音总线。</param>
    /// <param name="volume">声部音量。</param>
    public void Play(AudioClip clip,AudioBus bus=AudioBus.Effects,float volume=1)=>owner.Request(new{op="mixer",action="play",entity=owner.Entity,path=clip.Path,bus=(int)bus,volume});
}
public sealed partial class GameObject
{
    /// <summary>当前应用的原生音频混音接口。</summary>
    public AudioMixer AudioMixer=>new(this);
}
