using Atom;
namespace StarOutpost;
public sealed class ProjectileSpawner:Behaviour
{
    private Simulation? game;private ActorPool? pool;
    public override void OnUpdate(float dt){if(game==null){if(OutpostRuntime.Current==null)return;game=OutpostRuntime.Current.Game;pool=new(Object,game,"Asset/Prefabs/Projectile.prefab",Rules.BulletCap);}pool!.Sync(i=>game.Bullets[i].Active,4);}
}
public sealed class ProjectileController:PooledActor
{
    protected override void Attach()=>Game.ProjectileSteps[Slot]=FixedStep;
    private void FixedStep(float dt)=>ProjectileLogic.Tick(Game,Game.Bullets[Slot],dt);
    protected override void Detach()=>Game.ProjectileSteps[Slot]=null;
    protected override void Present(){var b=Game.Bullets[Slot];Show(InRun&&b.Active);if(b.Active)Place(b.Position,.4f,.12f);}
}
