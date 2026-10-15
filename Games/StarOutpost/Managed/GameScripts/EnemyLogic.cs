using System.Numerics;
namespace StarOutpost;
public static class EnemyLogic
{
    public static void Tick(Simulation game,Enemy e,float dt)
    {
e.HealTimer-=dt;e.HitFlash=Math.Max(0,e.HitFlash-dt);e.Timer-=dt;e.AttackTimer-=dt;var delta=game.Player-e.Position;float distance=delta.Length();var direction=Simulation.Normalize(delta);
            if(e.Kind==1){if(e.Phase==0){e.Position+=direction*Rules.Enemies[1].Speed*dt;if(distance<4 && e.Timer<=0){e.Phase=1;e.Timer=1;e.Direction=direction;game.Sound?.Invoke("charge");}}
                else if(e.Phase==1){if(e.Timer<=0){e.Phase=2;e.Timer=.45f;}}
                else if(e.Phase==2){e.Position+=e.Direction*9*dt;if(e.Timer<=0){e.Phase=3;e.Timer=.6f;}}
                else if(e.Timer<=0){e.Phase=0;e.Timer=2;}}
            else if(e.Kind>=4){if(e.Phase==0){e.Position+=direction*Rules.Enemies[e.Kind].Speed*dt;if(e.Timer<=0){e.Phase=1;e.Timer=1;e.Direction=direction;}}
                else if(e.Timer<=0){if(e.Kind==5){if(Simulation.SegmentDistance(game.Player,e.Position,e.Position+e.Direction*18)<.55f)game.Hurt(32,5);game.Ring(e.Position,2.5f,2);if(distance<2.5f)game.Hurt(20,5);}else{if(distance<2.4f)game.Hurt(20,4);game.Ring(e.Position,2.4f,2);}e.Phase=0;e.Timer=e.Kind==5?(e.Hp<e.MaxHp*.5f?1.5f:3):2.5f;}}
            else if(e.Kind==2 && distance<.9f){if(e.Phase==0){e.Phase=1;e.Timer=.8f;}else if(e.Timer<=0){if(distance<1.25f)game.Hurt(18,2);e.Hp=0;}}
            else e.Position+=direction*Rules.Enemies[e.Kind].Speed*dt;
            e.Position=Vector2.Clamp(e.Position,-Rules.Bounds,Rules.Bounds);
            if(e.Kind==3 && e.HealTimer<=0){foreach(var other in game.Enemies)if(other.Active&&other!=e&&Vector2.DistanceSquared(e.Position,other.Position)<4)other.Hp=Math.Min(other.MaxHp,other.Hp+2);game.Ring(e.Position,1,3);}
            if(distance<Simulation.Radius(e)+.22f && e.AttackTimer<=0){game.Hurt(e.Kind>=4?20:8,e.Kind);e.AttackTimer=.7f;}
        
        if(e.HealTimer<=0)e.HealTimer=1;
    }
}
