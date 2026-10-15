using System.Numerics;
namespace Atom;
/// <summary>当前游戏视口的鼠标位置，编辑器与独立游戏均以渲染像素计。</summary>
public readonly record struct PointerState(bool Valid,Vector2 Position,Vector2 ViewportSize);
public sealed partial class GameObject
{
    /// <summary>一次提交完整局部变换，避免分别写入位置、旋转、缩放时重复读取变换。</summary>
    /// <param name="position">有限的局部位置。</param>
    /// <param name="rotation">非零且有限的四元数，由原生接口归一化。</param>
    /// <param name="scale">各分量大于零的有限局部缩放。</param>
    public void SetPose(Vector3 position,Quaternion rotation,Vector3 scale)=>Write(new NativeTransform{Position=position,Rotation=rotation,Scale=scale});
    /// <summary>设置全部材质槽的 RGB 颜色与自发光倍数（0～10）；对象必须拥有材质组件。</summary>
    /// <param name="color">非负且有限的 RGB 颜色值。</param>
    /// <param name="glow">自发光倍数，范围为零到十。</param>
    public void SetTint(Vector3 color,float glow=0)=>Request(new{op="tint",entity=Entity,color=new[]{color.X,color.Y,color.Z},glow});
    /// <summary>读取游戏视口鼠标；失去焦点或移出视口时 Valid 为假。</summary>
    public PointerState Pointer
    {
        get {var r=Request(new{op="pointer"});var p=r.GetProperty("position");var s=r.GetProperty("size");return new(r.GetProperty("valid").GetBoolean(),new(p[0].GetSingle(),p[1].GetSingle()),new(s[0].GetSingle(),s[1].GetSingle()));}
    }
}
