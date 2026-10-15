using System.Numerics;
namespace StarOutpost;
public sealed class WeaponLogic(int index)
{
    private float cooldown;
    private readonly Dictionary<int,float> orbitalHits=new();
    public void Reset(){cooldown=0;orbitalHits.Clear();}
    public void Forget(int id)=>orbitalHits.Remove(id);
    private float WeaponDamage(Simulation game,int i)=>Rules.Weapons[i].Damage*(1+.2f*(game.Weapons[i]-1))*game.DamageMultiplier*(game.Evolution[i]>0?2:1);
    public void Tick(Simulation game,float dt)
    {
        int i=index;
        if(game.Weapons[i]>0){cooldown-=dt;if(i==1){TickMoon(game,dt);return;}if(cooldown<=0){cooldown+=1/(Rules.Weapons[i].Rate*game.AttackMultiplier*(game.Evolution[i]==2?1.5f:1));
            if(i==0||i==2||i==4){int count=Rules.Weapons[i].Count+(i==0&&game.Evolution[i]==1?1:0)+(i==2&&game.Evolution[i]==1?2:0)+(i==4&&game.Evolution[i]==1?2:0);for(int k=0;k<count;k++){float angle=MathF.Atan2(game.Aim.Y,game.Aim.X)+(k-(count-1)*.5f)*Rules.Weapons[i].Spread;Shoot(game,i,new(MathF.Cos(angle),MathF.Sin(angle)));}}
            if(i==3){var end=game.Player+game.Aim*16;int remaining=game.Evolution[3]==1?20:4;foreach(var e in game.Enemies.Where(e=>game.IsTargetable(e)).OrderBy(e=>Vector2.DistanceSquared(game.Player,e.Position)))if(Simulation.SegmentDistance(e.Position,game.Player,end)<(game.Evolution[3]==2?.6f:.16f)+Simulation.Radius(e)){game.Hit(e,WeaponDamage(game,3),game.Aim);if(--remaining<=0)break;}for(int k=0;k<22;k++)game.Effect(game.Player+game.Aim*k*.45f,Vector2.Zero,.16f,.12f,1);}
            if(i==5){game.BlackHole=game.Nearest(game.Player)?.Position??game.Player+game.Aim*2;game.BlackHoleLife=3;}
            if(i==6){float range=game.Evolution[6]==1?4:2.6f;game.Area(game.Player,range,WeaponDamage(game,6));if(game.Evolution[6]==2){var target=game.Nearest(game.Player);if(target!=null)game.Area(target.Position,2,WeaponDamage(game,6)*.5f);}game.Ring(game.Player,range,1);game.Sound?.Invoke("storm");}
        }}
        if(index==5 && game.BlackHoleLife>0){game.BlackHoleLife-=dt;float range=game.Evolution[5]==1?3:1.8f;foreach(var e in game.Enemies)if(game.IsTargetable(e)&&Vector2.Distance(e.Position,game.BlackHole)<range){e.Position+=Simulation.Normalize(game.BlackHole-e.Position)*dt*1.8f;game.Hit(e,WeaponDamage(game,5)*dt,Vector2.Zero,false);}}
    }
    private void Shoot(Simulation game,int weapon,Vector2 direction){var b=game.Bullets.FirstOrDefault(x=>!x.Active);if(b==null)return;b.Active=true;b.Position=game.Player;b.Velocity=direction*Rules.Weapons[weapon].Speed;b.Damage=WeaponDamage(game,weapon);b.Life=weapon==2&&game.Evolution[2]!=2?.65f:1.8f;b.Pierce=weapon==0&&game.Evolution[0]==2?5:Rules.Weapons[weapon].Pierce;b.Weapon=weapon;b.Hit.Clear();game.ShotCount++;if(weapon==0 && game.ShotCount%3==0)game.Sound?.Invoke("shot");}
    private void TickMoon(Simulation game,float dt)
    {
        if(game.Weapons[1]>0){int count=game.Evolution[1]==1?5:2;foreach(var e in game.Enemies)if(game.IsTargetable(e) && (!orbitalHits.TryGetValue(e.Id,out float last)||game.Elapsed-last>=.5f/game.AttackMultiplier))for(int k=0;k<count;k++){float a=game.Elapsed*MathF.Tau*2*(game.Evolution[1]==2?1.5f:1)+MathF.Tau*k/count;var p=game.Player+new Vector2(MathF.Cos(a),MathF.Sin(a))*1.1f;if(Vector2.Distance(e.Position,p)<Simulation.Radius(e)+.38f){game.Hit(e,WeaponDamage(game,1),e.Position-game.Player);orbitalHits[e.Id]=game.Elapsed;break;}}}
    }
}
