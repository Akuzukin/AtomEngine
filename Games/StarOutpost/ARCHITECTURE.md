# 对象脚本与动态生成

`OutpostRuntime` 只持有本局状态、存档与60Hz固定时钟，不读取或修改敌人/弹丸等对象的Transform，也不创建它们。

|GameObject / 资源|脚本|职责|
|---|---|---|
|Player|PlayerController|WASD/方向键、鼠标瞄准、玩家移动与表现|
|Camera|CameraController|跟随与震动|
|EnemySpawner|EnemySpawner|普通波次、90秒精英、5分钟Boss；按需实例化敌人和预警预制体|
|Enemy.prefab实例|EnemyController|该敌人的AI、攻击、前摇与受击表现|
|Weapon0～Weapon6|WeaponController|该武器独立冷却、自动攻击与进化行为|
|ProjectileSpawner / Projectile.prefab|ProjectileSpawner / ProjectileController|按需生成、每颗弹丸移动、碰撞、穿透去重与回收|
|DropSpawner / Drop.prefab|DropSpawner / DropController|掉落生成、磁吸、一次性拾取与过期结算|
|EffectSpawner及效果预制体|EffectController、CorpseController、DamageNumberController|粒子生命周期、碎尸显示、伤害数字投影|
|Telegraph / OrbitBlade / BlackHole / Supply|各自Controller|预警、月刃、黑洞与补给站表现|
|UI根与按钮|OutpostUI / OutpostButton|菜单、HUD、升级、商店、结算与交互|
|AudioController|AudioController|音效节流、播放与静音|

各控制器注册自己的固定步回调，由战斗时钟按“移动→波次→武器→弹丸→死亡→拾取→敌人攻击”顺序调度，避免依赖引擎ECS脚本遍历顺序。纯规则位于对应 `*Logic.cs`，原生对象脚本与无窗口测试复用同一份规则。

## 按需对象池

正式场景147个对象，启动时敌人、弹丸、掉落、粒子、碎尸及伤害数字实例均为0。场景只包含少量固定玩法控制对象、UI和静态环境。

`ActorPool` 初始为空；模型槽活跃时才提交InstantiatePrefab请求。EnemySpawner每帧最多提交4个新敌人请求，其余池也按小批量扩容，遵守引擎256请求上限。异步请求完成后按实体ID绑定对应模型槽，对象脚本下一帧注册其行为。尚未绑定的敌人不能攻击，也不能被武器当作目标。

失活对象保留用于复用，同时停用其GameObject，减少空脚本更新。达到容量上限时复用现有实例；敌人按出生顺序回收。规则层预留的是轻量数据槽，不是已经生成的GameObject。

激活/停用变更由各对象池收集，通过 `GameObject.SetActiveBatch` 一次提交到 C++ ECS 的 ObjectStateComponent；死亡、复用和返回菜单无需为每个对象单独序列化一次激活请求。空批次不会调用原生层。

压力测试由单独的GameDiagnostics脚本触发，只有RuntimeCheck.scene包含它；正式场景不包含任何自动开始、无敌或600敌人初始化代码。GameplayCheck.scene用于验证真实开局的渐进生成。

## 方向

输入正Y映射到世界正Z；当前相机位于负Z一侧，因此W/↑投影到屏幕上方，S/↓投影到屏幕下方。PlayerController不再对Y输入取反，测试覆盖实际相机投影方向。

## 原生 ECS 接口与固定时间

PlayerController 在创建时通过 `Object.FindByName("Camera")` 获取原生相机实体，并使用 `HasComponent(Camera)` 检查组件。UI 保留按持久 ID 的原生查找。脚本可通过 `GetObject(entity)` 解析碰撞实体，通过 `Query(ComponentKind, activeOnly)` 获取 C++ 组件视图快照；对象变换由 `SetPose` 直接写入原生 TransformComponent，显隐、材质与组件启用同样操作原生 ECS。

击中和死亡只触发闪白、粒子、伤害数字、碎尸和震动，不再缩放整个固定 Tick 的 dt。移动、武器、冷却、波次和比赛时间始终共享 1/60 秒步长，避免连续击杀导致输入仿佛停顿。升级、暂停、补给界面的明确暂停规则保留。

玩法数据槽仍保存生命、伤害、奖励和结算身份；场景实体、Transform、Mesh、Camera、ObjectState 等由原生 ECS 持有。当前接口没有把这些业务数据注册为新的 C++ 自定义组件，也没有将武器伤害算法改写为 C++ 系统。

特效调用为可回退的表现层：EffectSpawner绑定时检查实际Particle组件；原生爆裂回调返回bool，成功时不重复生成mesh，不可用时进入几何池。运行时原生调用失败只停用对应发射器并提示，不向战斗时钟传播异常。
