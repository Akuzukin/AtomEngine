using System.Numerics;
namespace StarOutpost;

public record WeaponRule(string Name, float Damage, float Rate, float Speed, int Pierce, int Count, float Spread, string EvolutionA, string EvolutionB);
public record EnemyRule(string Name,float Hp,float Speed,int Exp,int GoldTenths);
public record Offer(string Id,string Name,string Description,int Kind,int Index,int Rarity=0);
public static class Rules
{
    public const string Ruleset="survivor-v1";
    public const int EnemyCap=600, BulletCap=400, DropCap=1200, ParticleCap=1200;
    // One mesh unit = 100 design pixels. Movement uses the GDD's units/second.
    public static readonly Vector2 Bounds=new(12,8);
    public static int ExpNext(int level)=>(int)Math.Floor(15*Math.Pow(1.12,level-1));
    public static readonly WeaponRule[] Weapons=[
        new("脉冲枪",8,4,16,1,1,0,"双管脉冲","相位穿透"),
        new("月刃",6,2,0,99,1,0,"剑刃风暴","高速月轮"),
        new("霰弹",5,1.2f,12,1,5,.18f,"七星散射","长程霰弹"),
        new("光束",15,.8f,0,4,1,0,"贯星射线","宽幅光束"),
        new("飞剑",10,1.5f,12,1,1,0,"三重飞剑","疾风追猎"),
        new("黑洞",4,1,0,99,1,0,"事件视界","奇点坍缩"),
        new("雷暴",20,.5f,0,99,1,0,"雷霆领域","链式闪电")];
    public static readonly EnemyRule[] Enemies=[new("碎石虫",12,.95f,2,1),new("冲锋怪",20,1.2f,6,3),new("爆裂者",10,1.35f,3,2),new("治疗虫",25,.75f,4,4),new("裂隙精英",96,1.1f,40,100),new("裂隙守卫",720,.65f,150,700)];
    public static readonly string[] PassiveNames=["增幅核心","推进器","磁力线圈","生命装甲","复合护甲","超频回路","主动充能"];
    public static readonly string[] PassiveDescriptions=["伤害 +20%，强化全部武器","移速 +15%，提高脱围能力","经验吸取范围 +30%","生命上限 +20，恢复 20","减伤 +8%，上限 60%","攻速 +20%，更密集弹幕","冲击波冷却 -15%，威力 +20%"];
    public static readonly int[] MetaCosts=[60,150,350,700];
    public static readonly string[] MetaNames=["攻击","生命","攻速","拾取","护甲"];
    public static readonly int[] UnlockCosts=[0,0,300,400,500,650,800];
}
public sealed class Profile
{
    public int Version {get;set;}=1;
    public int GoldTenths {get;set;}
    public int Stars {get;set;}
    public int[] Meta {get;set;}=new int[5];
    public bool[] Unlocked {get;set;}=[true,true,false,false,false,false,false];
    public int[] Consumables {get;set;}=new int[3];
    public bool[] Achievements {get;set;}=new bool[3];
    public bool ScoutUnlocked {get;set;}
    public bool LegendUnlocked {get;set;}
    public int Character {get;set;}
    public bool BuyMeta(int i) {if(i<0||i>=5||Meta[i]>=4)return false;int price=Rules.MetaCosts[Meta[i]]*10;if(GoldTenths<price)return false;GoldTenths-=price;Meta[i]++;return true;}
    public bool Unlock(int i){if(i<2||i>=7||Unlocked[i]||GoldTenths<Rules.UnlockCosts[i]*10)return false;GoldTenths-=Rules.UnlockCosts[i]*10;Unlocked[i]=true;return true;}
    public bool BuyConsumable(int i){int[] prices=[40,80,150];if(i<0||i>=3||GoldTenths<prices[i]*10||Consumables[i]>=9)return false;GoldTenths-=prices[i]*10;Consumables[i]++;return true;}
}
public enum Flow {Menu,Prep,Run,Upgrade,Pause,Supply,Result,Shop}
public sealed class Enemy
{
    public bool Active; public int Slot,Id,Kind,Phase;public Vector2 Position,Direction;public float Hp,MaxHp,Timer,AttackTimer,HitFlash,HealTimer=1;public long Born;
}
public sealed class Bullet
{
    public bool Active;public Vector2 Position,Velocity;public float Damage,Life;public int Pierce,Weapon;public HashSet<int> Hit=new();
}
public sealed class Drop
{
    public bool Active;public Vector2 Position;public int Exp,Gold,Stars;public float Life;
}
public sealed class Fx
{
    public bool Active;public Vector2 Position,Velocity;public float Life,MaxLife,Size;public int Kind;
}
public sealed class FloatingDamage
{
    public Vector2 Position;public float Life;public int Amount,Id;
}
