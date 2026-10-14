using System.Numerics;
using System.Text.Json;
namespace Atom;
/// <summary>射线命中结果，包含命中对象、世界坐标命中点、法线和距离。</summary>
/// <param name="Object">命中对象。</param>
/// <param name="Point">世界坐标命中点。</param>
/// <param name="Normal">世界坐标表面法线。</param>
/// <param name="Distance">从射线起点到命中点的距离。</param>
public readonly record struct RaycastHit(GameObject Object,Vector3 Point,Vector3 Normal,float Distance);
public sealed partial class GameObject {
    private static float[] V(Vector3 v)=>new[]{v.X,v.Y,v.Z};
    internal unsafe JsonElement Request(object request) {
        EnsureValid();var data=Utf8(JsonSerializer.Serialize(request));int count;fixed(byte* p=data)count=Api.Exchange(Context,p,null,0);
        if(count<=0 || count>16777216)throw new InvalidOperationException("Invalid native response size");var buffer=new byte[count];
        fixed(byte* p=buffer)Api.Exchange(Context,null,p,count);using var doc=JsonDocument.Parse(buffer.AsMemory(0,count-1));var value=doc.RootElement.Clone();
        if(value.ValueKind==JsonValueKind.Object && value.TryGetProperty("error",out var error))throw new InvalidOperationException(error.GetString());return value;
    }
    /// <summary>沿世界坐标射线查询最近的碰撞体；方向自动归一化，未命中返回 null，可过滤碰撞层、触发器和指定对象。</summary>
    /// <param name="origin">射线起点，世界坐标。</param>
    /// <param name="direction">非零有限射线方向，内部归一化。</param>
    /// <param name="distance">射线最大距离，必须为正且有限。</param>
    /// <param name="mask">碰撞层位掩码，第 n 位对应第 n 层，默认查询全部层。</param>
    /// <param name="includeTriggers">是否包含触发器，默认不包含。</param>
    /// <param name="ignore">可忽略的同一世界对象，null 表示不忽略。</param>
    public RaycastHit? Raycast(Vector3 origin,Vector3 direction,float distance,uint mask=uint.MaxValue,bool includeTriggers=false,GameObject? ignore=null) {
        Validate(origin);Validate(direction);if(direction.LengthSquared()<1e-12f || !float.IsFinite(distance)||distance<=0)throw new ArgumentException("Ray direction and distance must be positive and finite");
        var hit=Request(new{op="raycast",origin=V(origin),direction=V(Mathf.NormalizeOrZero(direction)),distance,mask,triggers=includeTriggers,ignore=Ignore(ignore)});
        if(hit.ValueKind==JsonValueKind.Null)return null;var point=hit.GetProperty("point");var normal=hit.GetProperty("normal");
        return new(Wrap(hit.GetProperty("entity").GetUInt32()),new(point[0].GetSingle(),point[1].GetSingle(),point[2].GetSingle()),new(normal[0].GetSingle(),normal[1].GetSingle(),normal[2].GetSingle()),hit.GetProperty("distance").GetSingle());
    }
    /// <summary>查询与世界坐标球体重叠的碰撞体，返回对象数组；无结果时为空数组。</summary>
    /// <param name="center">球心，世界坐标。</param>
    /// <param name="radius">球体半径，必须为正且有限。</param>
    /// <param name="mask">碰撞层位掩码，第 n 位对应第 n 层，默认查询全部层。</param>
    /// <param name="includeTriggers">是否包含触发器，默认不包含。</param>
    /// <param name="ignore">可忽略的同一世界对象，null 表示不忽略。</param>
    public GameObject[] OverlapSphere(Vector3 center,float radius,uint mask=uint.MaxValue,bool includeTriggers=false,GameObject? ignore=null) {
        Validate(center);if(!float.IsFinite(radius)||radius<=0)throw new ArgumentException("Radius must be positive and finite");
        return Request(new{op="overlap",center=V(center),radius,mask,triggers=includeTriggers,ignore=Ignore(ignore)}).EnumerateArray().Select(e=>Wrap(e.GetUInt32())).ToArray();
    }
    private uint Ignore(GameObject? other){if(other==null)return uint.MaxValue;other.EnsureValid();if(other.Context!=Context || other.Generation!=Generation)throw new ArgumentException("Ignore object belongs to another World");return other.Entity;}
    private static void Validate(Vector3 v){if(!float.IsFinite(v.X)||!float.IsFinite(v.Y)||!float.IsFinite(v.Z))throw new ArgumentException("Vector must be finite");}
}
