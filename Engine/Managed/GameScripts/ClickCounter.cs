using Atom;
namespace Game;

public sealed class ClickCounter : Behaviour
{
    public int Count;
    public override void OnUiClick() { Object.Text = $"Clicks: {++Count}"; }
}
