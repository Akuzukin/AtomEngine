using Atom;
namespace Game;
/// <summary>游玩预览回归使用的对象查询、显隐及更新夹具，不包含示例游戏逻辑。</summary>
public sealed class RuntimeApiProbe : Behaviour
{
    private int frames;
    public override void OnCreate()
    {
        var target = Object.FindById("Player") ?? throw new Exception("FindById failed");
        if (!target.IsValid || Object.FindById("missing-id") != null) throw new Exception("Validity/query failed");
        if(Object.FindByName("Player")?.Entity!=target.Entity||Object.GetObject(target.Entity)?.Entity!=target.Entity||Object.GetObject(uint.MaxValue)!=null)throw new Exception("Native ECS object lookup failed");
        if(!target.HasComponent(ComponentKind.Mesh)||!Object.Query(ComponentKind.Mesh,false).Any(o=>o.Entity==target.Entity))throw new Exception("Native ECS component query failed");
        bool active=target.ActiveSelf;
        Object.SetActiveBatch([new(target,false)]);
        if(target.ActiveInHierarchy||Object.Query(ComponentKind.Mesh).Any(o=>o.Entity==target.Entity))throw new Exception("Native ECS batch deactivate failed");
        Object.SetActiveBatch([new(target,active)]);
        target.Visible = false;
        if (target.Visible) throw new Exception("Visibility hide failed");
        target.Visible = true;
        if (!target.Visible) throw new Exception("Visibility restore failed");
    }
    public override void OnUpdate(float dt) => Object.Text = $"Frames:{++frames}";
}
