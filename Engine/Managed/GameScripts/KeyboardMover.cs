using Atom;
using System.Numerics;
namespace Game;

public sealed class KeyboardMover : Behaviour
{
    public float Speed = 4;
    public override void OnFixedUpdate(float dt)
    {
        var velocity = Object.LinearVelocity;
        velocity.X = ((Object.IsKeyDown(0x44) ? 1 : 0) - (Object.IsKeyDown(0x41) ? 1 : 0) + Object.GamepadAxis(0)) * Speed;
        velocity.Z = ((Object.IsKeyDown(0x57) ? 1 : 0) - (Object.IsKeyDown(0x53) ? 1 : 0) + Object.GamepadAxis(1)) * Speed;
        Object.LinearVelocity = velocity;
    }
}
