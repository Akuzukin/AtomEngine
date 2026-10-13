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
}
public sealed class GameObject
{
    internal nint Context; public uint Entity { get; internal set; }
    internal unsafe NativeApi Api;
    internal unsafe NativeTransform Read() { NativeTransform t = default; if (Api.GetTransform(Context, Entity, &t) == 0) throw new InvalidOperationException("GameObject was destroyed"); return t; }
    internal unsafe void Write(NativeTransform t) { if (Api.SetTransform(Context, Entity, &t) == 0) throw new InvalidOperationException("GameObject was destroyed"); }
    public Vector3 Position { get => Read().Position; set { var t = Read(); t.Position = value; Write(t); } }
    public Quaternion Rotation { get => Read().Rotation; set { var t = Read(); t.Rotation = Quaternion.Normalize(value); Write(t); } }
    public Vector3 Scale { get => Read().Scale; set { var t = Read(); t.Scale = value; Write(t); } }
    public unsafe string Text { get { int count = Api.UiText(Context, Entity, null, 0, 0); if (count <= 0) throw new InvalidOperationException("GameObject has no UI element"); var bytes = new byte[count]; fixed (byte* p = bytes) Api.UiText(Context, Entity, p, count, 0); return System.Text.Encoding.UTF8.GetString(bytes, 0, count - 1); } set { var bytes = System.Text.Encoding.UTF8.GetBytes(value); fixed (byte* p = bytes) if (Api.UiText(Context, Entity, p, bytes.Length, 1) == 0) throw new InvalidOperationException("GameObject has no UI element"); } }
    public unsafe Vector3 LinearVelocity { get { Vector3 v = default; if (Api.Velocity(Context, Entity, (float*)&v, 0) == 0) throw new InvalidOperationException("GameObject has no rigid body"); return v; } set { var v = value; if (Api.Velocity(Context, Entity, (float*)&v, 1) == 0) throw new InvalidOperationException("Invalid rigid body velocity"); } }
    public unsafe bool IsMouseDown(int button) => Api.Button(0, button) != 0;
    public unsafe bool IsGamepadDown(int button) => Api.Button(1, button) != 0;
    public unsafe bool IsKeyPressed(int virtualKey) => Api.Button(2, virtualKey) != 0;
    public unsafe float GamepadAxis(int axis) => Api.Axis(axis);
    public unsafe bool IsKeyDown(int virtualKey) => Api.KeyDown(virtualKey) != 0;
}
public abstract class Behaviour
{
    public GameObject Object { get; internal set; } = null!;
    public virtual void OnCreate() { }
    public virtual void OnUpdate(float dt) { }
    public virtual void OnFixedUpdate(float dt) { }
    public virtual void OnCollision(uint other, bool trigger) { }
    public virtual void OnUiClick() { }
    public virtual void OnDestroy() { }
}
