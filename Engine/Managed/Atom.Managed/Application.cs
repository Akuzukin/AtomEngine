namespace Atom;
/// <summary>当前原生应用的生命周期接口。</summary>
public sealed class Application
{
    private readonly GameObject owner;
    internal Application(GameObject owner)=>this.owner=owner;
    /// <summary>请求正常退出当前原生应用，包括编辑器进程；不强制终止进程。</summary>
    public void Quit()=>owner.Request(new{op="application",action="quit"});
    /// <summary>设置当前应用窗口的客户区大小与全屏状态，在窗口消息处理后更新渲染缓冲。</summary>
    /// <param name="width">窗口模式客户区宽度，320到16384。</param>
    /// <param name="height">窗口模式客户区高度，240到16384。</param>
    /// <param name="fullscreen">是否使用无边框全屏；全屏尺寸由当前显示器决定。</param>
    public void SetWindow(int width,int height,bool fullscreen=false)=>owner.Request(new{op="application",action="window",width,height,fullscreen});
}
public sealed partial class GameObject
{
    /// <summary>当前对象所属原生应用的生命周期接口。</summary>
    public Application Application=>new(this);
}
public abstract partial class Behaviour
{
    /// <summary>当前脚本所属原生应用的生命周期接口。</summary>
    public Application Application=>Object.Application;
}
