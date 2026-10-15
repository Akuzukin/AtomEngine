using System.Numerics;
namespace StarOutpost;
public static class ProjectileLogic
{
    public static void Tick(Simulation game,Bullet b,float dt)
    {
var previous=b.Position;b.Life-=dt;if(b.Weapon==4){var target=game.Nearest(b.Position);if(target!=null)b.Velocity=Simulation.Normalize(target.Position-b.Position)*Rules.Weapons[4].Speed;}b.Position+=b.Velocity*dt;
            foreach(var e in game.Enemies)if(game.IsTargetable(e) && !b.Hit.Contains(e.Id) && Simulation.SegmentDistance(e.Position,previous,b.Position)<Simulation.Radius(e)+.08f){b.Hit.Add(e.Id);game.Hit(e,b.Damage,b.Velocity);if(--b.Pierce<=0){b.Active=false;break;}}
            if(b.Life<=0||Math.Abs(b.Position.X)>15||Math.Abs(b.Position.Y)>11)b.Active=false;
        
    }
}
