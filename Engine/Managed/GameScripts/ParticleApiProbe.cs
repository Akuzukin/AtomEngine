using Atom;
using System.Numerics;
namespace Game;
// Attached only by the native particle regression.
public sealed class ParticleApiProbe : Behaviour
{
    public ParticleComponent Effect;
    public override void OnCreate()
    {
        if (Effect.IsValid)
        {
            var queried=Object.FindParticleComponent(Effect.PersistentId) ?? throw new Exception("Particle query failed");
            if (queried.PersistentId!=Effect.PersistentId) throw new Exception("Queried component lost persistent ID");
            Effect=queried;
            Effect.Play(Vector3.Zero);
        }
    }
    public override void OnUpdate(float dt) => Object.Text = Effect.IsValid ? Effect.PersistentId : "unassigned";
}
