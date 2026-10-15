using System.Numerics;
namespace StarOutpost;

public sealed class EnemyController:PooledActor
{
    private static readonly Vector3[] colors=[new(.8f,.18f,.28f),new(1,.35f,.08f),new(.9f,.1f,.55f),new(.15f,.9f,.35f),new(.7f,.2f,1),new(1,.15f,.15f)];
    private int tint=-1;
    protected override void Attach()=>Game.EnemySteps[Slot]=FixedStep;
    private void FixedStep(float dt)=>EnemyLogic.Tick(Game,Game.Enemies[Slot],dt);
    protected override void Detach()=>Game.EnemySteps[Slot]=null;
    protected override void Present()
    {
        var e=Game.Enemies[Slot];Show(InRun&&e.Active);if(!e.Active)return;
        float size=Simulation.Radius(e)*2;
        Place(e.Position,size*.5f,size*(e.HitFlash>0?1.15f:1),Quaternion.CreateFromAxisAngle(Vector3.UnitY,MathF.Atan2(e.Direction.X,e.Direction.Y)));
        int color=e.HitFlash>0?6:e.Kind;
        if(color!=tint){Object.SetTint(color==6?Vector3.One:colors[color],.4f);tint=color;}
    }
}
public sealed class TelegraphController:PooledActor
{
    protected override void Present()
    {
        var e=Game.Enemies[Slot];bool active=e.Active&&e.Phase==1&&e.Kind is 1 or 2 or 4 or 5;
        Show(InRun&&active);if(!active)return;
        var direction=e.Direction;float length=e.Kind==5?18:1.6f;
        var center=e.Kind==5?e.Position+direction*length*.5f:e.Position;
        Object.SetPose(new(center.X,.03f,center.Y),Quaternion.CreateFromAxisAngle(Vector3.UnitY,MathF.Atan2(direction.X,direction.Y)),new(e.Kind==5?.35f:1.6f,.03f,length));
    }
}
