# C# 脚本 API

## 《碎星哨站》新增接口（2026-10-14）

- `Object.Pointer` 返回 `PointerState(Valid, Position, ViewportSize)`，以UI合成缓冲像素为单位；原生独立窗口与编辑器视口均使用对应坐标，移出视口或失焦时Valid为false。
- `Object.SetTint(Vector3 color, float glow=0)` 修改全部材质槽的RGB颜色与自发光倍数。颜色必须有限且非负，glow范围0～10。对象需已有Material组件。
- `Object.SetPose(Vector3 position, Quaternion rotation, Vector3 scale)` 一次提交完整局部变换，避免逐属性写入时反复读取变换；原生接口保持有限值、非零四元数、正缩放校验。

这些接口沿用桥ABI7；使用更新后的Atom.Managed.dll仍需重启编辑器。游戏制作中的原生UI缩放、普通UI背景和隐藏对象池优化记录见 [ENGINE_ISSUES.md](../Games/StarOutpost/ENGINE_ISSUES.md)。

多对象脚本优化：宿主缓存OnUpdate/OnFixedUpdate/OnCollision是否实际重写；继承重写仍执行，热重载刷新该缓存。基类空回调不再跨托管桥分派。OnUpdate复用每帧发布的Time快照，固定更新和碰撞仍收到各自步长；生命周期、输入焦点和回调顺序保持原有语义。

以 `Managed/Atom.Managed` 与原生 `ScriptSystem` 为实现来源；托管桥 ABI 为 **7**。本次升级需要完整构建并重启编辑器，游戏程序集热重载不能替换宿主接口 DLL。

## VS Code 与自动补全

使用 VS Code 打开 `Managed/AtomScripts.code-workspace`，或点击编辑器脚本区域的“在 VS Code 打开脚本”。按钮直接启动 VS Code，不依赖 .sln 文件关联。工作区推荐 Microsoft C# 与 C# Dev Kit 扩展，需要 .NET 10 SDK；本机已安装这些扩展。

工作区自动选择独立的 `Managed/AtomScripts.sln`，其中 AtomScripts 引用 Atom.Managed。Visual Studio 的主解决方案 `AtomEngine.sln` 保留 C++ 编辑器项目，并添加独立 AtomGame 目标；脚本解决方案仍独立。引擎 API 位于 `Atom` 命名空间；`using Atom;` 后可提示 Behaviour、GameObject、Mathf、KeyCode、ScriptInput、AudioClip 等类及成员。XML 注释提供参数/说明提示，源码 ProjectReference 支持跳转到定义。

首次打开后按 Ctrl+Shift+B 执行默认 Debug 脚本构建，生成音频目录成员；“运行生成任务”也可选择 Release。VS Code 构建任务输出到脚本项目的 bin 目录。将改动用于引擎运行时，点击引擎编辑器 Build scripts 或完整构建引擎，以部署程序集并触发热重载。首次安装 .NET 10 宿主接口更新后重启编辑器。

## Time 与帧参数

`Atom.Time` 提供当前游戏世界的只读时间，不需要对象引用。所有脚本共享同一帧号，计数按引擎帧或物理步推进。

| 属性 | 含义 |
| --- | --- |
| DeltaTime | 当前回调间隔（秒）；OnFixedUpdate/OnCollision 使用物理步长，其他回调使用游戏帧间隔 |
| FrameDeltaTime | 游戏帧间隔，与 OnUpdate(dt) 一致 |
| FixedDeltaTime | 固定更新步长，默认 1/60 秒 |
| ElapsedTime | 本次 Play 累计游戏帧时间，double 精度 |
| FixedTime | 已执行的固定步累计时间，double 精度 |
| FrameCount / FixedFrameCount | 游戏帧数 / 物理固定更新次数，ulong；第一帧为 1 |
| FramesPerSecond | 当前游戏帧的瞬时帧率；零间隔和暂停时为零 |
| IsFixedUpdate / IsPaused | 当前是否在固定更新或碰撞回调 / 游戏是否暂停 |

暂停时 DeltaTime、FrameDeltaTime 和 FramesPerSecond 为零，累计时间及计数停止；FixedDeltaTime 保留步长。重新 Play 或切换运行世界时归零，热重载保留时间。ElapsedTime 使用游戏帧的 dt 累加；物理累加器将单帧输入限制到 0.25 秒，因此卡顿时 FixedTime 不一定等于 ElapsedTime。这些参数描述游戏更新，不统计编辑器渲染帧，也不提供时间缩放。

```csharp
public override void OnUpdate(float dt)
{
    Object.Position += System.Numerics.Vector3.UnitX * 3f * Time.DeltaTime;
    if (Time.FrameCount % 60 == 0)
        Object.Text = $"运行 {Time.ElapsedTime:F1} 秒，当前 {Time.FramesPerSecond:F0} FPS";
}
```

所有公开脚本 API 的类型、方法、属性、枚举成员及方法参数均提供中文 XML 注释；包括生命周期、Mathf、输入、音频、物理查询、UI、对象状态和延迟创建。VS Code 可在悬停、成员补全和参数提示中读取这些说明，生成的 AudioAssets 成员也提供中文资源说明。更新 Atom.Managed 后需要完整构建并重启编辑器。

## 具名按键与移动预设

```csharp
public InputPreset Controls = InputPreset.Wasd; // Inspector 可改为 ArrowKeys
public KeyCode FireKey = KeyCode.Space;        // 可以配置、禁用或运行时调整

public override void OnUpdate(float dt)
{
    var move = Object.Input.Movement(Controls); // Vector2，键盘 + 左摇杆
    if (Object.Input.WasPressed(FireKey)) { /* 执行操作 */ }
    bool held = Object.Input.IsDown(GamepadButton.A);
    bool released = Object.Input.WasReleased(MouseButton.Left);
    float turn = Object.Input.Axis(GamepadAxis.RightX);
}
```

KeyCode 覆盖常用键盘键、A～Z、D0～D9、小键盘、F1～F24、左右修饰键；None 表示不绑定。MouseButton、GamepadButton、GamepadAxis 提供具名枚举，禁止无效枚举值。IsDown/WasPressed/WasReleased 分别表示持续按下、首次按下及松开，所有查询遵守游戏输入焦点。旧 int 参数重载继续兼容，新增脚本优先使用具名类型。

InputPreset.Wasd/ArrowKeys 都叠加左摇杆，移动向量长度限制为 1。自定义方向键可用 `Movement(forward, backward, left, right, horizontal, vertical)`，其中前四项为 KeyCode，后两项为 GamepadAxis。公开枚举字段可在 Inspector 下拉配置并在运行/暂停时修改，Stop 恢复编辑快照。

原生 Input 也支持 KeyCode 重载；UI 导航使用具名 Tab、Shift、方向键、Enter 和 Space。键值与托管枚举一致。

## 音频引用与资源名称提示

```csharp
using Atom;
using Game.Assets;

// 两种配置方式：拖拽字段，或选择自动生成的资源成员。
public AudioClip Effect = AudioClip.None;
public KeyCode PlayKey = KeyCode.Space;
public override void OnUpdate(float dt)
{
    if (Object.Input.WasPressed(PlayKey)) Object.PlayAudio(Effect, .65f);
    // 也可以直接调用：Object.PlayAudio(AudioAssets.SeMp3, .65f);
}
```

AudioClip 是 MP3/WAV 的强类型引用，公开字段在 Inspector 显示音频资源控件，支持从 Assets 拖拽。配置值随场景/预制体保存，取消覆盖恢复构造默认值；实时修改不会重建实例。None 不播放，PlayAudio 返回 false；资源引用本身不解码文件，实际播放时加载并缓存。`Object.PlayAudio()` 仍播放 Audio Source，`PlaySound(string)` 保留兼容。

每次脚本构建扫描 `Asset/Sounds` 下的 MP3/WAV，生成 `Managed/GameScripts/Generated/AudioAssets.g.cs`。成员位于 `Game.Assets.AudioAssets`，以相对目录、文件名及扩展名生成合法 C# 名称，例如 se.mp3 → SeMp3；清洗名称冲突时附加路径哈希，中文名称和资源路径正确转义。新增/删除/重命名资源后重新 Build scripts 或构建 C# 项目，VS Code 即可提示最新成员。删除或重命名已引用成员时需要调整源码；不会悄悄播放其他文件。生成文件不手动维护。

## 数学函数

`Atom.Mathf` 与原生 Math 的常用语义一致，继续使用 `System.Numerics.Vector2/3/4` 与 `Quaternion`。

- 常量：PI、TwoPI、HalfPI、Deg2Rad、Rad2Deg、Epsilon、Infinity、NegInfinity。
- 标量：Abs、Sqr、Sqrt、InvSqrt、Equal、IsNaN、IsFinite、Min、Max、Clamp、Clamp01、Floor、Ceil、Pow。
- 角度：DegreesToRadians、RadiansToDegrees、Sin、Cos、Tan、Asin、Acos、Atan、Atan2。三角函数参数/结果为弧度，Asin/Acos 的越界行为与原生一致。
- 插值：标量/向量 Lerp **不限制 t**；SmoothStep(edge0, edge1, value) 返回 0～1 权重。
- 向量：Length、Normalize、NormalizeOrZero、Dot、Cross、Distance、DistanceSquared、Floor。
- 旋转：AxisAngle(axis, radians)（归一化轴，零轴返回 Identity）、EulerRadians、EulerDegrees、Rotate、Slerp。欧拉合成采用引擎的 `qz * qx * qy` 顺序；Slerp 对应原生自由函数，默认 shortestPath=true。

原生 Math::Normalize 对精确零向量返回 (1,1,1)，Mathf.Normalize 保留这个约定；游戏方向计算可使用返回零向量的 NormalizeOrZero。Equal 的默认容差是 float 的机器 epsilon，Epsilon 常量为 1e-6。Clamp 的上下界必须有效；Clamp/Min/Max 同时提供整数重载。

```csharp
using Atom;
using System.Numerics;
float radians = Mathf.DegreesToRadians(90);
Vector3 direction = Mathf.NormalizeOrZero(target - Object.Position);
Object.Rotation = Mathf.EulerDegrees(new Vector3(0, 90, 0));
Object.Position = Mathf.Lerp(Object.Position, target, .1f);
```

## 对象创建与销毁

`Create(name, position)` 创建带 Transform 的空对象；`InstantiatePrefab(path, position)` 实例化预制体。二者返回 `SpawnRequest`，在安全点执行后可查询 `IsCompleted`、`Failed`、`Result`。Result 在等待时为 null，失败时抛异常并可在 Console 查看原因。

```csharp
private SpawnRequest? pending;
public override void OnCreate()
{
    pending = Object.InstantiatePrefab("Asset/Prefabs/PhysicsCube.prefab", new Vector3(0, 3, 0));
}
public override void OnUpdate(float dt)
{
    if (pending == null || !pending.IsCompleted) return;
    if (!pending.Failed && pending.Result is { IsValid: true } spawned)
        spawned.Visible = true;
    pending = null;
}
```

`Destroy()` 返回请求是否接受，销毁对象及其子树；当前回调结束前对象仍存在。对象操作队列在 Update、FixedUpdate 和碰撞回调的安全点提交，新实例的脚本下一帧启动。销毁会先调用 OnDestroy、停止该对象音频，再删除实体。重复销毁是安全的。每个待处理队列最多 256 项，未查询的创建结果最多 4096 项；应及时查询并释放请求引用。

操作必须在引擎脚本回调线程调用。预制体读取/资源构建目前仍是同步的，延迟执行代表避免回调期间修改 ECS，并不代表后台导入。

`IsValid` 同时检查实体版本与 World 代次，Stop/场景恢复后的旧引用不会因实体 ID 重用而重新有效。Visible 仅控制该对象的网格/UI，不会禁用物理和脚本。FindById 使用持久 ID，未找到时返回 null。

## 音频

在 Inspector → Add component → Audio Source 添加音频源，配置 clip、volume (0～1)、loop 和 playOnAwake；路径支持拖拽，Preview audio/Stop audio 用于试听。Assets 双击 .mp3/.wav 也可试听。

```csharp
Object.PlayAudio();                         // 播放该对象 Audio Source 的配置
Object.StopAudio();                         // 停止该对象发出的声音
bool ok = Object.PlaySound("Asset/Sounds/se.mp3", .65f); // 一次性效果
```

播放方法返回成功与否，文件/解码失败写入 Audio 日志，避免音效失败终止玩法。MP3/WAV 经 Media Foundation 解码后由 XAudio2 播放，重复效果使用独立声部，不会截断前一次。效果声部上限为 32；满载时回收最旧的非循环声部，保留循环声部。路径相对于程序工作目录解析，Debug 通常以 Engine 为工作目录。

AudioSource 支持场景/预制体保存、复制、撤销/重做，启动时预加载；Play 的暂停/恢复会同步声部，Stop 清理播放，销毁音频源会停止其循环声。当前为 **2D 音频**，完整文件解码，不包含空间衰减、混音总线或流式背景音乐。

幸存者 Demo 的 HUD 脚本字段 DeathSound 已配置示例音频，DeathSoundVolume 控制其音量；字段未指定时沿用该对象 Audio Source 配置。每次弹丸击杀播放一次，重开清理上一局效果。

## 脚本字段与实时调参

Inspector 的 Script class 下拉列出程序集中的具体 Behaviour 类型及公开可写实例字段，支持 bool、string、float/double、int/uint/long/ulong、枚举、Vector2/3/4、Quaternion 和 AudioClip。向量以 JSON 数组保存，数值使用 invariant culture。readonly、static 和复杂对象字段不作为编辑参数；元数据读取不执行用户构造函数。

勾选 Override 后使用类型化控件配置，取消覆盖使用构造默认值。Play 前校验类和全部覆盖值，错误包含脚本类型、对象持久 ID 和字段名。运行中可在 Hierarchy 选择对象，Inspector 显示当前实例字段；修改覆盖值或取消覆盖直接作用于原实例，不重跑 OnCreate。暂停时也可调参，玩法回调仍保持冻结。无效修改整项拒绝、保留原实例和字段值。Stop 恢复编辑快照，运行时调整不会写回编辑场景。

## 空间查询与碰撞过滤

```csharp
RaycastHit? hit = Object.Raycast(origin, direction, 30, mask: 1u << 2,
                                includeTriggers: false, ignore: Object);
if (hit is { } h) { GameObject target = h.Object; Vector3 point = h.Point; }
GameObject[] near = Object.OverlapSphere(Object.Position, 3, mask: 1u << 2);
```

Raycast 返回最近命中的 Object、Point、Normal、Distance；OverlapSphere 返回去重后的对象数组。使用 Jolt 实际碰撞形状及当前世界变换，停用碰撞体/刚体和非 Active 子树不参与查询。方向会归一化，默认查询全部层、排除 Trigger；长度/半径须为正有限值。原点位于实体形状内部时射线距离可为 0，按 Jolt 的表面法线约定返回。

Collider Inspector 可配置 0～31 的 Collision layer 和十六进制 Collision mask，XML/预制体保存这些值。实际碰撞要求双方掩码都允许对方层；查询只使用调用者提供的 mask。查询不负责阵营、伤害等游戏规则。

## Active 与组件启停

```csharp
Object.ActiveSelf = false;
bool active = Object.ActiveInHierarchy;
Object.SetComponentEnabled(ComponentKind.Collider, false);
bool localSwitch = Object.IsComponentEnabled(ComponentKind.Collider);
```

ActiveSelf 是本对象开关；ActiveInHierarchy 同时考虑祖先。组件开关支持 Mesh、Collider、RigidBody、Script、UI、Audio、Camera、Light、Joint，IsComponentEnabled 读取本地开关。Inspector 可操作这些开关，并随场景/预制体持久化。

每个挂载脚本 OnCreate 一次（包括初始停用对象），有效启用时 OnEnable，停用时 OnDisable；停用不重建实例或清空私有状态。停用的脚本不执行 Update/FixedUpdate/碰撞/UI 点击。最终删除先 Disable 再 Destroy。组件/对象停用会排除渲染、物理和 UI，音频暂停并保留播放位置，再启用继续；暂停游戏与对象暂停组合生效。Visible 仍只表示显示开关。

## UI 布局与导航

UiElement 的 responsive 默认 false，保持旧场景固定像素布局。启用后，anchorMin/anchorMax、pivot、position、size 相对于父 UI 或视窗布局，偏移和尺寸使用 1920×1080 参考单位并按视窗比例缩放。父 UI 可设 Horizontal/Vertical 子布局及 padding、spacing、layoutOrder。布局结果同时用于绘制和点击测试。

```csharp
Object.UI.Responsive = true;
Object.UI.Position = new Vector2(24, 24);
Object.UI.Progress = .5f;  // 对应 Inspector 的 Progress bar
Object.UI.Interactable = true;
```

脚本 UI 接口包含 Position、Size、AnchorMin、AnchorMax、Pivot、Progress、Visible、Interactable、Responsive。进度限定 0～1。可交互且有效显示的元素参与焦点导航：Tab/Shift+Tab、方向键或手柄 D-pad 切换，Enter/Space/手柄 A 触发 OnUiClick，焦点背景高亮。游戏输入焦点有效时接收键盘/手柄，不要求鼠标一直停留在视窗内。

## 显式热重载状态

Behaviour 提供 CaptureReloadState()、RestoreReloadState(string)、OnReload()。Capture 返回 null 沿用公开字段迁移及 OnCreate 的旧流程；返回字符串时，新实例先接收兼容公开字段并 Restore，再提交替换，调用 OnReload（不会调用 OnCreate）。有效启用实例随后调用 OnEnable。应在 OnReload 重建静态注册表，在下一次 Update 绑定其他脚本引用。

```csharp
private int ticks;
public override string CaptureReloadState() => ticks.ToString();
public override void RestoreReloadState(string state) => ticks = int.Parse(state);
public override void OnReload() { /* 重新登记静态列表 */ }
```

准备阶段加载、构造、类型和 Restore 失败保留旧实例与程序集。保存/恢复钩子应只处理状态值，避免修改世界；用户钩子的外部副作用无法回滚。提交后旧实例 Disable/Destroy、新实例 Reload/Create/Enable 的用户回调失败会报告错误，但不回退已提交的实例。私有对象图不自动遍历迁移。DEMO 显式保存时间、生命、等级、击杀、随机序列及对象池状态。

## 创建对象后应用预制体

```csharp
SpawnRequest created = Object.Create("Enemy", position);
// 在后续回调中等待 created.IsCompleted，再取 Result：
GameObject enemy = created.Result!;
SpawnRequest applied = enemy.ApplyPrefab("Asset/Prefabs/PhysicsCube.prefab");
// applied 完成后 Result 是同一个 enemy；请同时检查 Failed。
```

ApplyPrefab 为普通 GameObject 覆盖预制体中存在的组件属性并挂接其子对象，保留目标引用、持久 ID、名称、Transform、父级及 Active 开关；未在预制体中声明的目标组件保留。内部父子关系、关节引用与预制体链接重映射到目标。已经绑定预制体的目标会拒绝再次附加，以避免重复子树；创建完整实例可直接 InstantiatePrefab。资源/格式读取失败保留目标。应用在安全点同步执行，GPU 网格变更会等待已有提交完成，新脚本在后续帧同步。

## 验证

回归入口：--play-workflow-smoke-test（包含 Tests/EngineCompletionSmokeChecks.h）、--enhancement-smoke-test、--runtime-smoke-test。覆盖反射/类型校验、实时字段及默认值恢复、查询及双向碰撞过滤、Active 子树/组件状态、布局/命中/焦点切换、热重载状态及失败保留、模板/对象池撤销重做、脚本创建及预制体附加。真实手柄按键和多屏 DPI 的显示效果仍需设备人工验证。

本轮接口验证（2026-10-14）：Tests/ScriptApi 的实际托管接口检查覆盖命名输入状态、焦点、预设与对角归一化、资源播放路由、类型字段解析、原生键码一致性；Roslyn 验证键名/函数/引擎类/音频成员补全及 XML 提示。资源生成检查覆盖中文名称、冲突、扩展名过滤、内容不变时不重写、删除后刷新。原生工作流增加真实音频声部、反射、实时按键/音频字段、XML 往返和枚举控件检查。

验证结果：Debug/Release 原生构建及两种配置的工作流回归通过；Release 独立运行回归通过；ScriptApi 接口/资源目录/Roslyn 补全测试通过。构建未产生原生或 C# 编译警告。日志为 generated/script-api-{build,tests}.log 与 generated/demo-engine-{debug,release}-build.log。


## 项目场景接口

Behaviour 的 SceneManager 属性或 Object.SceneManager 提供 Scenes、CurrentScene、LoadScene、IsLoading、LastLoadError。LoadScene 仅加载 .atproj 清单内的场景，在脚本回调结束后的下一帧处理；失败保留旧世界和原脚本实例，成功后重置 Time 并使旧 GameObject 句柄失效。中文 XML 文档和 Roslyn 补全覆盖新接口。示例及发布说明见 [PROJECT_WORKFLOW.md](PROJECT_WORKFLOW.md)。

## C++ ECS 对象与组件接口

GameObject 是原生实体的带版本引用。以下查询直接遍历当前 World 的 C++ ECS 组件视图，不扫描托管脚本实例。结果是查询时的快照，长期缓存后应检查 IsValid；场景切换后旧引用失效。

```csharp
GameObject? player = Object.FindById("Player"); // 唯一持久 ID
GameObject? camera = Object.FindByName("Camera"); // 精确、区分大小写；含未激活对象
GameObject? other = Object.GetObject(otherEntity); // 当前世界的碰撞/查询实体句柄
GameObject[] meshes = Object.Query(ComponentKind.Mesh); // 层级激活且网格组件启用
GameObject[] allMeshes = Object.Query(ComponentKind.Mesh, activeOnly: false);
bool hasCamera = camera?.HasComponent(ComponentKind.Camera) == true;
if (player != null && camera != null)
    Object.SetActiveBatch([new(player, false), new(camera, true)]);
```

FindByName 重名时返回句柄最小者，需要唯一引用时优先使用 FindById。Query 返回按实体句柄排序的对象数组，MeshSource 与运行时 Mesh 去重。HasComponent 检查实际组件存在，IsComponentEnabled 只检查启用标记。SetActiveBatch 在原生层先验证全部实体，再批量写入 ObjectStateComponent；托管层拒绝其他世界或旧世界对象，空批次不跨 ABI，重复对象以最后一项为准。实体创建/销毁仍使用安全点命令队列。

Transform 的 SetPose 通过直接函数指针写入原生 TransformComponent；Visible、SetTint 和组件激活状态操作原生组件。脚本负责玩法逻辑与调用时机，原生 World 负责场景对象、组件数据和渲染/物理。业务生命值、奖励等状态不会因调用 Query 自动变成原生组件。

## 通用引擎服务（2026-10-15）

完整公开成员与参数见 [SCRIPT_REFERENCE.md](SCRIPT_REFERENCE.md)。新 API 必须提供中文 XML 注释并运行 `python Engine/Tools/Generate-ScriptReference.py`；规则见 AGENTS.md。

- UI 以1920×1080参考画布等比例缩放；Inspector提供角点、中心及拉伸锚点预设。脚本设置AnchorMin/AnchorMax会启用响应式布局；之后可显式设置Responsive=false。层级布局、绘制和命中使用同一Resolve结果。
- `new SaveStore("GameId")` 按游戏标识和槽位隔离，`Save<T>/Load<T>` 支持游戏自定义类型和公开字段，JSON损坏抛出异常并保留文件；序列化完成后同目录临时文件原子替换，旧槽位备份为.bak。版本与迁移由游戏定义。
- `Object.AudioMixer.MasterVolume` 为全局音量；Effects/Music/UI 三条总线由GetVolume/SetVolume控制，立即影响现有及新增声部。AudioSource在Inspector/XML中保存bus。
- 添加 GPU particle component，在独立粒子编辑窗口设置发射形状、常用预设、颜色、尺寸、速度、重力、纹理、广告牌与混色。公开 `ParticleComponent` 字段支持拖入场景对象或预制体实例组件，保存持久ID并在当前世界解析。`Object.GetParticleComponent()` / `Object.FindParticleComponent(idOrName)` 查询组件，`Play(worldPosition)` / `Stop()` 控制发射；实体销毁和场景重置按GPU fence回收资源。旧 `Object.Particles` 接口已移除，新预设采用 version 2 JSON。详情见 [GPU_PARTICLES.md](GPU_PARTICLES.md)。
- `Application.Quit()` 请求正常退出当前进程（编辑器中也会退出编辑器）。`Application.SetWindow(width,height,fullscreen)` 设置客户区或无边框全屏，渲染资源在下一帧安全点更新。

```csharp
var saves = new SaveStore("MyGame");
saves.Save("profile", myProfile);
var restored = saves.Load<MyProfile>("profile");
Object.AudioMixer.MasterVolume = .8f;
Object.AudioMixer.SetVolume(AudioBus.Music, .5f);
Object.GetParticleComponent()?.Play(Object.Position);
Application.SetWindow(1280, 720);
// 点击退出按钮时：Application.Quit();
```

编辑器视口UI坐标（2026-10-15修正）：普通和放大Play视口使用实际UI合成缓冲尺寸计算布局与点击，3D场景分辨率与其不同也不会改变UI锚点位置。3D拾取单独使用场景像素坐标。Standalone窗口尺寸诊断不能代替编辑器视口回归。

底层Overlay资源约束（2026-10-15）：主Overlay跟随交换链输出大小，重建3D native resolution时不会改变UI尺寸。离屏视图的Overlay跟随该视图目标。Composite按输出像素读取主Overlay，故必须保证两者尺寸一致；仅检查世界的uiRenderSize不足以验收，应检查最终GPU合成像素。
