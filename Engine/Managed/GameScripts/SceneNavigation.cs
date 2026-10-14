using Atom;
namespace Game;

/// <summary>项目多场景示例：UI 点击或指定按键切换到目标场景。</summary>
public sealed class SceneNavigation : Behaviour
{
    /// <summary>项目清单中的场景名称或相对路径。</summary>
    public string Destination = "Survivors";
    /// <summary>切换按键；None 表示仅允许 UI 点击。</summary>
    public KeyCode Key = KeyCode.Enter;
    /// <summary>处理项目场景切换按键。</summary>
    public override void OnUpdate(float dt) { if (Key != KeyCode.None && Object.Input.WasPressed(Key)) Object.SceneManager.LoadScene(Destination); }
    /// <summary>响应按钮点击并请求切换。</summary>
    public override void OnUiClick() => Object.SceneManager.LoadScene(Destination);
}

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
