using System.Numerics;
namespace StarOutpost;
public static class DropLogic
{
    public static void Tick(Simulation game,Drop d,float dt)
    {
d.Life-=dt;var delta=game.Player-d.Position;float distance=delta.Length();if(distance<game.Magnet || distance<.4f && d.Gold>0)d.Position+=Simulation.Normalize(delta)*Math.Min(distance,dt*(4+10/(distance+.2f)));if(Vector2.DistanceSquared(game.Player,d.Position)<.09f){game.Exp+=d.Exp;game.RunGold+=d.Gold;game.RunStars+=d.Stars;game.TotalEarned+=d.Gold;d.Active=false;}else if(d.Life<=0){/* Keep finite pools: expired rewards go to run wallet, XP remains a movement reward. */game.RunGold+=d.Gold;game.RunStars+=d.Stars;game.TotalEarned+=d.Gold;d.Active=false;}
    }
}
