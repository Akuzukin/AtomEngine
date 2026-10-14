# 幸存者 DEMO

场景：`Asset/Scenes/Survivors.scene`；可复用模板：`Asset/Templates/Survivors.scene`；脚本：`Managed/GameScripts/Survivor.cs`。默认启动场景仍为 MaterialTest。本任务不包含打包。

## 运行与操作

1. 完整构建引擎后运行输出目录 AtomEngine.exe，工作目录为 Engine；脚本宿主 ABI 为 7，升级后需重启编辑器。
2. File → Open 打开 Survivors.scene，或 File → New from template 选择 Survivors 创建未保存副本，然后 Play。
3. WASD / 左摇杆移动，自动朝最近敌人发射弹丸。初始 5 点生命，接触后有一秒无敌。生存 120 秒获胜，生命耗尽失败。
4. 达到击杀门槛升级并暂停推进玩法。1/2/3 提高射速/移速/治疗；也可点击按钮，或方向键/Tab/手柄 D-pad 选择后按 Enter/Space/A 确认。
5. R 重开；F11 切换全屏预览；编辑器 Stop 恢复编辑快照。点击预览窗口获取游戏输入焦点。

## 使用引擎功能

预置 64 个敌人和 24 个弹丸循环使用，停用对象通过 ActiveSelf 停止相关系统，并同步 Visible 显示开关。HUD/血条和升级按钮使用响应式 UI、布局容器、进度条与焦点导航；伤害和弹丸命中规则仍由玩法脚本计算。

Inspector 的脚本类型和公开字段来自程序集反射。运行时可选择 HUD 调整 Duration、Controls、RestartKey、升级键和 DeathSound，不重建当前局；暂停时也可调参，Stop 会恢复原配置。HUD 脚本的 DeathSound 字段通过资源引用配置 se.mp3，DeathSoundVolume 控制死亡音效；未指定字段时沿用 Audio Source。R 重开清理上一局效果。

Build scripts 后热重载显式保存时间、生命、击杀数、等级、计时器、随机序列和对象池状态，重建静态注册表并重新绑定实例引用。加载或状态恢复失败保留原实例。

File 菜单中的模板可复用整套玩法；Edit → Batch object pool 可复制选中子树或批量实例化预制体，配置数量、网格间距及初始停用，一次撤销整批操作。脚本可 Create、InstantiatePrefab 或给创建的普通对象 ApplyPrefab，具体语义见 SCRIPT_API.md。

## 验证

原生 --play-workflow-smoke-test 覆盖工具功能、短时 DEMO、实际击杀音效、窗口缩放/全屏/最小化、多视窗、重排脚本初始化后的重复 Play；并验证热重载不重置 HUD 中的时间/生命/击杀/等级。SurvivorsLogic 使用对象替身检查初始化、移动边界、生成、弹丸击杀、重开、音效请求、胜利、受伤失败与清理。

完整两分钟人工试玩、真实手柄操作、不同显示器 DPI 下的视觉效果仍需人工检查。其他实现边界统一见 FEATURES.md 和 SCRIPT_API.md。

使用 VS Code 编写脚本时打开 Managed/AtomScripts.code-workspace，或点击编辑器“在 VS Code 打开脚本”；Object.Input、KeyCode 和 Game.Assets.AudioAssets 提供自动补全。资源成员在脚本构建时更新。
