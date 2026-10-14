using Atom;
using Game.Assets;
using System.Numerics;
namespace Game;
// Attached by the native regression only.
public sealed class TypedApiProbe:Behaviour
{
    public KeyCode FireKey=KeyCode.Space;
    public InputPreset Controls=InputPreset.Wasd;
    public AudioClip Clip=AudioAssets.SeMp3;
    private int ticks;
    public override void OnCreate(){if(!Object.PlayAudio(Clip,.1f))throw new Exception("Typed audio native playback failed");}
    public override void OnUpdate(float dt)
    {
        if(Object.Input.IsDown(FireKey)||Object.Input.WasPressed(FireKey)||Object.Input.WasReleased(FireKey)||
           Object.Input.IsDown(MouseButton.Left)||Object.Input.WasPressed(GamepadButton.A)||Object.Input.WasReleased(GamepadButton.B)||Object.Input.Movement(Controls)!=Vector2.Zero)
            throw new Exception("Typed input bypassed gameplay focus gate");
        ticks++;Object.Text=$"{ticks}:{FireKey}:{Controls}:{Clip.Path}";
    }
    public override void OnDestroy()=>Object.StopAudio();
}
