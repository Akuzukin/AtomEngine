# GPU 粒子组件 v2

旧 Particle / ParticleEditor 类、旧发射计数与间接绘制 shader 已删除。新的 ParticleComponent 作为独立 ECS 组件保存参数，ParticleSystem 负责固定容量 GPU 模拟与实例化绘制，脚本负责播放时机。

## 编辑

在 Assets 工具栏点击“新建粒子”，或在资源列表空白处右键选择“新建粒子”，会在当前目录创建不重名的 `.particle` 资产并打开独立编辑窗口。双击已有 `.particle` 可重新编辑。使用 Save .particle 保存，Save as .particle... 另存为；场景组件也可以导出为 `.particle` 并绑定资产。资产编辑不会增加或修改场景对象，关闭有修改的资产窗口会提示保存、放弃或取消；切换文档前需保存或 Revert。

`.particle` 使用版本化 JSON 内容，保存前校验参数并通过同目录临时文件原子替换。Inspector 的粒子组件可直接接收 `.particle` 拖拽；脚本的 ParticleComponent 字段也可接收该资产，编辑器将创建对应场景组件并绑定持久ID。Assets 详情中的 Create particle object 可显式创建组件对象。发行工具递归收集 `.particle` 内引用的纹理。

1. 在 Inspector 的 Add component 中添加 GPU particle component。
2. 点击 Edit in particle preview window 打开独立编辑窗口。左侧参数、右侧预览；拖动预览旋转相机，滚轮缩放，Replay 重播，Preview running 暂停预览。
3. 选择 Burst、Smoke、Fountain、Rain、Ring 常用预设，继续调整参数。支持 Point、Box、Sphere、Cone、Ring 发射形状。
4. Billboard 开关控制面向相机或固定世界 XY 平面的四边形。纹理字段可从 Assets 拖入纹理，留空仅使用颜色。混色支持 Alpha、Additive、Multiply、Opaque；Opaque 不使用透明度混合。
5. 容量为 1–65536，Burst 为瞬时发射数量，Rate 为每秒数量；Loop 持续发射，否则 Duration 后停止。生命周期、尺寸、颜色渐变、速度、阻力、重力、半径、锥角及盒范围可调整。Stop 停止新发射，已有粒子继续消退；Play 可再次播放，复用组件 GPU 存储。
6. 参数保存到场景或预制体的 ParticleComponent/Settings。编辑资产预设的组件时，首次修改参数会复制为组件内联参数；原 JSON 资产保留。独立预览不进入场景的渲染列表，也不改变场景中的发射实例。

## 脚本引用

```csharp
using Atom;
public sealed class HitFx : Behaviour
{
    public ParticleComponent Impact;
    public override void OnCreate()
    {
        if (Impact.IsValid) Impact.Play(Object.Position);
    }
}
```

启用 Inspector 中字段覆盖后，把拥有粒子组件的 Hierarchy 对象或 Inspector 的 GPU particle component 标题拖到 Impact 字段。也可从 Assets 直接拖入粒子 prefab：编辑器将其实例化到当前场景并绑定组件，优先选择根对象的粒子组件，否则要求恰好一个子粒子组件；无组件或存在歧义时拒绝且不修改场景。引用保存为 `particle:<PersistentId>`，复制子树、预制体实例化和预制体更新都会重映射内部引用。脚本也可以通过 InstantiatePrefab 请求实例化后查询组件。

`Object.GetParticleComponent()` 查询当前对象，`Object.FindParticleComponent("HitFx")` 按 ID 或名称查询当前世界。没有组件返回 null；未分配引用 IsValid 为 false，Play/Stop 对未分配或过期引用报告异常。字段在 OnCreate 前、热更新和热重载时绑定世界。查询得到的引用同样保留持久ID。

## 底层与迁移

每个组件持有固定容量 StructuredBuffer，GPU 执行形状采样、运动、老化、尺寸与颜色更新；CPU 累积小数发射数量，避免高帧率时低发射率被舍去。场景模拟每帧一次，多视图不重复推进。句柄包含槽位和代次，回收后过期句柄无法操作复用槽位；GPU 资源按各队列 fence 延迟释放。

新场景标签为 ParticleComponent，preset 可引用 version 2 JSON，Settings 为内联 JSON。加载器保留旧 ParticleEmitter XML 的读取迁移入口，但旧 JSON 格式和旧托管 ParticleEmitter 类不再使用。仓库粒子资产、游戏场景、资产生成器及发行 shader 依赖已迁移。原资产备份在 generated/particle-v1-migration。

旧脚本 `Object.Particles.Emit(position)` 改为 `Object.GetParticleComponent()?.Play(position)`。完整 API 参数见 [SCRIPT_REFERENCE.md](SCRIPT_REFERENCE.md)。

## 验证

重新编译 AtomEngine 并部署引擎测试脚本后，运行 `--play-workflow-smoke-test` 检查 GPU 发射数量、停止和生命周期、句柄回收、组件 XML、公开字段热更新/热重载、预制体引用以及独立预览。项目启动器会部署游戏脚本，不能用这些游戏脚本代替引擎测试脚本运行该回归。ScriptApiTests 检查字段编码、中文 XML 文档和 Roslyn 补全。GPU 预览帧可导出检查，发行包需要包含 ParticleV2 CS/VS/PS/hlsli。
