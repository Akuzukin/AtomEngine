using Atom;
using System.Numerics;
namespace StarOutpost;

internal readonly record struct ActorBinding(Simulation Game,int Slot);
internal static class ActorBindings
{
    internal static readonly Dictionary<uint,ActorBinding> Pending=new();
    internal static void Clear()=>Pending.Clear();
}

// Empty at startup. Prefab instances are allocated only when a live model needs one.
internal sealed class ActorPool(GameObject owner,Simulation game,string prefab,int capacity)
{
    private readonly GameObject?[] objects=new GameObject?[capacity];
    private readonly SpawnRequest?[] requests=new SpawnRequest?[capacity];
    private readonly bool[] enabled=new bool[capacity];
    private readonly List<ObjectActivation> activationChanges=new();
    private readonly List<int> changedSlots=new();
    public int Created=>objects.Count(o=>o!=null);
    public int Pending=>requests.Count(r=>r!=null);
    public void Sync(Func<int,bool> needsActor,int budget=2)
    {
        activationChanges.Clear();changedSlots.Clear();
        bool inRun=game.State is Flow.Run or Flow.Upgrade or Flow.Pause or Flow.Supply or Flow.Result;
        for(int i=0;i<capacity;i++)
        {
            if(requests[i] is {} request && request.IsCompleted)
            {
                requests[i]=null;
                if(request.Failed){game.Say("预制体生成失败："+prefab);continue;}
                objects[i]=request.Result!;enabled[i]=true;
                ActorBindings.Pending[objects[i]!.Entity]=new(game,i);
            }
            bool needed=inRun&&needsActor(i);
            if(objects[i] is {} actor){if(enabled[i]!=needed){activationChanges.Add(new(actor,needed));changedSlots.Add(i);}continue;}
            if(requests[i]!=null||!needed||budget<=0)continue;
            requests[i]=owner.InstantiatePrefab(prefab,new Vector3(0,-100,0));budget--;
        }
        // Deaths, reuse and menu transitions write native ECS ObjectState in one call per pool.
        owner.SetActiveBatch(activationChanges);
        for(int i=0;i<changedSlots.Count;i++)enabled[changedSlots[i]]=activationChanges[i].Active;
    }
}

public abstract class PooledActor:Behaviour
{
    protected Simulation Game=null!;
    protected int Slot;
    protected bool Bound;
    private bool visible;
    private float presentedTime=-1;
    private Flow presentedState;
    public override void OnCreate()=>Object.Visible=false;
    public override void OnUpdate(float dt)
    {
        if(!Bound)
        {
            if(!ActorBindings.Pending.Remove(Object.Entity,out var binding))return;
            Game=binding.Game;Slot=binding.Slot;Bound=true;Attach();
        }
        if(presentedTime==Game.Elapsed&&presentedState==Game.State)return;
        presentedTime=Game.Elapsed;presentedState=Game.State;Present();
    }
    protected virtual void Attach(){}
    protected abstract void Present();
    protected bool InRun=>Game.State is Flow.Run or Flow.Upgrade or Flow.Pause or Flow.Supply or Flow.Result;
    protected void Show(bool value){if(visible==value)return;Object.Visible=value;visible=value;}
    protected void Place(Vector2 p,float y,float size,Quaternion? rotation=null)=>Object.SetPose(new(p.X,y,p.Y),rotation??Quaternion.Identity,new(size));
    public override void OnDestroy(){ActorBindings.Pending.Remove(Object.Entity);if(Bound)Detach();}
    protected virtual void Detach(){}
}
