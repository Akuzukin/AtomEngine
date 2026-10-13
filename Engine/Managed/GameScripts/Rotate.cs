using Atom;
using System.Numerics;
namespace Game;

public sealed class Rotate : Behaviour
{
    public float Speed = 0.5f;

    public void OnCreate() 
    { 

    }

    public void OnUpdate(float dt) 
    {
        Object.Rotation = Quaternion.Normalize(Object.Rotation * Quaternion.CreateFromAxisAngle(Vector3.UnitY, Speed * dt));
    }
    public void OnFixedUpdate(float dt) 
    { 

    }
    public void OnCollision(uint other, bool trigger)
    { 

    }

    public void OnUiClick() 
    { 

    }

    public void OnDestroy() 
    {

    }
}
