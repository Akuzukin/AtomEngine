using System.Text.Json;
namespace Atom;

/// <summary>访问当前项目的场景清单，并在下一帧安全切换游戏场景。</summary>
public sealed class SceneManager
{
    private readonly GameObject owner;
    internal SceneManager(GameObject owner) => this.owner = owner;
    /// <summary>当前游戏场景相对于项目目录的路径。</summary>
    public string CurrentScene => owner.Request(new { op = "scene", action = "current" }).GetString() ?? "";
    /// <summary>最近一次异步场景加载的错误；新请求或成功加载后清空。</summary>
    public string LastLoadError => owner.Request(new { op = "scene", action = "error" }).GetString() ?? "";
    /// <summary>是否存在尚待下一帧处理的切换请求；加载失败会保留旧场景及脚本状态。</summary>
    public bool IsLoading => owner.Request(new { op = "scene", action = "loading" }).GetBoolean();
    /// <summary>项目构建清单内可加载的场景路径。</summary>
    public string[] Scenes => owner.Request(new { op = "scene", action = "list" }).Deserialize<string[]>() ?? [];
    /// <summary>请求切换场景；当前回调完成后在下一帧执行，旧场景对象及脚本实例不再有效。</summary>
    /// <param name="nameOrPath">场景名称或项目清单中的完整相对路径；名称重复时必须传入路径。</param>
    public void LoadScene(string nameOrPath)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(nameOrPath);
        owner.Request(new { op = "scene", action = "load", scene = nameOrPath });
    }
}
public sealed partial class GameObject
{
    /// <summary>当前对象所属运行上下文的项目场景接口。</summary>
    public SceneManager SceneManager => new(this);
}
public abstract partial class Behaviour
{
    /// <summary>当前脚本的项目场景接口；在生命周期方法中直接调用 SceneManager.LoadScene。</summary>
    public SceneManager SceneManager => Object.SceneManager;
}
