using Atom;
using System.Numerics;
using System.Text.Json;
namespace Game;
// Only attached by the native gameplay-tool regression.
public sealed class GameplayToolsProbe : Behaviour
{
    public bool RetainForRestart;
    private static GameObject? retained;
    public override void OnDestroy() { retained = RetainForRestart ? Object : null; }
    private SpawnRequest? created, prefab, missing, replacement;
    private GameObject? createdObject, prefabObject;
    private int stage;
    public override void OnCreate()
    {
        if (retained != null && retained.IsValid) throw new Exception("Old World reference revived after restart");
        retained = null;
        var q = Mathf.EulerDegrees(new Vector3(20,35,-10));
        var rotated = Mathf.Rotate(q, Vector3.UnitZ);
        if (Mathf.Acos(float.NaN) != Mathf.PI || Mathf.Asin(float.NaN) != -Mathf.HalfPI ||
            Mathf.Min(2, float.NaN) != 2 || Mathf.NormalizeOrZero(Vector3.Zero) != Vector3.Zero ||
            Mathf.Cross(Vector3.Zero, new Vector3(float.NaN, 1, 2)) != Vector3.Zero ||
            !float.IsPositiveInfinity(Mathf.Length(new Vector3(float.PositiveInfinity, float.NaN, 0))))
            throw new Exception("Math edge semantics differ from engine");
        Object.Text = JsonSerializer.Serialize(new[] { Mathf.PI, Mathf.TwoPI, Mathf.Deg2Rad,
            Mathf.Sin(.4f), Mathf.Cos(.4f), Mathf.Tan(.4f), Mathf.Acos(2), Mathf.Asin(-2),
            Mathf.Atan2(.5f,-.25f), Mathf.Lerp(2,4,1.5f), Mathf.SmoothStep(-1,3,1),
            Mathf.Normalize(Vector3.Zero).X, Mathf.Normalize(new Vector3(3,0,4)).Z,
            Mathf.Dot(new Vector3(1,2,3),new Vector3(4,5,6)), Mathf.Cross(Vector3.UnitX,Vector3.UnitY).Z,
            Mathf.Floor(-1.2f), Mathf.Equal(1,1.0000001f) ? 1f : 0f,
            q.X,q.Y,q.Z,q.W,rotated.X,rotated.Y,
            Mathf.Slerp(Quaternion.Identity,Mathf.AxisAngle(Vector3.UnitY,2),.3f).Y,
            Mathf.Normalize(new Vector3(3e20f,0,4e20f)).Z, Mathf.Distance(new Vector3(1e20f,0,0),Vector3.Zero) });
        if (!Object.PlayAudio() || !Object.PlayAudio()) throw new Exception("Configured MP3 playback failed");
        created = Object.Create("ScriptCreated", new Vector3(3,2,1));
        prefab = Object.InstantiatePrefab("Asset/Prefabs/PhysicsCube.prefab", new Vector3(0,4,0));
        missing = Object.InstantiatePrefab("Asset/Prefabs/MissingGameplayFixture.prefab", Vector3.Zero);
    }
    public override void OnUpdate(float dt)
    {
        if (stage == 0 && created!.IsCompleted && prefab!.IsCompleted && missing!.IsCompleted)
        {
            if (!missing.Failed) throw new Exception("Missing prefab succeeded");
            try { _ = missing.Result; throw new Exception("Failed spawn did not throw"); }
            catch (InvalidOperationException) { }
            createdObject = created.Result!; prefabObject = prefab.Result!;
            if (!createdObject.IsValid || !prefabObject.IsValid || createdObject.Position != new Vector3(3,2,1) || prefabObject.Position != new Vector3(0,4,0))
                throw new Exception("Deferred spawn result/position failed");
            prefabObject.Visible=false;
            if (prefabObject.Visible) throw new Exception("Spawned prefab visibility failed");
            if (!createdObject.Destroy() || !prefabObject.Destroy() || !createdObject.Destroy()) throw new Exception("Deferred destroy rejected");
            replacement = Object.Create("Replacement", Vector3.Zero);
            stage=1;
        }
        else if (stage == 1 && replacement!.IsCompleted)
        {
            if (createdObject!.IsValid || prefabObject!.IsValid) throw new Exception("Destroyed references remained valid");
            try { _ = createdObject.Position; throw new Exception("Destroyed object access accepted"); }
            catch (InvalidOperationException) { }
            replacement.Result!.Destroy(); Object.StopAudio(); Object.Position = new Vector3(9,0,0); stage=2;
        }
    }
}
