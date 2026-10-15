using System.Numerics;
namespace StarOutpost;

// Engine-independent combat rules. Rendering never owns damage or currency settlement.
public sealed class Simulation
{
    public Profile Profile;
    public Flow State=Flow.Menu;
    public readonly Enemy[] Enemies=Enumerable.Range(0,Rules.EnemyCap).Select(i=>new Enemy{Slot=i}).ToArray();
    public readonly Bullet[] Bullets=Enumerable.Range(0,Rules.BulletCap).Select(_=>new Bullet()).ToArray();
    public readonly Drop[] Drops=Enumerable.Range(0,Rules.DropCap).Select(_=>new Drop()).ToArray();
    public readonly Fx[] Particles=Enumerable.Range(0,Rules.ParticleCap).Select(_=>new Fx()).ToArray();
    public readonly List<(Vector2 Position,float Time)> Corpses=new();
    public readonly FloatingDamage[] Numbers=Enumerable.Range(0,64).Select(_=>new FloatingDamage()).ToArray();
    // Native scripts register their own fixed-step behavior; tests use the same pure rules.
    public bool NativeActors;
    public Action<float,Vector2,Vector2?>? PlayerStep;
    public Action<float>? SpawnerStep;
    public readonly Action<float>?[] WeaponSteps=new Action<float>?[7];
    public readonly Action<float>?[] EnemySteps=new Action<float>?[Rules.EnemyCap];
    public readonly Action<float>?[] ProjectileSteps=new Action<float>?[Rules.BulletCap];
    public readonly Action<float>?[] DropSteps=new Action<float>?[Rules.DropCap];
    public readonly Action<float>?[] EffectSteps=new Action<float>?[Rules.ParticleCap];
    public readonly WaveScheduler Waves=new();
    public readonly WeaponLogic[] WeaponSystems=Enumerable.Range(0,7).Select(i=>new WeaponLogic(i)).ToArray();
    public double NextRandom()=>random.NextDouble();
    public bool IsTargetable(Enemy e)=>e.Active&&(!NativeActors||EnemySteps[e.Slot]!=null);
    public int[] Weapons=new int[7],Evolution=new int[7],Passives=new int[7];
    public int[] ChosenRarities=new int[4],CandidateRarities=new int[4];
    public float[] DamageSources=new float[6];
    public Offer[] Choices=[];
    public Vector2 Player,Aim=Vector2.UnitY,SupplyPosition,BlackHole;
    public float Elapsed,Duration=1200,Hp,MaxHp,Energy,Cooldown,Invulnerable,DamageMultiplier,AttackMultiplier,MoveSpeed,Magnet,Armor,Shake,BlackHoleLife,TemporarySpeed;
    public int Level,Exp,Kills,RunGold,RunStars,TotalEarned,ShotCount;
    public bool Won,SupplyAvailable,Shield;
    public string Notice="",LastDeath="";
    public float NoticeTime;
    public Action? Persist;
    public Action<string>? Sound;
    public Func<Vector2,int,bool>? BurstEffect;
    private Random random;
    private float accumulator;
    private int serial; private long born;private bool settled;
    private bool queuedActive,queuedDetonate;
    public Simulation(Profile? profile=null,int seed=42){Profile=profile??new();random=new(seed);}
    public void Say(string text){Notice=text;NoticeTime=3;}
    public void Start()
    {
        foreach(var e in Enemies)e.Active=false;foreach(var b in Bullets)b.Active=false;foreach(var d in Drops)d.Active=false;foreach(var p in Particles)p.Active=false;
        Corpses.Clear();foreach(var weapon in WeaponSystems)weapon.Reset();Waves.Reset();foreach(var n in Numbers)n.Life=0;Array.Clear(Weapons);Array.Clear(Evolution);Array.Clear(Passives);Array.Clear(DamageSources);Array.Clear(ChosenRarities);Array.Clear(CandidateRarities);
        Weapons[Profile.Character==1?1:0]=1;Player=Vector2.Zero;Aim=Vector2.UnitY;
        MaxHp=100+20*Profile.Meta[1];Hp=MaxHp;DamageMultiplier=1+.1f*Profile.Meta[0];AttackMultiplier=1+.08f*Profile.Meta[2];MoveSpeed=3.2f*(Profile.Character==1?1.15f:1);Magnet=1.2f*(1+.2f*Profile.Meta[3]);Armor=.04f*Profile.Meta[4];
        Elapsed=Cooldown=Invulnerable=Shake=BlackHoleLife=accumulator=TemporarySpeed=0;
        Level=1;Exp=Kills=RunGold=RunStars=TotalEarned=ShotCount=0;Energy=0;settled=Won=SupplyAvailable=queuedActive=queuedDetonate=false;
        Shield=Profile.Consumables[1]>0;if(Shield)Profile.Consumables[1]--;
        if(Profile.Consumables[2]>0){Profile.Consumables[2]--;TemporarySpeed=300;}
        if(Profile.Consumables[0]>0){Profile.Consumables[0]--;Say("携带恢复剂：首次生命低于一半时自动恢复 40");carriedPotion=true;}else carriedPotion=false;
        State=Flow.Run;Persist?.Invoke();Say("裂谷站台 · 生存 20 分钟，自动开火");
    }
    private bool carriedPotion;
    public void Advance(float frameDt,Vector2 movement,Vector2? aim=null,bool active=false,bool detonate=false)
    {
        if(State!=Flow.Run)return;
        queuedActive|=active;queuedDetonate|=detonate;
        accumulator+=Math.Clamp(frameDt,0,.25f);
        const float step=1f/60;
        while(accumulator>=step && State==Flow.Run){accumulator-=step;Tick(step,movement,aim,queuedActive,queuedDetonate);queuedActive=queuedDetonate=false;}
        // A modal opens at the exact simulation step; retain the fractional remainder.
    }
    public void Tick(float dt,Vector2 movement,Vector2? aim=null,bool active=false,bool detonate=false)
    {
        if(State!=Flow.Run)return;
        NoticeTime-=dt;Shake=Math.Max(0,Shake-dt*2);Cooldown=Math.Max(0,Cooldown-dt);Invulnerable=Math.Max(0,Invulnerable-dt);TemporarySpeed=Math.Max(0,TemporarySpeed-dt);
        // Combat feedback is visual only; movement, waves and cooldowns share the fixed clock.
        Elapsed+=dt;
        if(PlayerStep is {} playerStep)playerStep(dt,movement,aim);else PlayerLogic.Tick(this,dt,movement,aim);
        if(SpawnerStep is {} spawnerStep)spawnerStep(dt);else Waves.Tick(this,dt);
        for(int i=0;i<7;i++){if(WeaponSteps[i] is {} step)step(dt);else if(!NativeActors)WeaponSystems[i].Tick(this,dt);}
        for(int i=0;i<Bullets.Length;i++)if(Bullets[i].Active){if(ProjectileSteps[i] is {} step)step(dt);else if(!NativeActors)ProjectileLogic.Tick(this,Bullets[i],dt);}
        SettleDeaths();
        for(int i=0;i<Drops.Length;i++)if(Drops[i].Active){if(DropSteps[i] is {} step)step(dt);else if(!NativeActors)DropLogic.Tick(this,Drops[i],dt);}
        if(Exp>=Rules.ExpNext(Level)){Exp-=Rules.ExpNext(Level);Level++;Choices=DrawChoices();State=Flow.Upgrade;Sound?.Invoke("upgrade");Burst(Player,36,1);return;}
        if(active)ActiveSkill();if(detonate)Detonate();SettleDeaths();
        for(int i=0;i<Enemies.Length;i++)if(Enemies[i].Active){if(EnemySteps[i] is {} step)step(dt);else if(!NativeActors)EnemyLogic.Tick(this,Enemies[i],dt);}
        SettleDeaths();
        if(carriedPotion && Hp<MaxHp*.5f){Hp=Math.Min(MaxHp,Hp+40);carriedPotion=false;Say("起始恢复剂已使用 · +40 生命");}
        Corpses.RemoveAll(c=>Elapsed-c.Time>12);if(Corpses.Count>256)Corpses.RemoveRange(0,Corpses.Count-256);
        for(int i=0;i<Particles.Length;i++)if(Particles[i].Active){if(EffectSteps[i] is {} step)step(dt);else if(!NativeActors)EffectLogic.Tick(Particles[i],dt);}
        foreach(var n in Numbers)if(n.Life>0)EffectLogic.TickNumber(n,dt);
        if(Hp<=0)Finish(false);else if(Elapsed>=Duration)Finish(true);
    }
    public Enemy SpawnEnemy(int kind,Vector2? position=null)
    {
        var e=Enemies.FirstOrDefault(x=>!x.Active)??Enemies.MinBy(x=>x.Born)!;
        WeaponSystems[1].Forget(e.Id);
        var r=Rules.Enemies[kind];float angle=(float)random.NextDouble()*MathF.Tau;
        e.Active=true;e.Id=++serial;e.Kind=kind;e.Phase=0;e.Timer=kind==5?3:kind==4?2:1;e.AttackTimer=0;e.HitFlash=0;e.HealTimer=1;e.Born=++born;
        e.Position=position??Vector2.Clamp(Player+new Vector2(MathF.Cos(angle),MathF.Sin(angle))*9,-Rules.Bounds,Rules.Bounds);e.Hp=e.MaxHp=r.Hp*(1+.3f*Elapsed/60);e.Direction=Vector2.Zero;return e;
    }
    public static Vector2 Normalize(Vector2 v)=>v.LengthSquared()<1e-8f?Vector2.Zero:Vector2.Normalize(v);
    public static float Radius(Enemy e)=>e.Kind==5?.85f:e.Kind==4?.52f:.22f;
    public Enemy? Nearest(Vector2 point)=>Enemies.Where(e=>IsTargetable(e)&&e.Hp>0).MinBy(e=>Vector2.DistanceSquared(point,e.Position));
    public static float SegmentDistance(Vector2 p,Vector2 a,Vector2 b){var d=b-a;float t=d.LengthSquared()<1e-8f?0:Math.Clamp(Vector2.Dot(p-a,d)/d.LengthSquared(),0,1);return Vector2.Distance(p,a+d*t);}
    public void Hit(Enemy e,float damage,Vector2 direction,bool feedback=true){if(!e.Active||e.Hp<=0)return;e.Hp-=damage;e.HitFlash=.08f;if(feedback){e.Position+=Normalize(direction)*.08f;Burst(e.Position,3,0);if(e.Kind>=4)Shake=Math.Max(Shake,.18f);var n=Numbers.FirstOrDefault(n=>n.Life<=0)??Numbers.MinBy(n=>n.Life)!;n.Position=e.Position;n.Life=.65f;n.Amount=(int)Math.Ceiling(damage);n.Id++;}}
    public void Area(Vector2 position,float radius,float damage){foreach(var e in Enemies)if(IsTargetable(e)&&Vector2.Distance(e.Position,position)<radius+Radius(e))Hit(e,damage,e.Position-position);}
    public void SettleDeaths()
    {
        // Snapshot of newly dead entities; active is cleared before secondary explosion.
        bool again;
        do {again=false;foreach(var e in Enemies)if(e.Active&&e.Hp<=0){e.Active=false;WeaponSystems[1].Forget(e.Id);again=true;Kills++;Energy=Math.Min(100,Energy+4);var r=Rules.Enemies[e.Kind];DropReward(e.Position,r.Exp,r.GoldTenths,e.Kind==5?12:e.Kind==4?3:0);Corpses.Add((e.Position,Elapsed));Burst(e.Position,8,2);if(e.Kind==2){Area(e.Position,1.25f,20*DamageMultiplier);Ring(e.Position,1.25f,2);}}}while(again);
    }
    public void DropReward(Vector2 position,int exp,int gold,int stars=0){var d=Drops.FirstOrDefault(x=>!x.Active);if(d==null){RunGold+=gold;RunStars+=stars;TotalEarned+=gold;return;}d.Active=true;d.Position=position;d.Exp=exp;d.Gold=gold;d.Stars=stars;d.Life=120;}
    public void Hurt(float damage,int source){if(Hp<=0||Invulnerable>0)return;if(Shield){Shield=false;Invulnerable=.8f;Say("护盾吸收了一次伤害");return;}float actual=damage*(1-Armor);Hp-=actual;DamageSources[source]+=actual;Invulnerable=.35f;Shake=.45f;Sound?.Invoke("hurt");if(Hp<=0)LastDeath=Rules.Enemies[source].Name;}
    public bool ActiveSkill(){if(Energy<100||Cooldown>0){Say(Energy<100?"能量不足 · 击杀充能":"冲击波冷却中");return false;}Energy=0;Cooldown=12*MathF.Pow(.85f,Passives[6]);Area(Player,3.5f,50*DamageMultiplier*(1+.2f*Passives[6]));foreach(var e in Enemies)if(e.Active&&Vector2.Distance(e.Position,Player)<3.5f){e.Phase=0;e.Timer=2;}Ring(Player,3.5f,1);Shake=.7f;Sound?.Invoke("storm");return true;}
    public bool Detonate(){if(Corpses.Count==0){Say("无碎尸");return false;}var corpses=Corpses.ToArray();Corpses.Clear();foreach(var c in corpses){Area(c.Position,1.25f,25*DamageMultiplier);Ring(c.Position,1.25f,2);}Sound?.Invoke("storm");return true;}
    public void Effect(Vector2 position,Vector2 velocity,float life,float size,int kind){var p=Particles.FirstOrDefault(x=>!x.Active);if(p==null)return;p.Active=true;p.Position=position;p.Velocity=velocity;p.Life=p.MaxLife=life;p.Size=size;p.Kind=kind;}
    public void Burst(Vector2 position,int count,int kind){if(BurstEffect is {} emit&&emit(position,kind))return;if(Particles.Count(p=>p.Active)>600)count=Math.Max(1,count/3);for(int i=0;i<count;i++){float a=(float)random.NextDouble()*MathF.Tau;Effect(position,new Vector2(MathF.Cos(a),MathF.Sin(a))*1.5f,.3f+(float)random.NextDouble()*.3f,.06f,kind);}}
    public void Ring(Vector2 position,float radius,int kind){int n=Particles.Count(p=>p.Active)>600?12:32;for(int i=0;i<n;i++){float a=MathF.Tau*i/n;Effect(position+new Vector2(MathF.Cos(a),MathF.Sin(a))*radius,Vector2.Zero,.3f,.07f,kind);}}
    public Offer[] Eligible()
    {
        var list=new List<Offer>();for(int i=0;i<7;i++){if(!Profile.Unlocked[i])continue;if(Weapons[i]==0)list.Add(new($"new{i}",Rules.Weapons[i].Name,"新武器 · "+(i==1?"近身环绕，贴怪更危险":"自动施放，与主武器协同"),0,i));else if(Weapons[i]<5)list.Add(new($"weapon{i}",Rules.Weapons[i].Name+"强化","伤害 +20% · 等级 "+(Weapons[i]+1)+"/5",1,i));else if(Evolution[i]==0){list.Add(new($"evo{i}a",Rules.Weapons[i].EvolutionA,"进化分支 · 倍增输出与机制改变",2,i));list.Add(new($"evo{i}b",Rules.Weapons[i].EvolutionB,"进化分支 · 倍增输出与机制改变",3,i));}}
        for(int i=0;i<7;i++)if(Passives[i]<5)list.Add(new($"passive{i}",Rules.PassiveNames[i],Rules.PassiveDescriptions[i],4,i));
        if(Profile.LegendUnlocked && Passives[0]<5)list.Add(new("legend","恒星核心","伤害 +40%，恢复 20 生命",6,0,3));
        if(list.Count<3){list.Add(new("heal","应急修复","恢复 40 生命",5,0));list.Add(new("gold","资源回收","获得 20 金币",5,1));list.Add(new("energy","充能电池","主动能量充满",5,2));}return list.ToArray();
    }
    private int Rarity(){int[] w=Level<5?[80,18,2,0]:Level<15?[55,32,12,1]:[35,38,22,5];int n=random.Next(w.Sum());for(int i=0;i<4;i++){n-=w[i];if(n<0)return i;}return 0;}
    public Offer[] DrawChoices(){var eligible=Eligible().ToList();var chosen=new List<Offer>();var synergy=eligible.Where(x=>x.Kind is 1 or 2 or 3 || x.Kind==4&&x.Index==0).ToArray();if(synergy.Length>0){var o=synergy[random.Next(synergy.Length)];chosen.Add(o);eligible.RemoveAll(x=>x.Index==o.Index&&x.Kind==o.Kind);}while(chosen.Count<3 && eligible.Count>0){var o=eligible[random.Next(eligible.Count)];chosen.Add(o);eligible.RemoveAll(x=>x.Id==o.Id || x.Index==o.Index && (x.Kind is 2 or 3) && (o.Kind is 2 or 3));}return chosen.Select(o=>{int r=o.Kind==6?3:Rarity();CandidateRarities[r]++;return o with{Rarity=r};}).ToArray();}
    public bool Choose(int index){if(State!=Flow.Upgrade||index<0||index>=Choices.Length)return false;var o=Choices[index];if(!Eligible().Any(x=>x.Id==o.Id))return false;ChosenRarities[o.Rarity]++;switch(o.Kind){case 0:Weapons[o.Index]=1;break;case 1:Weapons[o.Index]++;break;case 2:Evolution[o.Index]=1;break;case 3:Evolution[o.Index]=2;break;case 4:Passives[o.Index]++;switch(o.Index){case 0:DamageMultiplier+=.2f;break;case 1:MoveSpeed*=1.15f;break;case 2:Magnet*=1.3f;break;case 3:MaxHp+=20;Hp=Math.Min(MaxHp,Hp+20);break;case 4:Armor=Math.Min(.6f,Armor+.08f);break;case 5:AttackMultiplier+=.2f;break;}break;case 5:if(o.Index==0)Hp=Math.Min(MaxHp,Hp+40);if(o.Index==1)RunGold+=200;if(o.Index==2)Energy=100;break;case 6:DamageMultiplier+=.4f;Hp=Math.Min(MaxHp,Hp+20);Passives[0]++;break;}State=Flow.Run;if(Exp>=Rules.ExpNext(Level)){Exp-=Rules.ExpNext(Level);Level++;Choices=DrawChoices();State=Flow.Upgrade;}return true;}
    public bool OpenSupply(){if(State!=Flow.Run||!SupplyAvailable||Vector2.Distance(Player,SupplyPosition)>1.5f){Say("靠近补给站后按 F");return false;}State=Flow.Supply;return true;}
    public bool BuySupply(int index){int[] prices=[60,50,80];if(State!=Flow.Supply||index<0||index>=3)return false;int price=prices[index]*10;if(RunGold+Profile.GoldTenths<price){Say("金币不足");return false;}int fromRun=Math.Min(RunGold,price);RunGold-=fromRun;Profile.GoldTenths-=price-fromRun;if(index==0)Hp=Math.Min(MaxHp,Hp+40);if(index==1)TemporarySpeed=60;if(index==2)DamageMultiplier+=.2f;SupplyAvailable=false;State=Flow.Run;Persist?.Invoke();return true;}
    public void Finish(bool won){if(settled)return;settled=true;Won=won;State=Flow.Result;Profile.GoldTenths+=RunGold;Profile.Stars+=RunStars;
        bool[] achieved=[won,Kills>=500,Weapons.Count(x=>x>0)>=4];for(int i=0;i<3;i++)if(achieved[i]&&!Profile.Achievements[i]){Profile.Achievements[i]=true;Profile.GoldTenths+=250;}
        Persist?.Invoke();Sound?.Invoke(won?"upgrade":"hurt");}
    public string BuildSummary()=>string.Join(" / ",Enumerable.Range(0,7).Where(i=>Weapons[i]>0).Select(i=>Evolution[i]>0?(Evolution[i]==1?Rules.Weapons[i].EvolutionA:Rules.Weapons[i].EvolutionB):$"{Rules.Weapons[i].Name} Lv.{Weapons[i]}"));
}
