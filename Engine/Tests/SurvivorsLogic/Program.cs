using System.Numerics;
namespace Atom {
 public readonly struct AudioClip {public static AudioClip None=>default;public bool IsAssigned=>false;}
 public sealed class InputStub {
  private readonly GameObject owner;public InputStub(GameObject owner){this.owner=owner;}
  public bool WasPressed(KeyCode key)=>owner.Keys.Contains((int)key);
  public Vector2 Movement(InputPreset preset){var up=preset==InputPreset.Wasd?KeyCode.W:KeyCode.Up;var down=preset==InputPreset.Wasd?KeyCode.S:KeyCode.Down;var left=preset==InputPreset.Wasd?KeyCode.A:KeyCode.Left;var right=preset==InputPreset.Wasd?KeyCode.D:KeyCode.Right;var v=new Vector2((owner.Keys.Contains((int)right)?1:0)-(owner.Keys.Contains((int)left)?1:0),(owner.Keys.Contains((int)up)?1:0)-(owner.Keys.Contains((int)down)?1:0));return v.LengthSquared()>1?Vector2.Normalize(v):v;}
 }
 public sealed class GameObject {
  public InputStub Input=>new(this);public bool PlayAudio(AudioClip clip,float volume=1)=>PlayAudio();
  public bool IsValid=true,ActiveSelf=true;public sealed class UiStub{public float Progress;}public UiStub UI=new();public GameObject? FindById(string id)=>null;
  public bool Visible = true; public int AudioPlays; public bool PlayAudio() { AudioPlays++; return true; } public void StopAudio() {} public Vector3 Position; public string Text = "";
  public HashSet<int> Keys = new();
  public bool IsKeyDown(int k) => Keys.Contains(k);
  public bool IsKeyPressed(int k) => Keys.Contains(k);
  public float GamepadAxis(int a) => 0;
 }
 public abstract class Behaviour {
  public GameObject Object { get; set; } = new();
  public virtual string? CaptureReloadState()=>null;public virtual void RestoreReloadState(string state){} public virtual void OnReload(){} public virtual void OnUiClick(){} public virtual void OnCreate() {} public virtual void OnDestroy() {} public virtual void OnUpdate(float dt) {}
 }
}
namespace Tests {
 static class Program {
  static void Check(bool ok, string name) { if (!ok) throw new Exception(name); Console.WriteLine("PASS " + name); }
  static void Main() {
   var actors = Enumerable.Range(0,89).Select(i => new Game.SurvivorActor { Role = i == 0 ? 0 : i < 65 ? 1 : 2 }).ToArray();
   foreach(var a in actors) a.OnCreate();
   var game = new Game.SurvivorGame(); game.OnCreate(); game.OnUpdate(.016f);
   Check(actors[0].Object.Position.Y == .65f, "player initialized");
   game.Object.Keys.Add(0x44); for(int i=0;i<120;i++) game.OnUpdate(.016f);game.Object.Keys.Clear();
   Check(actors[0].Object.Position.X > 0 && actors[0].Object.Position.X <= 13, "movement and arena clamp");
   Check(actors.Any(a => a.Role == 1 && a.Active), "enemy spawn");
   game.Object.Keys.Add(0x52);game.OnUpdate(0);game.Object.Keys.Clear();
   Check(actors[0].Object.Position.X == 0 && game.Object.Text.Contains("HP 5"), "restart resets state");
   int audioBefore = game.Object.AudioPlays;
   // Force one enemy into the firing line, exercise projectile hit and kill accounting.
   foreach(var a in actors.Where(a => a.Role != 0)) a.Hide();
   actors[1].Active=true;actors[1].Object.Position=new Vector3(2,.65f,0);
   for(int i=0;i<80;i++)game.OnUpdate(.016f);
   Check(game.Object.Text.Contains("Kills 1"), "auto aim projectile kill");
   Check(game.Object.AudioPlays == audioBefore + 1, "enemy death plays configured audio once");
   game.Duration=2f; for(int i=0;i<160;i++)game.OnUpdate(.016f);
   Check(game.Object.Text.Contains("SURVIVED!"), "victory");
   game.Duration=120;game.Object.Keys.Add(0x52);game.OnUpdate(0);game.Object.Keys.Clear();
   for(int i=0;i<400;i++){actors[1].Active=true;actors[1].Object.Position=actors[0].Object.Position;game.OnUpdate(.016f);}
   Check(game.Object.Text.Contains("DEFEAT"), "contact damage and defeat");
   foreach(var a in actors)a.OnDestroy();Check(Game.SurvivorActor.Actors.Count==0,"lifecycle cleanup");
  }
 }
}
