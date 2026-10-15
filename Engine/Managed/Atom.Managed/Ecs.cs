namespace Atom;

/// <summary>原生 ECS 对象的激活状态变更，用于一次提交多个实体。</summary>
/// <param name="Object">当前世界中的有效游戏对象。</param>
/// <param name="Active">是否激活对象自身。</param>
public readonly record struct ObjectActivation(GameObject Object, bool Active);

public sealed partial class GameObject
{
    /// <summary>此对象在场景中的持久化ID，可用于组件引用、存档与预制体实例查询。</summary>
    public string PersistentId => Request(new {op="ecsId",entity=Entity}).GetString()!;

    /// <summary>按名称在当前世界的 C++ ECS 中精确查找对象，包含未激活对象；同名时返回实体句柄最小的对象，不存在返回 null。</summary>
    /// <param name="name">区分大小写的完整场景对象名称；需要唯一定位时使用 FindById。</param>
    public GameObject? FindByName(string name)
    {
        ArgumentNullException.ThrowIfNull(name);
        var result=Request(new {op="ecsFind",name});
        return result.ValueKind==System.Text.Json.JsonValueKind.Null?null:Wrap(result.GetUInt32());
    }

    /// <summary>将当前世界的原生实体句柄解析为对象引用；实体已销毁或版本不符时返回 null，句柄不可跨世界使用。</summary>
    /// <param name="entity">来自当前世界查询或碰撞回调的实体句柄。</param>
    public unsafe GameObject? GetObject(uint entity)
    {
        EnsureValid();
        return Api.Valid(Context,entity)==0?null:Wrap(entity);
    }

    /// <summary>检查此对象是否实际拥有指定 C++ ECS 组件；与组件启用状态无关。</summary>
    /// <param name="component">需要检查的组件类别。</param>
    public bool HasComponent(ComponentKind component)=>Request(new {op="ecsHas",entity=Entity,kind=(int)component}).GetBoolean();

    /// <summary>通过 C++ ECS 组件视图查询当前世界，返回按实体句柄排序的对象快照；网格源与运行时网格合并去重。</summary>
    /// <param name="component">要求对象拥有的组件类别。</param>
    /// <param name="activeOnly">为真时仅包含层级激活且该组件启用的对象；为假时包含未激活对象。</param>
    public GameObject[] Query(ComponentKind component,bool activeOnly=true)=>Request(new {op="ecsQuery",kind=(int)component,activeOnly}).EnumerateArray().Select(e=>Wrap(e.GetUInt32())).ToArray();

    /// <summary>一次原生调用批量修改 ObjectStateComponent，先校验全部对象再提交；不销毁对象，停用脚本与物理遵循引擎生命周期。</summary>
    /// <param name="changes">同一世界对象的状态变更列表；空列表不调用原生层，同一对象多次出现时以最后一次为准。</param>
    public void SetActiveBatch(IReadOnlyList<ObjectActivation> changes)
    {
        EnsureValid();ArgumentNullException.ThrowIfNull(changes);
        if(changes.Count==0)return;
        var updates=new (uint Entity,bool Active)[changes.Count];
        for(int i=0;i<changes.Count;i++)
        {
            ArgumentNullException.ThrowIfNull(changes[i].Object);
            updates[i]=(Ignore(changes[i].Object),changes[i].Active);
        }
        Request(new {op="ecsActiveBatch",changes=updates.Select(c=>new {entity=c.Entity,active=c.Active}).ToArray()});
    }
}
