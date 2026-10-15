using Atom;
using System.Text.Json;
using System.Numerics;
namespace StarOutpost;
// Added only to RuntimeCheck.scene. Production startup contains no diagnostics or full-cap spawn.
public sealed class GameDiagnostics:Behaviour
{
    public static GameDiagnostics? Current;
    public bool Stress=true;
    public bool Running {get;private set;}
    private int frames,menuEnemyObjects;
    private bool ecsApiPassed;
    private float minStep=float.MaxValue,maxStep;
    private readonly List<float> times=new();
    public override void OnCreate()=>Current=this;
    public override void OnUpdate(float dt)
    {
        var g=OutpostRuntime.Current?.Game;var spawner=EnemySpawner.Current;
        if(g==null||spawner==null||g.PlayerStep==null||g.SpawnerStep==null||g.WeaponSteps.Any(x=>x==null))return;
        if(!Running){VerifyEcs();ecsApiPassed=true;var playerStep=g.PlayerStep;g.PlayerStep=(step,movement,aim)=>{minStep=Math.Min(minStep,step);maxStep=Math.Max(maxStep,step);if(Math.Abs(step-1f/60)>1e-7f)throw new InvalidOperationException("Combat altered fixed Tick step");playerStep(step,movement,aim);};menuEnemyObjects=spawner.CreatedEnemies;if(menuEnemyObjects!=0)throw new InvalidOperationException("Menu preallocated enemy GameObjects");g.Profile=new();g.Persist=null;g.Start();Running=true;if(Stress){Array.Fill(g.Profile.Unlocked,true);Array.Fill(g.Weapons,5);Array.Fill(g.Evolution,1);for(int i=0;i<600;i++){var e=g.SpawnEnemy(i==599?5:i>=594?4:i%4,new((i%30-15)*.6f,(i/30-10)*.6f));e.Hp=e.MaxHp=50000;}}}
        times.Add(dt);frames++;if(g.State==Flow.Upgrade)g.Choose(0);
        if(frames==600||frames==3000){var samples=times.Skip(30).Order().ToArray();var directory=Path.Combine(Environment.CurrentDirectory,"Diagnostics");Directory.CreateDirectory(directory);File.WriteAllText(Path.Combine(directory,"runtime-check.json"),JsonSerializer.Serialize(new{frames,ecsApiPassed,minStep,maxStep,nativeEmitters=EffectSpawner.Current?.NativeEmitterCount,menuEnemyObjects,createdEnemyObjects=spawner.CreatedEnemies,pendingEnemyObjects=spawner.PendingEnemies,registeredEnemyScripts=g.EnemySteps.Count(x=>x!=null),aliveEnemies=g.Enemies.Count(e=>e.Active),g.Elapsed,g.Kills,g.ShotCount,g.Level,averageFps=1/samples.Average(),p95FrameMs=samples[(int)(samples.Length*.95)]*1000}));}
    }
    private void VerifyEcs()
    {
        var player=Object.FindByName("Player")??throw new Exception("ECS name lookup failed");
        var camera=Object.FindById("Camera")??throw new Exception("ECS ID lookup failed");
        if(Object.FindByName("missing-object")!=null||Object.GetObject(uint.MaxValue)!=null||Object.GetObject(player.Entity)?.Entity!=player.Entity)throw new Exception("ECS entity validity failed");
        if(!player.HasComponent(ComponentKind.Mesh)||player.HasComponent(ComponentKind.Camera)||!camera.HasComponent(ComponentKind.Camera))throw new Exception("ECS component presence failed");
        if(Object.Query(ComponentKind.Camera).Count(o=>o.Entity==camera.Entity)!=1)throw new Exception("ECS query failed");
        bool playerActive=player.ActiveSelf,cameraActive=camera.ActiveSelf;
        try
        {
            Object.SetActiveBatch([new(player,false),new(camera,false)]);
            if(player.ActiveInHierarchy||camera.ActiveSelf||Object.Query(ComponentKind.Camera).Any(o=>o.Entity==camera.Entity)||!Object.Query(ComponentKind.Camera,false).Any(o=>o.Entity==camera.Entity))throw new Exception("ECS batch/filter failed");
            Object.SetActiveBatch([new(player,true),new(player,false)]);
            if(player.ActiveSelf)throw new Exception("ECS duplicate batch ordering failed");
            camera.SetComponentEnabled(ComponentKind.Camera,false);
            Object.SetActiveBatch([new(camera,true)]);
            if(Object.Query(ComponentKind.Camera).Any(o=>o.Entity==camera.Entity)||!camera.HasComponent(ComponentKind.Camera))throw new Exception("ECS enabled/presence filter failed");
        }
        finally {camera.SetComponentEnabled(ComponentKind.Camera,true);Object.SetActiveBatch([new(player,playerActive),new(camera,cameraActive)]);}
        try {Object.Query((ComponentKind)99);throw new Exception("Invalid ECS query accepted");}catch(InvalidOperationException error)when(error.Message=="Invalid component kind"){}
    }
    public override void OnDestroy(){if(Current==this)Current=null;}
}
