namespace Atom;
/// <summary>MP3/WAV 音频资源引用；公开脚本字段支持从 Inspector 拖拽赋值，不会在构造时解码文件。</summary>
public readonly record struct AudioClip
{
    private readonly string? path;
    /// <summary>引擎使用的资源路径；未指定音频时为空字符串。</summary>
    public string Path => path??string.Empty;
    /// <summary>已经指定非空资源路径时为真。</summary>
    public bool IsAssigned => path!=null;
    /// <summary>未指定的音频引用，适合作为 Inspector 可配置字段的默认值。</summary>
    public static AudioClip None => default;
    /// <summary>创建音频引用；空白路径表示未指定，仅接受 MP3/WAV，不会立即加载文件。</summary>
    /// <param name="path">资源路径，可相对于引擎工作目录或使用绝对路径。</param>
    public AudioClip(string path)
    {
        ArgumentNullException.ThrowIfNull(path);
        if(string.IsNullOrWhiteSpace(path)){this.path=null;return;}
        if(path.Contains('\0'))throw new ArgumentException("Audio path contains a null character",nameof(path));
        string extension=System.IO.Path.GetExtension(path);
        if(!extension.Equals(".mp3",StringComparison.OrdinalIgnoreCase) && !extension.Equals(".wav",StringComparison.OrdinalIgnoreCase))
            throw new ArgumentException("Audio clip must reference an MP3 or WAV file",nameof(path));
        this.path=path.Replace('\\','/');
    }
    /// <summary>返回资源路径，用于诊断和序列化。</summary>
    public override string ToString()=>Path;
}
public sealed partial class GameObject
{
    /// <summary>播放指定音频引用，创建独立的音效实例；未指定引用返回假。</summary>
    /// <param name="clip">Inspector 配置的音频引用或生成的 AudioAssets 成员。</param>
    /// <param name="volume">音量，通常为 0～1，原生音频服务限制有限数值的范围。</param>
    public bool PlayAudio(AudioClip clip,float volume=1)
    {
        EnsureValid();if(!float.IsFinite(volume))throw new ArgumentOutOfRangeException(nameof(volume));
        return clip.IsAssigned && PlaySound(clip.Path,volume);
    }
}
