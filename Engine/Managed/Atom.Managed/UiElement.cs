using System.Numerics;
using System.Text.Json;
namespace Atom;
/// <summary>UI 元素访问接口；所有属性写入均由引擎校验，对象需拥有 UI 组件。</summary>
public sealed class UiElement {
    private readonly GameObject owner;internal UiElement(GameObject value){owner=value;}
    private JsonElement Read()=>owner.Request(new{op="ui",entity=owner.Entity});
    private void Write(string name,object value)=>owner.Request(new Dictionary<string,object>{{"op","ui"},{"entity",owner.Entity},{name,value}});
    private Vector2 Get(string name){var v=Read().GetProperty(name);return new(v[0].GetSingle(),v[1].GetSingle());}
    private void Set(string name,Vector2 v)=>Write(name,new[]{v.X,v.Y});
    /// <summary>UI 布局偏移量；响应式模式下相对于锚点计算位置。</summary>
    public Vector2 Position{get=>Get("position");set=>Set("position",value);}
    /// <summary>UI 尺寸，各分量须大于零且不超过 4096。</summary>
    public Vector2 Size{get=>Get("size");set=>Set("size",value);}
    /// <summary>锚点区域的最小坐标，范围 0～1，不能大于 AnchorMax。</summary>
    public Vector2 AnchorMin{get=>Get("anchorMin");set=>Set("anchorMin",value);}
    /// <summary>锚点区域的最大坐标，范围 0～1，不能小于 AnchorMin。</summary>
    public Vector2 AnchorMax{get=>Get("anchorMax");set=>Set("anchorMax",value);}
    /// <summary>UI 的归一化轴心坐标，范围 0～1。</summary>
    public Vector2 Pivot{get=>Get("pivot");set=>Set("pivot",value);}
    /// <summary>进度显示值，范围 0～1。</summary>
    public float Progress{get=>Read().GetProperty("progress").GetSingle();set=>Write("progress",value);}
    /// <summary>UI 元素是否可见。</summary>
    public bool Visible{get=>Read().GetProperty("visible").GetBoolean();set=>Write("visible",value);}
    /// <summary>UI 元素是否接受点击和键盘、手柄导航。</summary>
    public bool Interactable{get=>Read().GetProperty("interactable").GetBoolean();set=>Write("interactable",value);}
    /// <summary>是否使用锚点、轴心和视口尺寸计算响应式布局。</summary>
    public bool Responsive{get=>Read().GetProperty("responsive").GetBoolean();set=>Write("responsive",value);}
}
public sealed partial class GameObject {
    /// <summary>访问此对象的 UI 组件；缺少组件时，属性读写抛出异常。</summary>
    public UiElement UI=>new(this);
}
