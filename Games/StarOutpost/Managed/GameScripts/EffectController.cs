using Atom;
using System.Numerics;
namespace StarOutpost;
public sealed class EffectSpawner:Behaviour
{
    public ParticleComponent HitEffect,UpgradeEffect,DeathEffect,HealEffect;
    private readonly ParticleComponent?[] gpuEmitters=new ParticleComponent?[4];
    private Simulation? game;private ActorPool? effects,corpses,numbers;
    public static EffectSpawner? Current;
    public int NativeEmitterCount=>gpuEmitters.Count(e=>e!=null);
    public override void OnCreate()=>Current=this;
    public override void OnUpdate(float dt)
    {
        if(game==null){if(OutpostRuntime.Current==null)return;game=OutpostRuntime.Current.Game;for(int i=0;i<4;i++){var assigned=new[]{HitEffect,UpgradeEffect,DeathEffect,HealEffect}[i];gpuEmitters[i]=assigned.IsValid?assigned:Object.FindParticleComponent("BurstEmitter"+i);}game.BurstEffect=TryEmit;effects=new(Object,game,"Asset/Prefabs/Effect.prefab",Rules.ParticleCap);corpses=new(Object,game,"Asset/Prefabs/Corpse.prefab",256);numbers=new(Object,game,"Asset/Prefabs/DamageNumber.prefab",64);}
        effects!.Sync(i=>game.Particles[i].Active,8);corpses!.Sync(i=>i<game.Corpses.Count);numbers!.Sync(i=>game.Numbers[i].Life>0);
    }
    private bool TryEmit(Vector2 position,int kind)
    {
        int index=Math.Clamp(kind,0,3);var emitter=gpuEmitters[index];
        if(emitter?.IsValid!=true)return false;
        try {emitter.Value.Play(new(position.X,.5f,position.Y));return true;}
        catch(InvalidOperationException error){gpuEmitters[index]=null;game!.Say("粒子效果不可用，已回退显示："+error.Message);return false;}
    }
    public override void OnDestroy(){if(game!=null)game.BurstEffect=null;if(Current==this)Current=null;}
}
public sealed class EffectController:PooledActor
{
    private int tint=-1;
    protected override void Attach()=>Game.EffectSteps[Slot]=FixedStep;
    private void FixedStep(float dt)=>EffectLogic.Tick(Game.Particles[Slot],dt);
    protected override void Detach()=>Game.EffectSteps[Slot]=null;
    protected override void Present(){var p=Game.Particles[Slot];Show(InRun&&p.Active);if(!p.Active)return;Place(p.Position,.55f,p.Size*Math.Max(.1f,p.Life/p.MaxLife));if(tint!=p.Kind){Object.SetTint(p.Kind==0?Vector3.One:p.Kind==1?new(.2f,.7f,1):p.Kind==2?new(1,.4f,.1f):new(.1f,1,.3f),1);tint=p.Kind;}}
}
public sealed class CorpseController:PooledActor
{
    protected override void Present(){bool active=Slot<Game.Corpses.Count;Show(InRun&&active);if(!active)return;var p=Game.Corpses[Slot].Position;Object.SetPose(new(p.X,.025f,p.Y),Quaternion.Identity,new(.25f,.025f,.25f));}
}
public sealed class DamageNumberController:PooledActor
{
    private int id=-1;
    protected override void Present()
    {
        var n=Game.Numbers[Slot];var camera=CameraController.Current;
        if(camera==null){Show(false);return;}
        var v=Vector3.Transform(new Vector3(n.Position.X,.8f+(.65f-n.Life),n.Position.Y)-camera.Object.Position,Quaternion.Inverse(camera.Object.Rotation));
        bool visible=InRun&&n.Life>0&&v.Z>.1f;Show(visible);if(!visible)return;
        var pointer=Object.Pointer;float w=pointer.ViewportSize.X,h=pointer.ViewportSize.Y,unit=Math.Min(w/1920,h/1080),tan=MathF.Tan(55*MathF.PI/360);
        Object.UI.Position=new(v.X/(v.Z*tan*w/h)*w/(2*unit),-v.Y/(v.Z*tan)*h/(2*unit));if(id!=n.Id){Object.Text=n.Amount.ToString();id=n.Id;}
    }
}
public sealed class SupplyController:Behaviour
{
    private bool shown;
    public override void OnCreate()=>Object.Visible=false;
    public override void OnUpdate(float dt){var g=OutpostRuntime.Current?.Game;if(g==null)return;bool visible=g.SupplyAvailable&&g.State is Flow.Run or Flow.Upgrade or Flow.Pause or Flow.Supply;if(visible!=shown){Object.Visible=visible;shown=visible;}if(visible)Object.SetPose(new(g.SupplyPosition.X,.35f,g.SupplyPosition.Y),Quaternion.Identity,new(.7f));}
}
