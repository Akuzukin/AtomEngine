using System.Numerics;
namespace Atom;
/// <summary>可序列化的粒子组件引用；Inspector可拖入场景组件、粒子预制体或.particle资产；资产会创建场景组件并绑定引用，在当前世界解析。</summary>
public readonly record struct ParticleComponent
{
    private readonly GameObject? context;
    private readonly GameObject? direct;
    private readonly string? persistentId;
    /// <summary>目标对象的持久化ID，空字符串表示未分配。</summary>
    public string PersistentId => persistentId ?? "";
    /// <summary>未分配的粒子组件引用。</summary>
    public static ParticleComponent None=>default;
    /// <summary>建立持久ID引用；脚本字段由宿主自动绑定世界。</summary>
    /// <param name="persistentId">场景对象或已实例化预制体对象的持久ID。</param>
    public ParticleComponent(string persistentId){this.persistentId=persistentId??throw new ArgumentNullException(nameof(persistentId));context=null;direct=null;}
    internal ParticleComponent(GameObject owner){persistentId=owner.PersistentId;context=owner;direct=owner;}
    internal ParticleComponent Bind(GameObject owner)=>new(PersistentId??"",owner);
    private ParticleComponent(string id,GameObject owner){persistentId=id;context=owner;direct=null;}
    private GameObject? Resolve()=>direct??(context?.IsValid==true&&!string.IsNullOrEmpty(PersistentId)?context.FindById(PersistentId):null);
    /// <summary>引用目标仍存在且拥有原生粒子组件时为真。</summary>
    public bool IsValid=>Resolve() is {} o&&o.IsValid&&o.HasComponent(ComponentKind.Particle);
    /// <summary>在指定世界位置播放预设粒子组件，过期引用报告异常。</summary>
    /// <param name="position">世界坐标发射位置。</param>
    public void Play(Vector3 position){var o=Resolve()??throw new InvalidOperationException("Unassigned particle component");o.Request(new{op="particles",action="emit",entity=o.Entity,position=new[]{position.X,position.Y,position.Z}});}
    /// <summary>停止发射；存活粒子继续按生命周期消退。</summary>
    public void Stop(){var o=Resolve()??throw new InvalidOperationException("Unassigned particle component");o.Request(new{op="particles",action="stop",entity=o.Entity});}
}
public sealed partial class GameObject {
    /// <summary>查询此对象或预制体实例上的粒子组件；不存在返回null。</summary>
    public ParticleComponent? GetParticleComponent()=>HasComponent(ComponentKind.Particle)?new ParticleComponent(this):null;
    /// <summary>按持久ID或对象名称查找当前世界中的预制粒子组件。</summary>
    /// <param name="idOrName">场景或预制体实例对象的持久ID或名称。</param>
    public ParticleComponent? FindParticleComponent(string idOrName)=>(FindById(idOrName)??FindByName(idOrName))?.GetParticleComponent();
}
