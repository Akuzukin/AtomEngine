using Atom;
namespace Game;

/// <summary>多场景回归夹具，切换请求必须来自实际脚本回调。</summary>
public sealed class ProjectSceneProbe : Behaviour
{
    /// <summary>下一场景路径。</summary>
    public string Destination = "";
    /// <summary>记录当前实例的更新次数，用于验证加载失败没有重建脚本。</summary>
    public int Updates;
    private int frames;
    /// <summary>验证清单与时间重置后，在回调中发起延迟切换。</summary>
    public override void OnUpdate(float dt)
    {
        if (frames == 0 && Time.FrameCount != 1) throw new InvalidOperationException("Scene time did not reset");
        if (Object.SceneManager.Scenes.Length != 2) throw new InvalidOperationException("Project scene list mismatch");
        Updates = ++frames;
        if (frames == 3) Object.SceneManager.LoadScene(Destination);
    }
}
