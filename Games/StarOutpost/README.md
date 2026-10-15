# 碎星哨站 · Starfall Outpost

使用AtomEngine制作的离线3D类幸存者游戏，规则集 `survivor-v1`。全部模型为基础网格，UI由引擎DirectWrite绘制，音效为本项目程序合成WAV，无网络资源依赖。

## 运行

发行包：双击 `Build/Windows/AtomGame.exe`。关闭窗口退出。存档位于 `%LOCALAPPDATA%/StarOutpost/profile.json`。

开发：运行 `Tools/Open-Editor.ps1`，会先编译并部署项目脚本，再打开 `StarOutpost.atproj`；点击Play启动。也可手动打开项目，先点击Build scripts再Play。SDK源码相对引用本仓库 `Engine/Managed/Atom.Managed`。

操作：WASD/方向键移动，鼠标瞄准；鼠标离开游戏视口时自动瞄准最近敌人。自动开火；1/2/3或点击选强化；空格冲击波；E碎尸引爆；靠近补给站按F；Esc暂停；M静音。按钮支持引擎Tab/方向键与Enter导航。

## 内容

- 标准20分钟，四阶段刷怪速率、90秒精英、5分钟Boss、4分钟补给站。
- 七种自动武器，每种五级、两个互斥进化分支；七类被动强化、三选一协同保底与稀有度统计。
- 碎石虫、冲锋怪、爆裂者、治疗虫、精英、两阶段Boss；红色前摇、激光预告、能量技能打断。
- 主菜单、局前准备、战斗HUD、升级、暂停、局内商店、结算、局外军械库。
- 五项四级永久强化、五项武器解锁、三种起始消耗品、星辰解锁月影哨兵与传说核心、三个一次性成就。
- 伤害归因与构筑回顾、死亡金币保留、整数货币和本地存档。
- 对象池：600敌人、400弹丸、1200掉落、1200效果粒子、256碎尸；达到敌人上限按出生顺序回收。

## 对象脚本结构

玩家、相机、敌人、武器、弹丸、掉落和特效均由各自控制器脚本驱动；`OutpostRuntime`仅保留状态、时钟和存档。`EnemySpawner`独立负责波次与敌人生成，正式场景147个对象，预置敌人实例为0；各对象池运行时按需扩容并复用。分工和固定步顺序见 [ARCHITECTURE.md](ARCHITECTURE.md)。

上下方向已修正：W/↑向屏幕上方移动，S/↓向屏幕下方移动。

命中/击杀已移除全局 Tick 减速，保留局部视觉反馈。脚本可调用 C++ ECS 对象/组件查询，玩家按场景名称获取相机，对象池批量提交原生激活状态；详见 [脚本接口](../../Engine/SCRIPT_API.md)。

## 重建与测试

从仓库根目录运行：

```powershell
python Games/StarOutpost/Tools/generate_assets.py
dotnet run --project Games/StarOutpost/Tests/Outpost.Tests.csproj -c Release
powershell -NoProfile -ExecutionPolicy Bypass -File Engine/Tools/Publish-Project.ps1 -Project D:/AtomEngine/Games/StarOutpost/StarOutpost.atproj -Output D:/AtomEngine/Games/StarOutpost/Build/Windows
```

自动诊断场景 `Asset/Scenes/RuntimeCheck.scene` 不属于正式场景清单；只用于原生验证，不用于玩家游戏。

引擎问题、缺失组件、规格口径与验收范围见 [ENGINE_ISSUES.md](ENGINE_ISSUES.md)，测试清单见 [Tests/ACCEPTANCE.md](Tests/ACCEPTANCE.md)。发行ZIP为 `Build/Windows.zip`。测试不会使用自动诊断存档覆盖玩家存档。

取消全局击杀减速后的独立控制器版本，持续近600敌人负载平均约44.74FPS（关闭其他3D程序，3000帧）；旧版46.7FPS和聚合控制器82FPS保留为历史数据。完整真人难度平衡和跨机器兼容性仍需试玩。`Preview/combat.png` 为正常开局验收画面；满容量测试只在独立诊断场景中启用。

引擎通用服务：UI角点锚定与窗口缩放、SaveStore类型存档、GPU爆裂粒子、音量总线、退出/窗口API已接入。完整脚本包文档见[Reference](../../Engine/SCRIPT_REFERENCE.md)。运行Tools/Open-Editor.ps1时自动选择较新的编辑器构建（本轮在generated/components-editor）。
