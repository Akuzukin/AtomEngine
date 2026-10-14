using System.Numerics;
using System.Runtime.InteropServices;
namespace Atom;

[StructLayout(LayoutKind.Sequential)]
internal struct NativeTransform { public Vector3 Position; public Quaternion Rotation; public Vector3 Scale; }
[StructLayout(LayoutKind.Sequential)]
internal unsafe struct NativeApi
{
    public uint Version, Size;
    public delegate* unmanaged[Cdecl]<nint, uint, NativeTransform*, int> GetTransform;
    public delegate* unmanaged[Cdecl]<nint, uint, NativeTransform*, int> SetTransform;
    public delegate* unmanaged[Cdecl]<int, int> KeyDown;
    public delegate* unmanaged[Cdecl]<nint, byte*, void> Log;
    public delegate* unmanaged[Cdecl]<nint, uint, float*, int, int> Velocity;
    public delegate* unmanaged[Cdecl]<int, int, int> Button;
    public delegate* unmanaged[Cdecl]<int, float> Axis;
    public delegate* unmanaged[Cdecl]<nint, uint, byte*, int, int, int> UiText;
    public delegate* unmanaged[Cdecl]<nint, byte*, uint> Find;
    public delegate* unmanaged[Cdecl]<nint, uint, int> Valid;
    public delegate* unmanaged[Cdecl]<nint, uint, int, int> Visibility;
    public delegate* unmanaged[Cdecl]<nint, uint, int, byte*, float, int> Audio;
    public delegate* unmanaged[Cdecl]<nint, uint, int, byte*, NativeTransform*, ulong> Command;
    public delegate* unmanaged[Cdecl]<nint, ulong, uint*, int> Result;
    public delegate* unmanaged[Cdecl]<nint, ulong> Epoch;
    public delegate* unmanaged[Cdecl]<nint, byte*, void> Reply;
    public delegate* unmanaged[Cdecl]<nint, byte*, byte*, int, int> Exchange;
}
/// <summary>游戏对象引用，绑定实体版本和世界代次；提供变换、输入、音频、UI 和延迟创建接口。</summary>
public sealed partial class GameObject
{
    internal ulong Generation;
    internal nint Context;
    /// <summary>当前对象的运行时实体句柄；跨世界或对象销毁后不可继续使用。</summary>
    public uint Entity { get; internal set; }
    internal unsafe NativeApi Api;
    internal unsafe NativeTransform Read() { EnsureValid(); NativeTransform t = default; if (Api.GetTransform(Context, Entity, &t) == 0) throw new InvalidOperationException("GameObject was destroyed"); return t; }
    internal unsafe void Write(NativeTransform t) { EnsureValid(); if (Api.SetTransform(Context, Entity, &t) == 0) throw new InvalidOperationException("GameObject was destroyed"); }
    /// <summary>对象仍存在且所属世界代次未改变时为真；未绑定的引用返回假。</summary>
    public unsafe bool IsValid => Api.Epoch != null && Api.Valid != null && Api.Epoch(Context) == Generation && Api.Valid(Context, Entity) != 0;
    internal void EnsureValid() { if (!IsValid) throw new InvalidOperationException("GameObject was destroyed or its World changed"); }
    internal unsafe GameObject Wrap(uint entity) => new() { Context = Context, Entity = entity, Api = Api, Generation = Generation };
    /// <summary>播放此对象在 Inspector 配置的 Audio Source；未配置或播放失败时返回假。</summary>
    public unsafe bool PlayAudio() { EnsureValid(); return Api.Audio(Context, Entity, 0, null, 1) != 0; }
    /// <summary>停止此对象拥有的所有音频播放实例。</summary>
    public unsafe void StopAudio() { EnsureValid(); Api.Audio(Context, Entity, 1, null, 1); }
    /// <summary>按资源路径播放独立音效；推荐使用 AudioClip 重载避免调用处硬编码路径。</summary>
    /// <param name="path">资源路径，可相对于引擎工作目录或使用绝对路径。</param>
    /// <param name="volume">音量，通常为 0～1，原生音频服务限制有限数值的范围。</param>
    public unsafe bool PlaySound(string path, float volume = 1)
    {
        EnsureValid(); var bytes = Utf8(path);
        fixed (byte* p = bytes) return Api.Audio(Context, Entity, 2, p, volume) != 0;
    }
    internal static byte[] Utf8(string value) { if (value.Contains('\0')) throw new ArgumentException("Embedded null is not allowed"); return System.Text.Encoding.UTF8.GetBytes(value + "\0"); }
    /// <summary>将实例化预制体加入命令队列；回调安全点执行后可轮询返回请求，新增脚本下一帧初始化。</summary>
    /// <param name="path">资源路径，可相对于引擎工作目录或使用绝对路径。</param>
    /// <param name="position">新对象的局部位置。</param>
    public SpawnRequest InstantiatePrefab(string path, Vector3 position) => QueueSpawn(1, path, position);
    /// <summary>将预制体属性和子对象延迟附加到此普通对象，保留目标身份和变换；已有预制体实例不能再次附加。</summary>
    /// <param name="path">资源路径，可相对于引擎工作目录或使用绝对路径。</param>
    public SpawnRequest ApplyPrefab(string path) => QueueSpawn(3,path,Vector3.Zero);
    /// <summary>延迟创建带 Transform 的具名游戏对象，返回可轮询的请求。</summary>
    /// <param name="name">新对象名称。</param>
    /// <param name="position">新对象的局部位置。</param>
    public SpawnRequest Create(string name, Vector3 position) => QueueSpawn(0, name, position);
    private unsafe SpawnRequest QueueSpawn(int operation, string value, Vector3 position)
    {
        EnsureValid(); var bytes = Utf8(value); var pose = new NativeTransform { Position = position };
        ulong request; fixed(byte* p = bytes) request = Api.Command(Context, Entity, operation, p, &pose);
        if (request == 0) throw new InvalidOperationException("Spawn request rejected (invalid arguments, stopping World or queue limit)");
        return new SpawnRequest(this, request);
    }
    /// <summary>将此对象及其子树的销毁加入队列，在回调安全点执行；无效对象返回假。</summary>
    public unsafe bool Destroy() => IsValid && Api.Command(Context, Entity, 2, null, null) != 0;
    /// <summary>按持久化 ID 在当前世界查找对象；不存在时返回 null。</summary>
    /// <param name="persistentId">场景持久化 ID。</param>
    public unsafe GameObject? FindById(string persistentId)
    {
        EnsureValid(); var bytes = Utf8(persistentId);
        uint entity; fixed (byte* p = bytes) entity = Api.Find(Context, p);
        return Api.Valid(Context, entity) == 0 ? null : Wrap(entity);
    }
    /// <summary>控制可渲染组件的可见性；缺少可渲染组件时抛出异常，不会停用脚本或物理。</summary>
    public unsafe bool Visible
    {
        get { EnsureValid(); int value = Api.Visibility(Context, Entity, -1); if (value < 0) throw new InvalidOperationException("Object has no renderable component or was destroyed"); return value != 0; }
        set { EnsureValid(); if (Api.Visibility(Context, Entity, value ? 1 : 0) < 0) throw new InvalidOperationException("Object has no renderable component or was destroyed"); }
    }
    /// <summary>相对于父对象的局部位置。</summary>
    public Vector3 Position { get => Read().Position; set { var t = Read(); t.Position = value; Write(t); } }
    /// <summary>局部旋转四元数，写入前进行归一化。</summary>
    public Quaternion Rotation { get => Read().Rotation; set { var t = Read(); t.Rotation = Quaternion.Normalize(value); Write(t); } }
    /// <summary>局部缩放，各分量必须为正。</summary>
    public Vector3 Scale { get => Read().Scale; set { var t = Read(); t.Scale = value; Write(t); } }
    /// <summary>UI 元素的文本；对象没有 UI 组件时抛出异常。</summary>
    public unsafe string Text { get { EnsureValid(); int count = Api.UiText(Context, Entity, null, 0, 0); if (count <= 0) throw new InvalidOperationException("GameObject has no UI element"); var bytes = new byte[count]; fixed (byte* p = bytes) Api.UiText(Context, Entity, p, count, 0); return System.Text.Encoding.UTF8.GetString(bytes, 0, count - 1); } set { EnsureValid(); var bytes = System.Text.Encoding.UTF8.GetBytes(value); fixed (byte* p = bytes) if (Api.UiText(Context, Entity, p, bytes.Length, 1) == 0) throw new InvalidOperationException("GameObject has no UI element"); } }
    /// <summary>刚体线速度；没有刚体或写入非法速度时抛出异常。</summary>
    public unsafe Vector3 LinearVelocity { get { EnsureValid(); Vector3 v = default; if (Api.Velocity(Context, Entity, (float*)&v, 0) == 0) throw new InvalidOperationException("GameObject has no rigid body"); return v; } set { EnsureValid(); var v = value; if (Api.Velocity(Context, Entity, (float*)&v, 1) == 0) throw new InvalidOperationException("Invalid rigid body velocity"); } }
    /// <summary>查询鼠标按键是否持续按下，使用原生数字编号；推荐使用 MouseButton 重载。</summary>
    /// <param name="button">要查询的鼠标或手柄按键。</param>
    public unsafe bool IsMouseDown(int button) => Api.Button(0, button) != 0;
    /// <summary>查询手柄按键是否持续按下，使用原生数字编号；推荐使用 GamepadButton 重载。</summary>
    /// <param name="button">要查询的鼠标或手柄按键。</param>
    public unsafe bool IsGamepadDown(int button) => Api.Button(1, button) != 0;
    /// <summary>查询当前帧是否首次按下指定 Win32 虚拟键；推荐使用 KeyCode 重载。</summary>
    /// <param name="virtualKey">Win32 虚拟键编号。</param>
    public unsafe bool IsKeyPressed(int virtualKey) => Api.Button(2, virtualKey) != 0;
    /// <summary>读取原生编号的手柄轴，摇杆范围 -1～1，扳机范围 0～1；推荐使用具名重载。</summary>
    /// <param name="axis">要读取的手柄轴。</param>
    public unsafe float GamepadAxis(int axis) => Api.Axis(axis);
    /// <summary>查询 Win32 虚拟键是否持续按下；查询遵守游戏输入焦点限制。</summary>
    /// <param name="virtualKey">Win32 虚拟键编号。</param>
    public unsafe bool IsKeyDown(int virtualKey) => Api.KeyDown(virtualKey) != 0;
}
/// <summary>引擎脚本基类；重写生命周期回调，公开可写字段可在 Inspector 配置和运行时调整。</summary>
public abstract partial class Behaviour
{
    /// <summary>此脚本绑定的游戏对象，由引擎在 OnCreate 之前设置。</summary>
    public GameObject Object { get; internal set; } = null!;
    /// <summary>实例创建时调用一次，包括初始未激活的对象；其他脚本可能尚未初始化。</summary>
    public virtual void OnCreate() { }
    /// <summary>对象、脚本组件及脚本附件均启用时调用；重新启用复用原实例。</summary>
    public virtual void OnEnable() { }
    /// <summary>脚本离开启用状态时调用；可用于清理持续行为。</summary>
    public virtual void OnDisable() { }
    /// <summary>每个游戏帧调用一次；当前帧的脚本初始化全部完成后执行，暂停或停用时不调用。</summary>
    /// <param name="dt">本次更新间隔，单位秒，可通过 Time.DeltaTime 读取。</param>
    public virtual void OnUpdate(float dt) { }
    /// <summary>每个物理固定步调用一次，通常为 1/60 秒；一帧可能执行零次或多次。</summary>
    /// <param name="dt">本次更新间隔，单位秒，可通过 Time.DeltaTime 读取。</param>
    public virtual void OnFixedUpdate(float dt) { }
    /// <summary>物理碰撞或触发器接触回调；Time.DeltaTime 使用固定步长。</summary>
    /// <param name="other">对方对象的运行时实体句柄。</param>
    /// <param name="trigger">此接触是否涉及触发器。</param>
    public virtual void OnCollision(uint other, bool trigger) { }
    /// <summary>此对象的可交互 UI 被鼠标、键盘或手柄激活时调用。</summary>
    public virtual void OnUiClick() { }
    /// <summary>脚本销毁、停止游戏或热重载替换旧实例时调用；不要依赖此时对象仍然有效。</summary>
    public virtual void OnDestroy() { }
    /// <summary>热重载前返回需要保留的私有状态字符串；默认 null 表示重新走 OnCreate。</summary>
    public virtual string? CaptureReloadState() => null;
    /// <summary>热重载的新实例接收旧实例保存的状态；抛出异常会取消此次替换。</summary>
    /// <param name="state">旧实例 CaptureReloadState 返回的状态字符串。</param>
    public virtual void RestoreReloadState(string state) { }
    /// <summary>成功恢复非 null 的热重载状态后调用，替代 OnCreate；公开字段由引擎迁移。</summary>
    public virtual void OnReload() { }
}
