using Atom;
namespace StarOutpost;

public sealed class EnemySpawner:Behaviour
{
    public static EnemySpawner? Current;
    private Simulation? game;
    private ActorPool? enemies,telegraphs;
    public int CreatedEnemies=>enemies?.Created??0;
    public int PendingEnemies=>enemies?.Pending??0;
    public override void OnCreate()=>Current=this;
    public override void OnUpdate(float dt)
    {
        if(game==null)
        {
            if(OutpostRuntime.Current==null)return;
            game=OutpostRuntime.Current.Game;
            game.SpawnerStep=TickWaves;
            enemies=new(Object,game,"Asset/Prefabs/Enemy.prefab",Rules.EnemyCap);
            telegraphs=new(Object,game,"Asset/Prefabs/Telegraph.prefab",Rules.EnemyCap);
        }
        enemies!.Sync(i=>game.Enemies[i].Active,4);
        telegraphs!.Sync(i=>game.Enemies[i].Active && game.Enemies[i].Phase==1 && game.Enemies[i].Kind is 1 or 2 or 4 or 5);
    }
    private void TickWaves(float dt)=>game!.Waves.Tick(game,dt);
    public override void OnDestroy(){if(game!=null)game.SpawnerStep=null;if(Current==this)Current=null;}
}
