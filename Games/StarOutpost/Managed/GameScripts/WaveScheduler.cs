using System.Numerics;
namespace StarOutpost;
public sealed class WaveScheduler
{
    private float spawn,elite,boss,supply;
    public WaveScheduler()=>Reset();
    public void Reset(){spawn=0;elite=90;boss=300;supply=240;}
    private static int RandomKind(Simulation game){if(game.Elapsed<90)return 0;double n=game.NextRandom();return n<.12?3:n<.3?2:n<.5?1:0;}
    public void Tick(Simulation game,float dt)
    {
        spawn-=dt;while(spawn<=0){game.SpawnEnemy(RandomKind(game));spawn+=1/(game.Elapsed<300?2:game.Elapsed<600?2.5f:game.Elapsed<900?3:3.5f);}
        if(game.Elapsed>=elite){game.SpawnEnemy(4);elite+=90;game.Say("警报 · 裂隙精英降临");game.Shake=.5f;game.Sound?.Invoke("warning");}
        if(game.Elapsed>=boss){game.SpawnEnemy(5);boss+=300;game.Say("裂隙守卫 · 注意红色射线蓄力");game.Shake=1;game.Sound?.Invoke("warning");}
        if(game.Elapsed>=supply){game.SupplyAvailable=true;game.SupplyPosition=Vector2.Clamp(game.Player+new Vector2(3,2),-Rules.Bounds+Vector2.One,Rules.Bounds-Vector2.One);supply+=240;game.Say("补给站已部署 · 靠近后按 F");}
    }
}
