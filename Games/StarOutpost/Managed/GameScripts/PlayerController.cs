using Atom;
using System.Numerics;
namespace StarOutpost;

public readonly record struct PlayerControls(Vector2 Movement,Vector2? Aim,bool Active,bool Detonate,bool UseSupply);
public sealed class PlayerController:Behaviour
{
    public static PlayerController? Current;
    private Simulation? game;
    private GameObject? cameraObject;
    private bool shown;
    public override void OnCreate(){Current=this;Object.Visible=false;cameraObject=Object.FindByName("Camera");if(cameraObject==null||!cameraObject.HasComponent(ComponentKind.Camera))throw new InvalidOperationException("Scene must contain Camera with a native camera component");}
    public override void OnUpdate(float dt)
    {
        if(game==null){if(OutpostRuntime.Current==null)return;game=OutpostRuntime.Current.Game;game.PlayerStep=FixedStep;}
        bool visible=game.State is Flow.Run or Flow.Upgrade or Flow.Pause or Flow.Supply or Flow.Result;
        if(visible!=shown){Object.Visible=visible;shown=visible;}
        if(visible)Object.SetPose(new(game.Player.X,.4f,game.Player.Y),Quaternion.CreateFromAxisAngle(Vector3.UnitY,MathF.Atan2(game.Aim.X,game.Aim.Y)),new(.44f));
    }
    private void FixedStep(float dt,Vector2 movement,Vector2? aim)=>PlayerLogic.Tick(game!,dt,movement,aim);
    public PlayerControls ReadControls()
    {
        var input=Object.Input;
        // +Y input maps to +Z world, which projects upward with the outpost camera.
        Vector2 movement=input.Movement(InputPreset.Wasd)+input.Movement(InputPreset.ArrowKeys);
        Vector2? aim=null;var pointer=Object.Pointer;var camera=cameraObject;
        if(pointer.Valid&&camera?.IsValid==true){var ndc=new Vector2(2*pointer.Position.X/pointer.ViewportSize.X-1,1-2*pointer.Position.Y/pointer.ViewportSize.Y);float aspect=pointer.ViewportSize.X/pointer.ViewportSize.Y;var local=Vector3.Normalize(new Vector3(ndc.X*aspect*MathF.Tan(55*MathF.PI/360),ndc.Y*MathF.Tan(55*MathF.PI/360),1));var ray=Vector3.Transform(local,camera.Rotation);var origin=camera.Position;if(ray.Y<-.001f){var target=origin+ray*((.4f-origin.Y)/ray.Y);aim=new Vector2(target.X,target.Z)-game!.Player;}}
        if(GameDiagnostics.Current?.Running==true){movement=new(MathF.Cos(game!.Elapsed),MathF.Sin(game.Elapsed));game.Invulnerable=1;}
        return new(movement,aim,input.WasPressed(KeyCode.Space),input.WasPressed(KeyCode.E),input.WasPressed(KeyCode.F));
    }
    public override void OnDestroy(){if(game!=null)game.PlayerStep=null;if(Current==this)Current=null;}
}
public sealed class CameraController:Behaviour
{
    public static CameraController? Current;
    public override void OnCreate()=>Current=this;
    public override void OnUpdate(float dt)
    {
        var game=OutpostRuntime.Current?.Game;if(game==null)return;
        bool inRun=game.State is Flow.Run or Flow.Upgrade or Flow.Pause or Flow.Supply or Flow.Result;
        var follow=inRun?game.Player*.45f:Vector2.Zero;float shake=game.Shake*MathF.Sin(game.Elapsed*113)*.1f;
        Object.Position=new(follow.X+shake,17,follow.Y-12+shake);
    }
    public override void OnDestroy(){if(Current==this)Current=null;}
}
