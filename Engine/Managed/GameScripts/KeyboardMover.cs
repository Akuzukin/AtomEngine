using Atom;
using System.Numerics;
namespace Game;

public sealed class KeyboardMover : Behaviour
{
    public float Speed = 4;
    public InputPreset Controls=InputPreset.Wasd;
    public override void OnFixedUpdate(float dt)
    {
        var velocity = Object.LinearVelocity;
        var move=Object.Input.Movement(Controls);
        velocity.X = move.X * Speed;
        velocity.Z = move.Y * Speed;
        Object.LinearVelocity = velocity;
    }
}
