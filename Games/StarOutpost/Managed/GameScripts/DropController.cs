using Atom;
using System.Numerics;
namespace StarOutpost;
public sealed class DropSpawner:Behaviour
{
    private Simulation? game;private ActorPool? pool;
    public override void OnUpdate(float dt){if(game==null){if(OutpostRuntime.Current==null)return;game=OutpostRuntime.Current.Game;pool=new(Object,game,"Asset/Prefabs/Drop.prefab",Rules.DropCap);}pool!.Sync(i=>game.Drops[i].Active,4);}
}
public sealed class DropController:PooledActor
{
    protected override void Attach()=>Game.DropSteps[Slot]=FixedStep;
    private void FixedStep(float dt)=>DropLogic.Tick(Game,Game.Drops[Slot],dt);
    protected override void Detach()=>Game.DropSteps[Slot]=null;
    protected override void Present(){var d=Game.Drops[Slot];Show(InRun&&d.Active);if(d.Active)Place(d.Position,.18f,d.Stars>0?.2f:.1f,Quaternion.CreateFromAxisAngle(Vector3.UnitY,Game.Elapsed*2));}
}
