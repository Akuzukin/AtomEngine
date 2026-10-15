namespace StarOutpost;
public static class EffectLogic
{
    public static void Tick(Fx p,float dt){p.Life-=dt;p.Position+=p.Velocity*dt;p.Velocity*=.94f;if(p.Life<=0)p.Active=false;}
    public static void TickNumber(FloatingDamage n,float dt){n.Life-=dt;n.Position.Y-=dt*.3f;}
}
