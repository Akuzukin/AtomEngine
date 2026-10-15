using Atom;
using System.Text.Json;
namespace StarOutpost;

// Session owns flow, persistence and the fixed clock. Every actor owns its behavior.
public sealed class OutpostRuntime:Behaviour
{
    public static OutpostRuntime? Current;
    public Simulation Game {get;private set;}=null!;
    public string SaveError=>saveError;
    private readonly SaveStore saves=new("StarOutpost");
    private string storage="",saveError="";
    private bool preserveInvalidProfile;
    public override void OnCreate()
    {
        Current=this;ActorBindings.Clear();
        storage=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"StarOutpost","profile.json");
        Profile profile=new();try{if(saves.Exists("profile")){profile=saves.Load<Profile>("profile")??new();Validate(profile);}else if(File.Exists(storage)){profile=JsonSerializer.Deserialize<Profile>(File.ReadAllText(storage))??new();Validate(profile);saves.Save("profile",profile);}}catch(Exception e){saveError="存档读取失败，旧文件保留："+e.Message;profile=new();preserveInvalidProfile=true;}
        Game=new(profile){NativeActors=true};Game.Persist=Save;
    }
    private static void Validate(Profile p){if(p.Version!=1||p.Meta.Length!=5||p.Unlocked.Length!=7||p.Consumables.Length!=3||p.Achievements.Length!=3||p.GoldTenths<0||p.Stars<0||p.Meta.Any(x=>x<0||x>4)||p.Character<0||p.Character>1)throw new InvalidDataException("存档格式无效");}
    public void Save(){try{Directory.CreateDirectory(Path.GetDirectoryName(storage)!);if(preserveInvalidProfile&&File.Exists(storage)){File.Copy(storage,storage+".invalid-"+DateTime.UtcNow.ToString("yyyyMMddHHmmssfff"),false);preserveInvalidProfile=false;}saves.Save("profile",Game.Profile);saveError="";}catch(Exception e){saveError="保存失败 · "+e.Message;}}
    public override void OnDestroy(){if(Current==this){Current=null;ActorBindings.Clear();}}
    public override void OnUpdate(float dt)
    {
        OutpostUI.Current?.HandleInput();
        if(Game.State!=Flow.Run||PlayerController.Current==null||Game.PlayerStep==null||Game.SpawnerStep==null||Game.WeaponSteps.Any(x=>x==null))return;
        var input=PlayerController.Current.ReadControls();
        if(input.UseSupply){Game.OpenSupply();return;}
        Game.Advance(dt,input.Movement,input.Aim,input.Active,input.Detonate);
    }
}
