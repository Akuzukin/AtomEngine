using Atom;
using System.Numerics;
using System.Text.Json;
namespace Game;

// Scene-owned pools keep this demo bounded; scripts can also queue runtime prefab creation.
public sealed class SurvivorActor : Behaviour
{
    public int Role; // 0 player, 1 enemy, 2 projectile
    private bool active;
    internal bool Active { get => active; set { active = value; Object.Visible = value; Object.ActiveSelf=value; } }
    internal Vector3 Velocity;
    internal float Life;
    internal static readonly List<SurvivorActor> Actors = new();
    public override void OnCreate() { Actors.Add(this); }
    public override void OnDestroy() { Actors.Remove(this); }
    internal void Hide() { Active = false; }
    public override string CaptureReloadState()=>JsonSerializer.Serialize(new {active,velocity=new[]{Velocity.X,Velocity.Y,Velocity.Z},Life});
    public override void RestoreReloadState(string state){using var d=JsonDocument.Parse(state);var r=d.RootElement;active=r.GetProperty("active").GetBoolean();var v=r.GetProperty("velocity");Velocity=new(v[0].GetSingle(),v[1].GetSingle(),v[2].GetSingle());Life=r.GetProperty("Life").GetSingle();}
    public override void OnReload(){Actors.Add(this);}
}

public sealed class SurvivorGame : Behaviour
{
    public float Duration = 120;
    public InputPreset Controls=InputPreset.Wasd;
    public KeyCode RestartKey=KeyCode.R,UpgradeOne=KeyCode.D1,UpgradeTwo=KeyCode.D2,UpgradeThree=KeyCode.D3;
    public AudioClip DeathSound=AudioClip.None;
    public float DeathSoundVolume=.65f;
    internal static SurvivorGame? Current;
    private GameObject? healthBar,upgradeChoices;
    internal void Choose(int choice){if(!choosing)return;if(choice==0)interval=Math.Max(.14f,interval*.8f);else if(choice==1)speed+=.8f;else if(choice==2)hp=Math.Min(8,hp+2);else return;choosing=false;}
    private Random random = new(42);
    private int randomDraws;
    private bool rebind;
    private bool initialized, ended, choosing;
    private float elapsed, spawn, fire, invulnerable, speed, interval;
    private int hp, kills, level, nextLevel;
    private SurvivorActor? player;
    private SurvivorActor[] enemies = [], bullets = [];
    public override void OnCreate() { initialized = false;Current=this; }
    public override void OnDestroy(){if(Current==this)Current=null;}
    private double NextRandom(){randomDraws++;return random.NextDouble();}
    private void Bind(){player=SurvivorActor.Actors.FirstOrDefault(a=>a.Role==0);enemies=SurvivorActor.Actors.Where(a=>a.Role==1).ToArray();bullets=SurvivorActor.Actors.Where(a=>a.Role==2).ToArray();healthBar=Object.FindById("HealthBar");upgradeChoices=Object.FindById("UpgradeChoices");}
    public override string CaptureReloadState()=>JsonSerializer.Serialize(new {initialized,ended,choosing,elapsed,spawn,fire,invulnerable,speed,interval,hp,kills,level,nextLevel,randomDraws});
    public override void RestoreReloadState(string state){using var d=JsonDocument.Parse(state);var r=d.RootElement;initialized=r.GetProperty("initialized").GetBoolean();ended=r.GetProperty("ended").GetBoolean();choosing=r.GetProperty("choosing").GetBoolean();
        elapsed=r.GetProperty("elapsed").GetSingle();spawn=r.GetProperty("spawn").GetSingle();fire=r.GetProperty("fire").GetSingle();invulnerable=r.GetProperty("invulnerable").GetSingle();speed=r.GetProperty("speed").GetSingle();interval=r.GetProperty("interval").GetSingle();hp=r.GetProperty("hp").GetInt32();kills=r.GetProperty("kills").GetInt32();level=r.GetProperty("level").GetInt32();nextLevel=r.GetProperty("nextLevel").GetInt32();randomDraws=r.GetProperty("randomDraws").GetInt32();random=new(42);for(int i=0;i<randomDraws;i++)random.NextDouble();rebind=true;}
    public override void OnReload(){Current=this;rebind=true;}
    private void Reset()
    {
        player = SurvivorActor.Actors.FirstOrDefault(a => a.Role == 0);
        enemies = SurvivorActor.Actors.Where(a => a.Role == 1).ToArray();
        bullets = SurvivorActor.Actors.Where(a => a.Role == 2).ToArray();
        if (player == null) return;
        healthBar=Object.FindById("HealthBar");upgradeChoices=Object.FindById("UpgradeChoices");
        Object.StopAudio();
        foreach (var a in SurvivorActor.Actors) a.Hide();
        player.Active = true; player.Object.Position = new Vector3(0, .65f, 0);
        elapsed = spawn = fire = invulnerable = 0; hp = 5; kills = 0; level = 1;
        nextLevel = 5; speed = 5; interval = .65f; ended = choosing = false; initialized = true;
    }
    public override void OnUpdate(float dt)
    {
        if(rebind){Bind();rebind=false;}
        if (!initialized) Reset();
        if (player == null) return;
        if (Object.Input.WasPressed(RestartKey)) Reset();
        if (choosing)
        {
            if (Object.Input.WasPressed(UpgradeOne)) { interval = Math.Max(.14f, interval * .8f); choosing = false; }
            else if (Object.Input.WasPressed(UpgradeTwo)) { speed += .8f; choosing = false; }
            else if (Object.Input.WasPressed(UpgradeThree)) { hp = Math.Min(8, hp + 2); choosing = false; }
        }
        if (!ended && !choosing) Tick(Math.Clamp(dt, 0, .05f));
        if(healthBar?.IsValid==true)healthBar.UI.Progress=hp/8f;
        if(upgradeChoices?.IsValid==true)upgradeChoices.ActiveSelf=choosing;
        Object.Text = $"ATOM SURVIVORS   {elapsed:0}/{Duration:0}s   HP {hp}   Kills {kills}   Lv {level}\n" +
            (ended ? (hp <= 0 ? "DEFEAT" : "SURVIVED!") + "   R: restart" : choosing ?
            "LEVEL UP!  1/2/3 or arrows / D-pad + Enter / A" :
            "WASD / left stick: move   Auto fire   R: restart");
    }
    private void Tick(float dt)
    {
        elapsed += dt; invulnerable -= dt; spawn -= dt; fire -= dt;
        if (elapsed >= Duration) { ended = true; return; }
        var input=Object.Input.Movement(Controls);
        var move = new Vector3(input.X,0,input.Y);
        if (move.LengthSquared() > 1) move = Mathf.NormalizeOrZero(move);
        var p = player!.Object.Position + move * speed * dt;
        p.X = Math.Clamp(p.X, -13, 13); p.Z = Math.Clamp(p.Z, -9, 9); player.Object.Position = p;
        if (spawn <= 0)
        {
            var e = Array.Find(enemies, a => !a.Active);
            if (e != null) { float angle = (float)NextRandom() * Mathf.TwoPI;
                e.Object.Position = p + new Vector3(Mathf.Cos(angle) * 17, 0, Mathf.Sin(angle) * 17); e.Active = true; }
            spawn = Math.Max(.18f, .9f - elapsed * .006f);
        }
        SurvivorActor? target = null; float nearest = float.MaxValue;
        foreach (var e in enemies)
        {
            if (!e.Active) continue;
            var delta = p - e.Object.Position; float distance = delta.Length();
            if (distance > .01f) e.Object.Position += delta / distance * (1.6f + elapsed * .013f) * dt;
            if (distance < nearest) { nearest = distance; target = e; }
            if (distance < 1.1f && invulnerable <= 0) { hp--; invulnerable = 1; if (hp <= 0) { ended = true; return; } }
        }
        if (fire <= 0 && target != null)
        {
            var b = Array.Find(bullets, a => !a.Active);
            var direction = target.Object.Position - p;
            if (b != null && direction.LengthSquared() > .0001f) { b.Object.Position = p; b.Velocity = Vector3.Normalize(direction) * 18; b.Life = 2; b.Active = true; }
            fire = interval;
        }
        foreach (var b in bullets)
        {
            if (!b.Active) continue;
            var start = b.Object.Position; var end = start + b.Velocity * dt;
            b.Object.Position = end; b.Life -= dt;
            foreach (var e in enemies)
            {
                if (!e.Active) continue;
                var segment = end - start;
                float t = Math.Clamp(Vector3.Dot(e.Object.Position - start, segment) / Math.Max(segment.LengthSquared(), .00001f), 0, 1);
                if (Vector3.DistanceSquared(start + segment * t, e.Object.Position) < .7f)
                { e.Hide(); b.Hide(); kills++; if(DeathSound.IsAssigned)Object.PlayAudio(DeathSound,DeathSoundVolume);else Object.PlayAudio(); break; }
            }
            if (b.Active && b.Life <= 0) b.Hide();
        }
        if (kills >= nextLevel) { level++; nextLevel += level * 5; choosing = true; }
    }
}

public sealed class SurvivorUpgrade:Behaviour {
    public int Choice;
    public override void OnUiClick()=>SurvivorGame.Current?.Choose(Choice);
}
