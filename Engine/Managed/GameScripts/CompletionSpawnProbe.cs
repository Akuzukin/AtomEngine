using Atom;
using System.Numerics;
namespace Game;

public sealed class CompletionSpawnProbe : Behaviour
{
    private SpawnRequest? created, applied; private GameObject? target; private uint entity; private int stage;
    public override void OnCreate() { created = Object.Create("Script prefab target", new(3, 2, 1)); }
    public override void OnUpdate(float dt)
    {
        if (stage == 0 && created!.IsCompleted) { target = created.Result!; entity = target.Entity; applied = target.ApplyPrefab("Asset/Prefabs/PhysicsCube.prefab"); stage = 1; }
        else if (stage == 1 && applied!.IsCompleted) { if (applied.Failed || applied.Result!.Entity != entity || target!.Position != new Vector3(3, 2, 1)) throw new Exception("ApplyPrefab changed handle or pose"); target.LinearVelocity = Vector3.Zero; Object.Text = "attached"; stage = 2; }
    }
}
