using System.Numerics;
namespace StarOutpost;
public static class PlayerLogic
{
    public static void Tick(Simulation game,float dt,Vector2 movement,Vector2? aim)
    {
        if(movement.LengthSquared()>1)movement=Vector2.Normalize(movement);
        game.Player=Vector2.Clamp(game.Player+movement*game.MoveSpeed*(game.TemporarySpeed>0?1.25f:1)*dt,-Rules.Bounds+new Vector2(.22f),Rules.Bounds-new Vector2(.22f));
        if(aim is {} a && a.LengthSquared()>.001f)game.Aim=Vector2.Normalize(a);
        else {var nearest=game.Nearest(game.Player);if(nearest!=null)game.Aim=Simulation.Normalize(nearest.Position-game.Player);}
    }
}
