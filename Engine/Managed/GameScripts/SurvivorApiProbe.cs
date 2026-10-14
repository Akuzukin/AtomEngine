using Atom;
namespace Game;
// Exercised only by --play-workflow-smoke-test.
public sealed class SurvivorApiProbe : Behaviour
{
    public override void OnCreate()
    {
        var player = Object.FindById("Player") ?? throw new Exception("FindById failed");
        if (!player.IsValid || Object.FindById("missing-id") != null) throw new Exception("Validity/query failed");
        player.Visible = false;
        if (player.Visible) throw new Exception("Visibility hide failed");
        player.Visible = true;
        if (!player.Visible) throw new Exception("Visibility restore failed");
    }
}
