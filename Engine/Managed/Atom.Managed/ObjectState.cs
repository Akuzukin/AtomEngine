namespace Atom;
/// <summary>可独立启用或停用的引擎组件类别。</summary>
public enum ComponentKind
{
    /// <summary>网格渲染组件。</summary>
    Mesh,
    /// <summary>碰撞体组件。</summary>
    Collider,
    /// <summary>刚体组件。</summary>
    RigidBody,
    /// <summary>脚本组件。</summary>
    Script,
    /// <summary>用户界面组件。</summary>
    UI,
    /// <summary>音频组件。</summary>
    Audio,
    /// <summary>相机组件。</summary>
    Camera,
    /// <summary>灯光组件。</summary>
    Light,
    /// <summary>物理关节组件。</summary>
    Joint,
}
public sealed partial class GameObject {
    /// <summary>此对象自身的激活状态；父对象的状态不改变此值。</summary>
    public bool ActiveSelf {get=>Request(new{op="active",entity=Entity}).GetBoolean();set=>Request(new{op="active",entity=Entity,value});}
    /// <summary>此对象及父层级全部激活时为真。</summary>
    public bool ActiveInHierarchy=>Request(new{op="activeHierarchy",entity=Entity}).GetBoolean();
    /// <summary>读取此对象的组件启用标记；不代表组件一定存在或父层级激活。</summary>
    /// <param name="component">组件类别。</param>
    public bool IsComponentEnabled(ComponentKind component)=>Request(new{op="component",entity=Entity,kind=(int)component}).GetBoolean();
    /// <summary>设置组件启用标记；不会移除组件，脚本实例在重新启用时复用。</summary>
    /// <param name="component">组件类别。</param>
    /// <param name="enabled">是否启用该组件。</param>
    public void SetComponentEnabled(ComponentKind component,bool enabled)=>Request(new{op="component",entity=Entity,kind=(int)component,value=enabled});
}
