using Atom;
namespace StarOutpost;
public sealed class OutpostButton:Behaviour
{
    public string Action="";
    public override void OnUiClick()=>OutpostUI.Current?.Click(Action);
}
public sealed class OutpostUI:Behaviour
{
    public static OutpostUI? Current;
    private Simulation game=>OutpostRuntime.Current!.Game;
    private string saveError=>OutpostRuntime.Current!.SaveError;
    private readonly Dictionary<string,GameObject> ui=new();
    private readonly Dictionary<string,string> cachedText=new();
    private readonly Dictionary<uint,bool> visibility=new();
    private float timer;
    private bool bound;
    public override void OnCreate(){Current=this;}
    public override void OnDestroy(){if(Current==this)Current=null;}
    public override void OnUpdate(float dt)
    {
        if(OutpostRuntime.Current==null)return;
        if(!bound){foreach(string id in new[]{"HUD","Health","XP","Energy","Boss","BossText","Notice","Panel","Title","Body","Footer","Primary","Secondary","Back","Choice0","Choice1","Choice2","Shop0","Shop1","Shop2","Shop3","Shop4","Shop5","Shop6","Shop7","Shop8","Shop9","Shop10","Shop11","Shop12","Shop13","Shop14","Mute"})ui[id]=Object.FindById(id)??throw new InvalidOperationException("Missing UI "+id);bound=true;}
        timer-=dt;if(timer<=0){UpdateUI();timer=.1f;}
    }
    public void HandleInput()
    {
        if(!bound)return;var input=Object.Input;
        if(input.WasPressed(KeyCode.M))AudioController.Current?.Toggle();
        if(input.WasPressed(KeyCode.Escape)){if(game.State==Flow.Run)game.State=Flow.Pause;else if(game.State==Flow.Pause||game.State==Flow.Supply)game.State=Flow.Run;else if(game.State==Flow.Shop||game.State==Flow.Prep)game.State=Flow.Menu;UpdateUI();}
        if(game.State==Flow.Upgrade||game.State==Flow.Supply){if(input.WasPressed(KeyCode.D1))Click("choice0");else if(input.WasPressed(KeyCode.D2))Click("choice1");else if(input.WasPressed(KeyCode.D3))Click("choice2");}
    }
    public void Click(string action)
    {
        if(action=="mute"){AudioController.Current?.Toggle();UpdateUI();return;}
        if(action.StartsWith("choice")&&int.TryParse(action[6..],out int c)){if(game.State==Flow.Upgrade)game.Choose(c);else if(game.State==Flow.Supply)game.BuySupply(c);}
        if(action=="primary"){if(game.State==Flow.Menu)game.State=Flow.Prep;else if(game.State==Flow.Prep)game.Start();else if(game.State==Flow.Pause)game.State=Flow.Run;else if(game.State==Flow.Result)game.State=Flow.Menu;}
        if(action=="secondary"){if(game.State==Flow.Menu)game.State=Flow.Shop;else if(game.State==Flow.Pause)game.Finish(false);else if(game.State==Flow.Prep){game.Profile.Character=game.Profile.ScoutUnlocked?1-game.Profile.Character:0;OutpostRuntime.Current?.Save();}}
        if(action=="back"){if(game.State==Flow.Supply)game.State=Flow.Run;else if(game.State==Flow.Shop||game.State==Flow.Prep)game.State=Flow.Menu;}
        if(action.StartsWith("shop")&&game.State==Flow.Shop&&int.TryParse(action[4..],out int s)){bool ok=false;if(s<5)ok=game.Profile.BuyMeta(s);else if(s<10)ok=game.Profile.Unlock(s-3);else if(s<13)ok=game.Profile.BuyConsumable(s-10);else if(s==13&&!game.Profile.ScoutUnlocked&&game.Profile.Stars>=50){game.Profile.Stars-=50;game.Profile.ScoutUnlocked=true;ok=true;}else if(s==14&&!game.Profile.LegendUnlocked&&game.Profile.Stars>=100){game.Profile.Stars-=100;game.Profile.LegendUnlocked=true;ok=true;}game.Say(ok?"购买成功 · 永久保留":"金币/星辰不足，或已满级");if(ok)OutpostRuntime.Current?.Save();}
        UpdateUI();
    }
    private void Show(GameObject obj,bool value){if(!visibility.TryGetValue(obj.Entity,out bool old)||old!=value){obj.Visible=value;visibility[obj.Entity]=value;}}
    private void Text(string id,string value){if(!cachedText.TryGetValue(id,out string? old)||old!=value){ui[id].Text=value;cachedText[id]=value;}}
    public void UpdateUI()
    {
        var state=game.State;bool combat=state is Flow.Run or Flow.Upgrade or Flow.Pause or Flow.Supply;
        // Space belongs to the active skill during combat, not the engine's UI activation key.
        ui["Mute"].UI.Interactable=!combat;
        foreach(var pair in ui)Show(pair.Value,false);
        Show(ui["Mute"],true);Text("Mute",AudioController.Current?.Muted==true?"M · 声音关闭":"M · 声音开启");
        if(combat){Show(ui["HUD"],true);Text("HUD",$"碎星哨站    {(int)game.Elapsed/60:00}:{(int)game.Elapsed%60:00} / 20:00    Lv.{game.Level}    击杀 {game.Kills}    金币 {game.RunGold/10f:0.0}\n{game.BuildSummary()}");foreach(string id in new[]{"Health","XP","Energy","Footer"})Show(ui[id],true);ui["Health"].UI.Progress=Math.Clamp(game.Hp/game.MaxHp,0,1);ui["XP"].UI.Progress=Math.Clamp(game.Exp/(float)Rules.ExpNext(game.Level),0,1);ui["Energy"].UI.Progress=game.Energy/100;Text("Footer",$"生命 {game.Hp:0}/{game.MaxHp:0}   能量 {game.Energy:0}%  冷却 {game.Cooldown:0.0}s   WASD/方向键 · 空格冲击波 · E引爆 · F补给 · Esc暂停");var boss=game.Enemies.FirstOrDefault(e=>e.Active&&e.Kind==5);if(boss!=null){Show(ui["Boss"],true);Show(ui["BossText"],true);ui["Boss"].UI.Progress=Math.Clamp(boss.Hp/boss.MaxHp,0,1);Text("BossText",$"裂隙守卫    {boss.Hp:0} / {boss.MaxHp:0}");}}
        if(game.NoticeTime>0||saveError!=""){Show(ui["Notice"],true);Text("Notice",saveError!=""?saveError:game.Notice);}
        if(state==Flow.Run)return;
        foreach(string id in new[]{"Panel","Title","Body"})Show(ui[id],true);
        if(state==Flow.Menu){Text("Title","碎 星 哨 站\nSTARFALL OUTPOST");Text("Body",$"裂谷站台 · 离线生存行动\n\n输出交给系统，走位交给你。\n在 20 分钟尸潮中组合武器、收集经验、进化构筑。\n\n金币 {game.Profile.GoldTenths/10f:0.0}   星辰 {game.Profile.Stars}\n成就：通关 {(game.Profile.Achievements[0]?"✓":"—")} / 500击杀 {(game.Profile.Achievements[1]?"✓":"—")} / 四武器 {(game.Profile.Achievements[2]?"✓":"—")}");Button("Primary","准备出击");Button("Secondary","军械库与永久养成");}
        if(state==Flow.Prep){Text("Title","局前准备 · 星域部署");Text("Body",$"哨兵：{(game.Profile.Character==0?"星屑 · 脉冲枪":"月影 · 月刃 / 移速+15%") }\n\n携带：恢复剂 {game.Profile.Consumables[0]} / 护盾 {game.Profile.Consumables[1]} / 推进器 {game.Profile.Consumables[2]}\n每种自动消耗一份 · 恢复剂低于半血触发\n\nWASD / 方向键移动 · 鼠标瞄准 · 自动开火\n升级按1/2/3或点击 · 空格冲击波 · E碎尸引爆\nF使用补给站 · Esc暂停 · M静音\n金币死亡保留 · 20分钟通关");Button("Primary","开始行动");Button("Secondary",game.Profile.ScoutUnlocked?"切换哨兵":"月影尚未解锁 · 50星辰");Button("Back","返回");}
        if(state==Flow.Pause){Text("Title","行动暂停");Text("Body","战斗、计时与冷却已冻结。\n\n放弃行动仍保留已拾取的金币与星辰。");Button("Primary","继续行动 · Esc");Button("Secondary","放弃并结算");}
        if(state==Flow.Upgrade||state==Flow.Supply){Text("Title",state==Flow.Upgrade?$"Lv.{game.Level} · 选择强化":"补给站 · 限购一次");Text("Body",state==Flow.Upgrade?"1 / 2 / 3 或点击卡片 · 战斗已冻结":"可使用本局与账户金币");for(int i=0;i<3;i++){string text;if(state==Flow.Upgrade){var o=game.Choices[i];string[] rarity=["普通","稀有","史诗","传说"];text=$"{i+1}   {rarity[o.Rarity]}\n\n{o.Name}\n\n{o.Description}";}else text=new[]{"1 · 恢复剂\n\n恢复40生命\n60金币","2 · 临时推进器\n\n60秒移速+25%\n50金币","3 · 一次性核心\n\n本局伤害+20%\n80金币"}[i];Button("Choice"+i,text);}if(state==Flow.Supply)Button("Back","暂不购买 · Esc");}
        if(state==Flow.Result){Text("Title",game.Won?"行动成功 · 哨站存续":"行动结束 · 星火未熄");float total=game.DamageSources.Sum();string sources=string.Join(" / ",Enumerable.Range(0,6).Where(i=>game.DamageSources[i]>0).Select(i=>$"{Rules.Enemies[i].Name} {game.DamageSources[i]/Math.Max(1,total):P0}"));Text("Body",$"生存 {(int)game.Elapsed/60:00}:{(int)game.Elapsed%60:00}    击杀 {game.Kills}    等级 {game.Level}\n\n构筑：{game.BuildSummary()}\n承伤来源：{sources}\n最后威胁：{game.LastDeath}\n\n金币入账 {game.RunGold/10f:0.0}    星辰入账 {game.RunStars}\n账户金币 {game.Profile.GoldTenths/10f:0.0}    星辰 {game.Profile.Stars}\n首次成就奖励25金币，已自动入账");Button("Primary","返回哨站");}
        if(state==Flow.Shop){Text("Title",$"军械库 · 金币 {game.Profile.GoldTenths/10f:0.0} / 星辰 {game.Profile.Stars}");Text("Body","永久强化共享全部哨兵 · 逐级价格60 / 150 / 350 / 700金币");for(int i=0;i<15;i++){string text;if(i<5){int l=game.Profile.Meta[i];text=$"{Rules.MetaNames[i]} {l}/4\n{(l<4?Rules.MetaCosts[l]+"金币":"已满级")}";}else if(i<10){int wi=i-3;text=$"{Rules.Weapons[wi].Name}\n{(game.Profile.Unlocked[wi]?"已解锁":Rules.UnlockCosts[wi]+"金币")}";}else if(i<13)text=new[]{"恢复剂 · 40金币","护盾 · 80金币","推进器 · 150金币"}[i-10]+$"\n持有 {game.Profile.Consumables[i-10]}";else text=i==13?(game.Profile.ScoutUnlocked?"月影 · 已解锁":"月影 · 50星辰"):(game.Profile.LegendUnlocked?"恒星核心 · 已解锁":"传说核心 · 100星辰");Button("Shop"+i,text);}Button("Back","返回哨站");}
    }
    private void Button(string id,string text){Show(ui[id],true);Text(id,text);}
}
