using Atom;
using System.Numerics;
namespace Game;
public enum ProbeMode { First, Second }
public sealed class CompletionProbe:Behaviour {
    public Vector3 Offset=new(1,2,3);public ProbeMode Mode=ProbeMode.First;public float Speed=2;public bool CheckPhysics=true;
    private int frames;
    public bool FailRestore=false;
    public override string CaptureReloadState()=>frames.ToString();
    public override void RestoreReloadState(string state){if(FailRestore)throw new Exception("Deliberate preflight failure");frames=int.Parse(state);}
    public override void OnUpdate(float dt) {
        Object.UI.Progress=.5f;if(Object.UI.Progress!=.5f || !Object.ActiveInHierarchy)throw new Exception("Managed UI/state API failed");
        frames++;Object.Text=$"{frames}:{Speed}:{Offset.X}:{Mode}";
        if(!CheckPhysics)return;
        var hit=Object.Raycast(new(0,0,0),Vector3.UnitX,20,1u<<2);
        if(hit==null || MathF.Abs(hit.Value.Distance-4.5f)>.01f || hit.Value.Normal.X>-.9f)throw new Exception("Managed ray hit/normal/filter failed");
        if(Object.OverlapSphere(new(5,0,0),1,1u<<2).Length!=1 || Object.OverlapSphere(new(5,0,0),1,1u<<1).Length!=0)throw new Exception("Managed overlap filter failed");
    }
}
