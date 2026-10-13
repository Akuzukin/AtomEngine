using Atom;
using System.Numerics;
namespace Game;

public sealed class Rotate : Behaviour
{
    public float Speed = 0.5f;

    public override void OnCreate() 
    { 

    }

    public override void OnUpdate(float dt) 
    {
        Object.Rotation = Quaternion.Normalize(Object.Rotation * Quaternion.CreateFromAxisAngle(Vector3.UnitY, Speed * dt));
    }
    public override void OnFixedUpdate(float dt) 
    { 

    }
    public overridevoid OnCollision(uint other, bool trigger)
    { 

    }

    public override void OnUiClick() 
    { 

    }

    public override void OnDestroy() 
    {

    }
}
