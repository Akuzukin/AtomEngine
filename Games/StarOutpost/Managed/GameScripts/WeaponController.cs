using Atom;
using System.Numerics;
namespace StarOutpost;
public sealed class WeaponController:Behaviour
{
    public int WeaponIndex;
    private Simulation? game;private ActorPool? blades;
    public override void OnUpdate(float dt)
    {
        if(game==null){if(OutpostRuntime.Current==null)return;game=OutpostRuntime.Current.Game;game.WeaponSteps[WeaponIndex]=FixedStep;if(WeaponIndex==1)blades=new(Object,game,"Asset/Prefabs/OrbitBlade.prefab",5);}
        if(blades!=null)blades.Sync(i=>game.Weapons[1]>0&&i<(game.Evolution[1]==1?5:2));
    }
    private void FixedStep(float dt)=>game!.WeaponSystems[WeaponIndex].Tick(game,dt);
    public override void OnDestroy(){if(game!=null)game.WeaponSteps[WeaponIndex]=null;}
}
public sealed class OrbitBladeController:PooledActor
{
    protected override void Present(){int count=Game.Weapons[1]>0?(Game.Evolution[1]==1?5:2):0;Show(InRun&&Slot<count);if(Slot>=count)return;float angle=Game.Elapsed*MathF.Tau*2*(Game.Evolution[1]==2?1.5f:1)+MathF.Tau*Slot/count;Place(Game.Player+new Vector2(MathF.Cos(angle),MathF.Sin(angle))*1.1f,.5f,.24f);}
}
public sealed class BlackHoleController:Behaviour
{
    private bool shown;
    public override void OnCreate()=>Object.Visible=false;
    public override void OnUpdate(float dt){var g=OutpostRuntime.Current?.Game;if(g==null)return;bool visible=g.BlackHoleLife>0&&g.State is Flow.Run or Flow.Upgrade or Flow.Pause or Flow.Supply;if(visible!=shown){Object.Visible=visible;shown=visible;}if(visible)Object.SetPose(new(g.BlackHole.X,.08f,g.BlackHole.Y),Quaternion.Identity,new(g.Evolution[5]==1?3:1.8f));}
}
