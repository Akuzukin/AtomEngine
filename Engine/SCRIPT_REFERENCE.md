# AtomEngine 脚本包 Reference

来源：Atom.Managed 公共 API 的 XML 文档。先编译 SDK，再运行 `python Engine/Tools/Generate-ScriptReference.py`。新增或修改 API 必须更新 XML 注释并重新生成本文件。

脚本使用 `using Atom;` 并继承 Behaviour。`new SaveStore("GameId")` 支持游戏自定义存档类型；`Object.AudioMixer` 控制音量总线；`Object.GetParticleComponent()` / `Object.FindParticleComponent(idOrName)` 查询粒子组件，`public ParticleComponent Effect;` 可接收 Inspector 组件拖拽，使用 `Effect.Play(position)` / `Effect.Stop()` 控制发射；`Application.Quit()` 正常退出当前进程。生命周期说明见 SCRIPT_API.md，粒子组件编辑与迁移见 GPU_PARTICLES.md。

编辑器 UI 布局与点击坐标使用实际 UI 合成缓冲；普通/放大视口和窗口缩放均同步其尺寸，3D拾取另用场景像素。排查坐标问题不能用场景渲染分辨率代替 UI 分辨率。

底层主 Overlay 资源始终使用交换链像素尺寸，独立于3D渲染分辨率；离屏Overlay使用本视图尺寸。尺寸字段检查之外，锚点须验证最终GPU合成结果。

## `T:Atom.Application`

当前原生应用的生命周期接口。


## `M:Atom.Application.Quit`

请求正常退出当前原生应用，包括编辑器进程；不强制终止进程。


## `M:Atom.Application.SetWindow(System.Int32,System.Int32,System.Boolean)`

设置当前应用窗口的客户区大小与全屏状态，在窗口消息处理后更新渲染缓冲。

- `width`：窗口模式客户区宽度，320到16384。
- `height`：窗口模式客户区高度，240到16384。
- `fullscreen`：是否使用无边框全屏；全屏尺寸由当前显示器决定。

## `T:Atom.GameObject`

游戏对象引用，绑定实体版本和世界代次；提供变换、输入、音频、UI 和延迟创建接口。


## `P:Atom.GameObject.Application`

当前对象所属原生应用的生命周期接口。


## `M:Atom.GameObject.PlayAudio(Atom.AudioClip,System.Single)`

播放指定音频引用，创建独立的音效实例；未指定引用返回假。

- `clip`：Inspector 配置的音频引用或生成的 AudioAssets 成员。
- `volume`：音量，通常为 0～1，原生音频服务限制有限数值的范围。

## `P:Atom.GameObject.AudioMixer`

当前应用的原生音频混音接口。


## `P:Atom.GameObject.Entity`

当前对象的运行时实体句柄；跨世界或对象销毁后不可继续使用。


## `P:Atom.GameObject.IsValid`

对象仍存在且所属世界代次未改变时为真；未绑定的引用返回假。


## `M:Atom.GameObject.PlayAudio`

播放此对象在 Inspector 配置的 Audio Source；未配置或播放失败时返回假。


## `M:Atom.GameObject.StopAudio`

停止此对象拥有的所有音频播放实例。


## `M:Atom.GameObject.PlaySound(System.String,System.Single)`

按资源路径播放独立音效；推荐使用 AudioClip 重载避免调用处硬编码路径。

- `path`：资源路径，可相对于引擎工作目录或使用绝对路径。
- `volume`：音量，通常为 0～1，原生音频服务限制有限数值的范围。

## `M:Atom.GameObject.InstantiatePrefab(System.String,System.Numerics.Vector3)`

将实例化预制体加入命令队列；回调安全点执行后可轮询返回请求，新增脚本下一帧初始化。

- `path`：资源路径，可相对于引擎工作目录或使用绝对路径。
- `position`：新对象的局部位置。

## `M:Atom.GameObject.ApplyPrefab(System.String)`

将预制体属性和子对象延迟附加到此普通对象，保留目标身份和变换；已有预制体实例不能再次附加。

- `path`：资源路径，可相对于引擎工作目录或使用绝对路径。

## `M:Atom.GameObject.Create(System.String,System.Numerics.Vector3)`

延迟创建带 Transform 的具名游戏对象，返回可轮询的请求。

- `name`：新对象名称。
- `position`：新对象的局部位置。

## `M:Atom.GameObject.Destroy`

将此对象及其子树的销毁加入队列，在回调安全点执行；无效对象返回假。


## `M:Atom.GameObject.FindById(System.String)`

按持久化 ID 在当前世界查找对象；不存在时返回 null。

- `persistentId`：场景持久化 ID。

## `P:Atom.GameObject.Visible`

控制可渲染组件的可见性；缺少可渲染组件时抛出异常，不会停用脚本或物理。


## `P:Atom.GameObject.Position`

相对于父对象的局部位置。


## `P:Atom.GameObject.Rotation`

局部旋转四元数，写入前进行归一化。


## `P:Atom.GameObject.Scale`

局部缩放，各分量必须为正。


## `P:Atom.GameObject.Text`

UI 元素的文本；对象没有 UI 组件时抛出异常。


## `P:Atom.GameObject.LinearVelocity`

刚体线速度；没有刚体或写入非法速度时抛出异常。


## `M:Atom.GameObject.IsMouseDown(System.Int32)`

查询鼠标按键是否持续按下，使用原生数字编号；推荐使用 MouseButton 重载。

- `button`：要查询的鼠标或手柄按键。

## `M:Atom.GameObject.IsGamepadDown(System.Int32)`

查询手柄按键是否持续按下，使用原生数字编号；推荐使用 GamepadButton 重载。

- `button`：要查询的鼠标或手柄按键。

## `M:Atom.GameObject.IsKeyPressed(System.Int32)`

查询当前帧是否首次按下指定 Win32 虚拟键；推荐使用 KeyCode 重载。

- `virtualKey`：Win32 虚拟键编号。

## `M:Atom.GameObject.GamepadAxis(System.Int32)`

读取原生编号的手柄轴，摇杆范围 -1～1，扳机范围 0～1；推荐使用具名重载。

- `axis`：要读取的手柄轴。

## `M:Atom.GameObject.IsKeyDown(System.Int32)`

查询 Win32 虚拟键是否持续按下；查询遵守游戏输入焦点限制。

- `virtualKey`：Win32 虚拟键编号。

## `P:Atom.GameObject.PersistentId`

此对象在场景中的持久化ID，可用于组件引用、存档与预制体实例查询。


## `M:Atom.GameObject.FindByName(System.String)`

按名称在当前世界的 C++ ECS 中精确查找对象，包含未激活对象；同名时返回实体句柄最小的对象，不存在返回 null。

- `name`：区分大小写的完整场景对象名称；需要唯一定位时使用 FindById。

## `M:Atom.GameObject.GetObject(System.UInt32)`

将当前世界的原生实体句柄解析为对象引用；实体已销毁或版本不符时返回 null，句柄不可跨世界使用。

- `entity`：来自当前世界查询或碰撞回调的实体句柄。

## `M:Atom.GameObject.HasComponent(Atom.ComponentKind)`

检查此对象是否实际拥有指定 C++ ECS 组件；与组件启用状态无关。

- `component`：需要检查的组件类别。

## `M:Atom.GameObject.Query(Atom.ComponentKind,System.Boolean)`

通过 C++ ECS 组件视图查询当前世界，返回按实体句柄排序的对象快照；网格源与运行时网格合并去重。

- `component`：要求对象拥有的组件类别。
- `activeOnly`：为真时仅包含层级激活且该组件启用的对象；为假时包含未激活对象。

## `M:Atom.GameObject.SetActiveBatch(System.Collections.Generic.IReadOnlyList{Atom.ObjectActivation})`

一次原生调用批量修改 ObjectStateComponent，先校验全部对象再提交；不销毁对象，停用脚本与物理遵循引擎生命周期。

- `changes`：同一世界对象的状态变更列表；空列表不调用原生层，同一对象多次出现时以最后一次为准。

## `P:Atom.GameObject.ActiveSelf`

此对象自身的激活状态；父对象的状态不改变此值。


## `P:Atom.GameObject.ActiveInHierarchy`

此对象及父层级全部激活时为真。


## `M:Atom.GameObject.IsComponentEnabled(Atom.ComponentKind)`

读取此对象的组件启用标记；不代表组件一定存在或父层级激活。

- `component`：组件类别。

## `M:Atom.GameObject.SetComponentEnabled(Atom.ComponentKind,System.Boolean)`

设置组件启用标记；不会移除组件，脚本实例在重新启用时复用。

- `component`：组件类别。
- `enabled`：是否启用该组件。

## `M:Atom.GameObject.GetParticleComponent`

查询此对象或预制体实例上的粒子组件；不存在返回null。


## `M:Atom.GameObject.FindParticleComponent(System.String)`

按持久ID或对象名称查找当前世界中的预制粒子组件。

- `idOrName`：场景或预制体实例对象的持久ID或名称。

## `M:Atom.GameObject.Raycast(System.Numerics.Vector3,System.Numerics.Vector3,System.Single,System.UInt32,System.Boolean,Atom.GameObject)`

沿世界坐标射线查询最近的碰撞体；方向自动归一化，未命中返回 null，可过滤碰撞层、触发器和指定对象。

- `origin`：射线起点，世界坐标。
- `direction`：非零有限射线方向，内部归一化。
- `distance`：射线最大距离，必须为正且有限。
- `mask`：碰撞层位掩码，第 n 位对应第 n 层，默认查询全部层。
- `includeTriggers`：是否包含触发器，默认不包含。
- `ignore`：可忽略的同一世界对象，null 表示不忽略。

## `M:Atom.GameObject.OverlapSphere(System.Numerics.Vector3,System.Single,System.UInt32,System.Boolean,Atom.GameObject)`

查询与世界坐标球体重叠的碰撞体，返回对象数组；无结果时为空数组。

- `center`：球心，世界坐标。
- `radius`：球体半径，必须为正且有限。
- `mask`：碰撞层位掩码，第 n 位对应第 n 层，默认查询全部层。
- `includeTriggers`：是否包含触发器，默认不包含。
- `ignore`：可忽略的同一世界对象，null 表示不忽略。

## `M:Atom.GameObject.SetPose(System.Numerics.Vector3,System.Numerics.Quaternion,System.Numerics.Vector3)`

一次提交完整局部变换，避免分别写入位置、旋转、缩放时重复读取变换。

- `position`：有限的局部位置。
- `rotation`：非零且有限的四元数，由原生接口归一化。
- `scale`：各分量大于零的有限局部缩放。

## `M:Atom.GameObject.SetTint(System.Numerics.Vector3,System.Single)`

设置全部材质槽的 RGB 颜色与自发光倍数（0～10）；对象必须拥有材质组件。

- `color`：非负且有限的 RGB 颜色值。
- `glow`：自发光倍数，范围为零到十。

## `P:Atom.GameObject.Pointer`

读取游戏视口鼠标；失去焦点或移出视口时 Valid 为假。


## `P:Atom.GameObject.SceneManager`

当前对象所属运行上下文的项目场景接口。


## `P:Atom.GameObject.Input`

此对象的具名输入接口，支持按键状态与移动预设。


## `M:Atom.GameObject.IsKeyDown(Atom.KeyCode)`

查询具名键盘按键是否持续按下；None 表示不绑定。

- `key`：要查询的键盘按键；None 返回假。

## `M:Atom.GameObject.IsKeyPressed(Atom.KeyCode)`

查询具名键盘按键是否在当前帧首次按下；None 表示不绑定。

- `key`：要查询的键盘按键；None 返回假。

## `M:Atom.GameObject.GamepadAxis(Atom.GamepadAxis)`

读取具名手柄轴，摇杆范围 -1～1，扳机范围 0～1。

- `axis`：要读取的手柄轴。

## `M:Atom.GameObject.IsGamepadDown(Atom.GamepadButton)`

查询具名手柄按键是否持续按下。

- `button`：要查询的鼠标或手柄按键。

## `M:Atom.GameObject.IsMouseDown(Atom.MouseButton)`

查询具名鼠标按键是否持续按下。

- `button`：要查询的鼠标或手柄按键。

## `P:Atom.GameObject.UI`

访问此对象的 UI 组件；缺少组件时，属性读写抛出异常。


## `T:Atom.Behaviour`

引擎脚本基类；重写生命周期回调，公开可写字段可在 Inspector 配置和运行时调整。


## `P:Atom.Behaviour.Application`

当前脚本所属原生应用的生命周期接口。


## `P:Atom.Behaviour.Object`

此脚本绑定的游戏对象，由引擎在 OnCreate 之前设置。


## `M:Atom.Behaviour.OnCreate`

实例创建时调用一次，包括初始未激活的对象；其他脚本可能尚未初始化。


## `M:Atom.Behaviour.OnEnable`

对象、脚本组件及脚本附件均启用时调用；重新启用复用原实例。


## `M:Atom.Behaviour.OnDisable`

脚本离开启用状态时调用；可用于清理持续行为。


## `M:Atom.Behaviour.OnUpdate(System.Single)`

每个游戏帧调用一次；当前帧的脚本初始化全部完成后执行，暂停或停用时不调用。

- `dt`：本次更新间隔，单位秒，可通过 Time.DeltaTime 读取。

## `M:Atom.Behaviour.OnFixedUpdate(System.Single)`

每个物理固定步调用一次，通常为 1/60 秒；一帧可能执行零次或多次。

- `dt`：本次更新间隔，单位秒，可通过 Time.DeltaTime 读取。

## `M:Atom.Behaviour.OnCollision(System.UInt32,System.Boolean)`

物理碰撞或触发器接触回调；Time.DeltaTime 使用固定步长。

- `other`：对方对象的运行时实体句柄。
- `trigger`：此接触是否涉及触发器。

## `M:Atom.Behaviour.OnUiClick`

此对象的可交互 UI 被鼠标、键盘或手柄激活时调用。


## `M:Atom.Behaviour.OnDestroy`

脚本销毁、停止游戏或热重载替换旧实例时调用；不要依赖此时对象仍然有效。


## `M:Atom.Behaviour.CaptureReloadState`

热重载前返回需要保留的私有状态字符串；默认 null 表示重新走 OnCreate。


## `M:Atom.Behaviour.RestoreReloadState(System.String)`

热重载的新实例接收旧实例保存的状态；抛出异常会取消此次替换。

- `state`：旧实例 CaptureReloadState 返回的状态字符串。

## `M:Atom.Behaviour.OnReload`

成功恢复非 null 的热重载状态后调用，替代 OnCreate；公开字段由引擎迁移。


## `P:Atom.Behaviour.SceneManager`

当前脚本的项目场景接口；在生命周期方法中直接调用 SceneManager.LoadScene。


## `T:Atom.AudioClip`

MP3/WAV 音频资源引用；公开脚本字段支持从 Inspector 拖拽赋值，不会在构造时解码文件。


## `P:Atom.AudioClip.Path`

引擎使用的资源路径；未指定音频时为空字符串。


## `P:Atom.AudioClip.IsAssigned`

已经指定非空资源路径时为真。


## `P:Atom.AudioClip.None`

未指定的音频引用，适合作为 Inspector 可配置字段的默认值。


## `M:Atom.AudioClip.#ctor(System.String)`

创建音频引用；空白路径表示未指定，仅接受 MP3/WAV，不会立即加载文件。

- `path`：资源路径，可相对于引擎工作目录或使用绝对路径。

## `M:Atom.AudioClip.ToString`

返回资源路径，用于诊断和序列化。


## `T:Atom.AudioBus`

原生音频混音总线。


## `F:Atom.AudioBus.Effects`

游戏音效。


## `F:Atom.AudioBus.Music`

背景音乐。


## `F:Atom.AudioBus.UI`

界面音效。


## `T:Atom.AudioMixer`

控制当前应用的全局音量与总线，变更立即影响正在播放和新播放的声部。


## `P:Atom.AudioMixer.MasterVolume`

全局音量，范围0到1。


## `M:Atom.AudioMixer.GetVolume(Atom.AudioBus)`

读取总线音量。

- `bus`：声音总线。

## `M:Atom.AudioMixer.SetVolume(Atom.AudioBus,System.Single)`

设置总线音量，范围0到1。

- `bus`：声音总线。
- `volume`：目标音量。

## `M:Atom.AudioMixer.Play(Atom.AudioClip,Atom.AudioBus,System.Single)`

在指定原生总线上播放音效。

- `clip`：音频资源。
- `bus`：声音总线。
- `volume`：声部音量。

## `T:Atom.Managed.Bridge`

引擎内部托管桥，负责程序集加载、反射和脚本生命周期分派；游戏脚本使用 Atom 命名空间的接口。


## `M:Atom.Managed.Bridge.Dispatch(System.Byte*,System.Void*,System.IntPtr)`

接收原生引擎的 JSON 请求并调用对应脚本接口，返回零表示成功。

- `request`：以零结尾的 UTF-8 请求。
- `apiPointer`：版本匹配的原生函数表。
- `context`：当前世界的原生上下文。

## `T:Atom.ObjectActivation`

原生 ECS 对象的激活状态变更，用于一次提交多个实体。

- `Object`：当前世界中的有效游戏对象。
- `Active`：是否激活对象自身。

## `M:Atom.ObjectActivation.#ctor(Atom.GameObject,System.Boolean)`

原生 ECS 对象的激活状态变更，用于一次提交多个实体。

- `Object`：当前世界中的有效游戏对象。
- `Active`：是否激活对象自身。

## `P:Atom.ObjectActivation.Object`

当前世界中的有效游戏对象。


## `P:Atom.ObjectActivation.Active`

是否激活对象自身。


## `T:Atom.KeyCode`

与 Win32 虚拟键一致的具名键盘按键；推荐替代数字编号。


## `F:Atom.KeyCode.None`

不绑定按键。


## `F:Atom.KeyCode.Backspace`

退格键。


## `F:Atom.KeyCode.Tab`

制表键。


## `F:Atom.KeyCode.Enter`

回车键。


## `F:Atom.KeyCode.Shift`

任一 Shift 键。


## `F:Atom.KeyCode.Control`

任一 Ctrl 键。


## `F:Atom.KeyCode.Alt`

任一 Alt 键。


## `F:Atom.KeyCode.Pause`

暂停键。


## `F:Atom.KeyCode.CapsLock`

大写锁定键。


## `F:Atom.KeyCode.Escape`

退出键。


## `F:Atom.KeyCode.Space`

空格键。


## `F:Atom.KeyCode.PageUp`

上一页键。


## `F:Atom.KeyCode.PageDown`

下一页键。


## `F:Atom.KeyCode.End`

末尾键。


## `F:Atom.KeyCode.Home`

首页键。


## `F:Atom.KeyCode.Left`

左方向键。


## `F:Atom.KeyCode.Up`

上方向键。


## `F:Atom.KeyCode.Right`

右方向键。


## `F:Atom.KeyCode.Down`

下方向键。


## `F:Atom.KeyCode.PrintScreen`

截屏键。


## `F:Atom.KeyCode.Insert`

插入键。


## `F:Atom.KeyCode.Delete`

删除键。


## `F:Atom.KeyCode.D0`

主键盘数字 0 键。


## `F:Atom.KeyCode.D1`

主键盘数字 1 键。


## `F:Atom.KeyCode.D2`

主键盘数字 2 键。


## `F:Atom.KeyCode.D3`

主键盘数字 3 键。


## `F:Atom.KeyCode.D4`

主键盘数字 4 键。


## `F:Atom.KeyCode.D5`

主键盘数字 5 键。


## `F:Atom.KeyCode.D6`

主键盘数字 6 键。


## `F:Atom.KeyCode.D7`

主键盘数字 7 键。


## `F:Atom.KeyCode.D8`

主键盘数字 8 键。


## `F:Atom.KeyCode.D9`

主键盘数字 9 键。


## `F:Atom.KeyCode.A`

字母 A 键。


## `F:Atom.KeyCode.B`

字母 B 键。


## `F:Atom.KeyCode.C`

字母 C 键。


## `F:Atom.KeyCode.D`

字母 D 键。


## `F:Atom.KeyCode.E`

字母 E 键。


## `F:Atom.KeyCode.F`

字母 F 键。


## `F:Atom.KeyCode.G`

字母 G 键。


## `F:Atom.KeyCode.H`

字母 H 键。


## `F:Atom.KeyCode.I`

字母 I 键。


## `F:Atom.KeyCode.J`

字母 J 键。


## `F:Atom.KeyCode.K`

字母 K 键。


## `F:Atom.KeyCode.L`

字母 L 键。


## `F:Atom.KeyCode.M`

字母 M 键。


## `F:Atom.KeyCode.N`

字母 N 键。


## `F:Atom.KeyCode.O`

字母 O 键。


## `F:Atom.KeyCode.P`

字母 P 键。


## `F:Atom.KeyCode.Q`

字母 Q 键。


## `F:Atom.KeyCode.R`

字母 R 键。


## `F:Atom.KeyCode.S`

字母 S 键。


## `F:Atom.KeyCode.T`

字母 T 键。


## `F:Atom.KeyCode.U`

字母 U 键。


## `F:Atom.KeyCode.V`

字母 V 键。


## `F:Atom.KeyCode.W`

字母 W 键。


## `F:Atom.KeyCode.X`

字母 X 键。


## `F:Atom.KeyCode.Y`

字母 Y 键。


## `F:Atom.KeyCode.Z`

字母 Z 键。


## `F:Atom.KeyCode.LeftWindows`

左 Windows 键。


## `F:Atom.KeyCode.RightWindows`

右 Windows 键。


## `F:Atom.KeyCode.Menu`

菜单键。


## `F:Atom.KeyCode.NumPad0`

小键盘数字 0 键。


## `F:Atom.KeyCode.NumPad1`

小键盘数字 1 键。


## `F:Atom.KeyCode.NumPad2`

小键盘数字 2 键。


## `F:Atom.KeyCode.NumPad3`

小键盘数字 3 键。


## `F:Atom.KeyCode.NumPad4`

小键盘数字 4 键。


## `F:Atom.KeyCode.NumPad5`

小键盘数字 5 键。


## `F:Atom.KeyCode.NumPad6`

小键盘数字 6 键。


## `F:Atom.KeyCode.NumPad7`

小键盘数字 7 键。


## `F:Atom.KeyCode.NumPad8`

小键盘数字 8 键。


## `F:Atom.KeyCode.NumPad9`

小键盘数字 9 键。


## `F:Atom.KeyCode.Multiply`

小键盘乘号键。


## `F:Atom.KeyCode.Add`

小键盘加号键。


## `F:Atom.KeyCode.Subtract`

小键盘减号键。


## `F:Atom.KeyCode.Decimal`

小键盘小数点键。


## `F:Atom.KeyCode.Divide`

小键盘除号键。


## `F:Atom.KeyCode.F1`

F1 功能键。


## `F:Atom.KeyCode.F2`

F2 功能键。


## `F:Atom.KeyCode.F3`

F3 功能键。


## `F:Atom.KeyCode.F4`

F4 功能键。


## `F:Atom.KeyCode.F5`

F5 功能键。


## `F:Atom.KeyCode.F6`

F6 功能键。


## `F:Atom.KeyCode.F7`

F7 功能键。


## `F:Atom.KeyCode.F8`

F8 功能键。


## `F:Atom.KeyCode.F9`

F9 功能键。


## `F:Atom.KeyCode.F10`

F10 功能键。


## `F:Atom.KeyCode.F11`

F11 功能键。


## `F:Atom.KeyCode.F12`

F12 功能键。


## `F:Atom.KeyCode.F13`

F13 功能键。


## `F:Atom.KeyCode.F14`

F14 功能键。


## `F:Atom.KeyCode.F15`

F15 功能键。


## `F:Atom.KeyCode.F16`

F16 功能键。


## `F:Atom.KeyCode.F17`

F17 功能键。


## `F:Atom.KeyCode.F18`

F18 功能键。


## `F:Atom.KeyCode.F19`

F19 功能键。


## `F:Atom.KeyCode.F20`

F20 功能键。


## `F:Atom.KeyCode.F21`

F21 功能键。


## `F:Atom.KeyCode.F22`

F22 功能键。


## `F:Atom.KeyCode.F23`

F23 功能键。


## `F:Atom.KeyCode.F24`

F24 功能键。


## `F:Atom.KeyCode.NumLock`

小键盘锁定键。


## `F:Atom.KeyCode.ScrollLock`

滚动锁定键。


## `F:Atom.KeyCode.LeftShift`

左Shift 键。


## `F:Atom.KeyCode.RightShift`

右Shift 键。


## `F:Atom.KeyCode.LeftControl`

左Ctrl 键。


## `F:Atom.KeyCode.RightControl`

右Ctrl 键。


## `F:Atom.KeyCode.LeftAlt`

左Alt 键。


## `F:Atom.KeyCode.RightAlt`

右Alt 键。


## `F:Atom.KeyCode.Semicolon`

分号键。


## `F:Atom.KeyCode.Equals`

等号键。


## `F:Atom.KeyCode.Comma`

逗号键。


## `F:Atom.KeyCode.Minus`

减号键。


## `F:Atom.KeyCode.Period`

句号键。


## `F:Atom.KeyCode.Slash`

斜杠键。


## `F:Atom.KeyCode.Backquote`

反引号键。


## `F:Atom.KeyCode.LeftBracket`

左方括号键。


## `F:Atom.KeyCode.Backslash`

反斜杠键。


## `F:Atom.KeyCode.RightBracket`

右方括号键。


## `F:Atom.KeyCode.Quote`

引号键。


## `T:Atom.MouseButton`

引擎支持的鼠标按键。


## `F:Atom.MouseButton.Left`

鼠标左键。


## `F:Atom.MouseButton.Right`

鼠标右键。


## `F:Atom.MouseButton.Middle`

鼠标中键。


## `F:Atom.MouseButton.Back`

鼠标后退侧键。


## `F:Atom.MouseButton.Forward`

鼠标前进侧键。


## `T:Atom.GamepadButton`

与原生引擎编号一致的手柄按键。


## `F:Atom.GamepadButton.A`

手柄 A 键。


## `F:Atom.GamepadButton.B`

手柄 B 键。


## `F:Atom.GamepadButton.X`

手柄 X 键。


## `F:Atom.GamepadButton.Y`

手柄 Y 键。


## `F:Atom.GamepadButton.LeftBumper`

左肩键。


## `F:Atom.GamepadButton.RightBumper`

右肩键。


## `F:Atom.GamepadButton.LeftStick`

左摇杆按下。


## `F:Atom.GamepadButton.RightStick`

右摇杆按下。


## `F:Atom.GamepadButton.DPadUp`

方向垫上键。


## `F:Atom.GamepadButton.DPadDown`

方向垫下键。


## `F:Atom.GamepadButton.DPadLeft`

方向垫左键。


## `F:Atom.GamepadButton.DPadRight`

方向垫右键。


## `F:Atom.GamepadButton.Menu`

手柄菜单键。


## `F:Atom.GamepadButton.View`

手柄视图键。


## `T:Atom.GamepadAxis`

手柄摇杆和扳机轴。


## `F:Atom.GamepadAxis.LeftX`

左摇杆横轴，范围 -1～1。


## `F:Atom.GamepadAxis.LeftY`

左摇杆纵轴，范围 -1～1。


## `F:Atom.GamepadAxis.RightX`

右摇杆横轴，范围 -1～1。


## `F:Atom.GamepadAxis.RightY`

右摇杆纵轴，范围 -1～1。


## `F:Atom.GamepadAxis.LeftTrigger`

左扳机，范围 0～1。


## `F:Atom.GamepadAxis.RightTrigger`

右扳机，范围 0～1。


## `T:Atom.InputPreset`

组合键盘方向与左摇杆的移动预设。


## `F:Atom.InputPreset.Wasd`

W/S/A/D 移动，并合成左摇杆输入。


## `F:Atom.InputPreset.ArrowKeys`

方向键移动，并合成左摇杆输入。


## `T:Atom.Mathf`

与引擎常用数学函数保持一致；三角函数的角度参数及结果使用弧度。


## `F:Atom.Mathf.PI`

圆周率 π。


## `F:Atom.Mathf.TwoPI`

完整一周的弧度值 2π。


## `F:Atom.Mathf.HalfPI`

直角的弧度值 π/2。


## `F:Atom.Mathf.Deg2Rad`

角度转弧度的乘数 π/180。


## `F:Atom.Mathf.Rad2Deg`

弧度转角度的乘数 180/π。


## `F:Atom.Mathf.Epsilon`

数学近似阈值 1e-6；Equal 的默认误差另行定义。


## `F:Atom.Mathf.Infinity`

正无穷。


## `F:Atom.Mathf.NegInfinity`

负无穷。


## `M:Atom.Mathf.Abs(System.Single)`

返回绝对值。

- `v`：输入数值或向量。

## `M:Atom.Mathf.IsNaN(System.Single)`

检查数值是否为 NaN。

- `v`：输入数值或向量。

## `M:Atom.Mathf.IsFinite(System.Single)`

检查数值是否有限，即不是 NaN 或无穷。

- `v`：输入数值或向量。

## `M:Atom.Mathf.Sqr(System.Single)`

返回平方。

- `v`：输入数值或向量。

## `M:Atom.Mathf.Sqrt(System.Single)`

返回平方根。

- `v`：输入数值或向量。

## `M:Atom.Mathf.InvSqrt(System.Single)`

返回平方根的倒数；零返回正无穷。

- `v`：输入数值或向量。

## `M:Atom.Mathf.Equal(System.Single,System.Single,System.Single)`

判断两个浮点数的绝对差是否不超过指定误差；默认使用原生 float 机器精度。

- `a`：第一个操作数、起点或起始旋转。
- `b`：第二个操作数、终点或目标旋转。
- `tolerance`：允许的最大绝对误差。

## `M:Atom.Mathf.Clamp(System.Single,System.Single,System.Single)`

将数值限制到闭区间 [min, max]；最小值大于最大值时抛出异常。

- `v`：输入数值或向量。
- `min`：闭区间最小值。
- `max`：闭区间最大值。

## `M:Atom.Mathf.Clamp(System.Int32,System.Int32,System.Int32)`

将数值限制到闭区间 [min, max]；最小值大于最大值时抛出异常。

- `v`：输入数值或向量。
- `min`：闭区间最小值。
- `max`：闭区间最大值。

## `M:Atom.Mathf.Clamp01(System.Single)`

将数值限制到 [0, 1]。

- `v`：输入数值或向量。

## `M:Atom.Mathf.Min(System.Single,System.Single)`

返回较小值。

- `a`：第一个操作数、起点或起始旋转。
- `b`：第二个操作数、终点或目标旋转。

## `M:Atom.Mathf.Max(System.Single,System.Single)`

返回较大值。

- `a`：第一个操作数、起点或起始旋转。
- `b`：第二个操作数、终点或目标旋转。

## `M:Atom.Mathf.Min(System.Int32,System.Int32)`

返回较小值。

- `a`：第一个操作数、起点或起始旋转。
- `b`：第二个操作数、终点或目标旋转。

## `M:Atom.Mathf.Max(System.Int32,System.Int32)`

返回较大值。

- `a`：第一个操作数、起点或起始旋转。
- `b`：第二个操作数、终点或目标旋转。

## `M:Atom.Mathf.Floor(System.Single)`

向负无穷取整；向量重载逐分量计算。

- `v`：输入数值或向量。

## `M:Atom.Mathf.Ceil(System.Single)`

向正无穷取整。

- `v`：输入数值或向量。

## `M:Atom.Mathf.Floor(System.Numerics.Vector3)`

向负无穷取整；向量重载逐分量计算。

- `v`：输入数值或向量。

## `M:Atom.Mathf.DegreesToRadians(System.Single)`

将角度转换为弧度。

- `v`：输入数值或向量。

## `M:Atom.Mathf.RadiansToDegrees(System.Single)`

将弧度转换为角度。

- `v`：输入数值或向量。

## `M:Atom.Mathf.Sin(System.Single)`

返回弧度角的正弦。

- `v`：输入数值或向量。

## `M:Atom.Mathf.Cos(System.Single)`

返回弧度角的余弦。

- `v`：输入数值或向量。

## `M:Atom.Mathf.Tan(System.Single)`

返回弧度角的正切。

- `v`：输入数值或向量。

## `M:Atom.Mathf.Acos(System.Single)`

返回反余弦，单位弧度；输入越界时按引擎规则限制到端点。

- `v`：输入数值或向量。

## `M:Atom.Mathf.Asin(System.Single)`

返回反正弦，单位弧度；输入越界时按引擎规则限制到端点。

- `v`：输入数值或向量。

## `M:Atom.Mathf.Atan(System.Single)`

返回反正切，单位弧度。

- `v`：输入数值或向量。

## `M:Atom.Mathf.Atan2(System.Single,System.Single)`

根据 y 和 x 返回包含象限信息的反正切，单位弧度。

- `y`：纵坐标。
- `x`：横坐标。

## `M:Atom.Mathf.Pow(System.Single,System.Single)`

返回 a 的 b 次方。

- `a`：底数。
- `b`：指数。

## `M:Atom.Mathf.Lerp(System.Single,System.Single,System.Single)`

进行线性插值；t 不限制到 [0, 1]，支持区间外推。

- `a`：第一个操作数、起点或起始旋转。
- `b`：第二个操作数、终点或目标旋转。
- `t`：插值系数，不自动限制范围。

## `M:Atom.Mathf.Lerp(System.Numerics.Vector2,System.Numerics.Vector2,System.Single)`

进行线性插值；t 不限制到 [0, 1]，支持区间外推。

- `a`：第一个操作数、起点或起始旋转。
- `b`：第二个操作数、终点或目标旋转。
- `t`：插值系数，不自动限制范围。

## `M:Atom.Mathf.Lerp(System.Numerics.Vector3,System.Numerics.Vector3,System.Single)`

进行线性插值；t 不限制到 [0, 1]，支持区间外推。

- `a`：第一个操作数、起点或起始旋转。
- `b`：第二个操作数、终点或目标旋转。
- `t`：插值系数，不自动限制范围。

## `M:Atom.Mathf.Lerp(System.Numerics.Vector4,System.Numerics.Vector4,System.Single)`

进行线性插值；t 不限制到 [0, 1]，支持区间外推。

- `a`：第一个操作数、起点或起始旋转。
- `b`：第二个操作数、终点或目标旋转。
- `t`：插值系数，不自动限制范围。

## `M:Atom.Mathf.SmoothStep(System.Single,System.Single,System.Single)`

将指定区间内的值平滑映射到 [0, 1]；两个边界应不同。

- `edge0`：映射区间起点。
- `edge1`：映射区间终点，须与起点不同。
- `value`：待映射的数值。

## `M:Atom.Mathf.Normalize(System.Numerics.Vector3)`

返回归一化向量；精确零向量按原生规则返回 Vector3.One。

- `v`：输入数值或向量。

## `M:Atom.Mathf.NormalizeOrZero(System.Numerics.Vector3)`

返回归一化向量；零向量返回 Vector3.Zero。

- `v`：输入数值或向量。

## `M:Atom.Mathf.Length(System.Numerics.Vector3)`

返回向量长度，任一分量无穷时返回正无穷。

- `v`：输入数值或向量。

## `M:Atom.Mathf.Dot(System.Numerics.Vector3,System.Numerics.Vector3)`

返回向量点积。

- `a`：第一个操作数、起点或起始旋转。
- `b`：第二个操作数、终点或目标旋转。

## `M:Atom.Mathf.Cross(System.Numerics.Vector3,System.Numerics.Vector3)`

返回向量叉积。

- `a`：第一个操作数、起点或起始旋转。
- `b`：第二个操作数、终点或目标旋转。

## `M:Atom.Mathf.Distance(System.Numerics.Vector3,System.Numerics.Vector3)`

返回两点间距离。

- `a`：第一个操作数、起点或起始旋转。
- `b`：第二个操作数、终点或目标旋转。

## `M:Atom.Mathf.DistanceSquared(System.Numerics.Vector3,System.Numerics.Vector3)`

返回两点间距离的平方。

- `a`：第一个操作数、起点或起始旋转。
- `b`：第二个操作数、终点或目标旋转。

## `M:Atom.Mathf.Rotate(System.Numerics.Quaternion,System.Numerics.Vector3)`

使用四元数旋转向量。

- `rotation`：旋转四元数。
- `v`：输入数值或向量。

## `M:Atom.Mathf.AxisAngle(System.Numerics.Vector3,System.Single)`

从旋转轴和弧度角构造四元数；轴自动归一化，零轴返回单位旋转。

- `axis`：旋转轴，内部归一化；零轴表示单位旋转。
- `radians`：旋转角，单位弧度。

## `M:Atom.Mathf.EulerRadians(System.Numerics.Vector3)`

从弧度欧拉角构造归一化四元数，顺序与引擎一致，为 qz * qx * qy。

- `angles`：X/Y/Z 欧拉角，单位弧度。

## `M:Atom.Mathf.EulerDegrees(System.Numerics.Vector3)`

从角度欧拉角构造旋转四元数，顺序为 qz * qx * qy。

- `angles`：X/Y/Z 欧拉角，单位度。

## `M:Atom.Mathf.Slerp(System.Numerics.Quaternion,System.Numerics.Quaternion,System.Single,System.Boolean)`

球面插值四元数，与原生函数一致；默认选择最短旋转路径，t 不作区间限制。

- `a`：第一个操作数、起点或起始旋转。
- `b`：第二个操作数、终点或目标旋转。
- `t`：插值系数，不自动限制范围。
- `shortestPath`：是否选择最短旋转路径，默认是。

## `T:Atom.ComponentKind`

可独立启用或停用的引擎组件类别。


## `F:Atom.ComponentKind.Mesh`

网格渲染组件。


## `F:Atom.ComponentKind.Collider`

碰撞体组件。


## `F:Atom.ComponentKind.RigidBody`

刚体组件。


## `F:Atom.ComponentKind.Script`

脚本组件。


## `F:Atom.ComponentKind.UI`

用户界面组件。


## `F:Atom.ComponentKind.Audio`

音频组件。


## `F:Atom.ComponentKind.Camera`

相机组件。


## `F:Atom.ComponentKind.Light`

灯光组件。


## `F:Atom.ComponentKind.Joint`

物理关节组件。


## `F:Atom.ComponentKind.Particle`

原生GPU粒子发射器组件。


## `T:Atom.ParticleComponent`

可序列化的粒子组件引用；Inspector可拖入场景组件、粒子预制体或.particle资产；资产会创建场景组件并绑定引用，在当前世界解析。


## `P:Atom.ParticleComponent.PersistentId`

目标对象的持久化ID，空字符串表示未分配。


## `P:Atom.ParticleComponent.None`

未分配的粒子组件引用。


## `M:Atom.ParticleComponent.#ctor(System.String)`

建立持久ID引用；脚本字段由宿主自动绑定世界。

- `persistentId`：场景对象或已实例化预制体对象的持久ID。

## `P:Atom.ParticleComponent.IsValid`

引用目标仍存在且拥有原生粒子组件时为真。


## `M:Atom.ParticleComponent.Play(System.Numerics.Vector3)`

在指定世界位置播放预设粒子组件，过期引用报告异常。

- `position`：世界坐标发射位置。

## `M:Atom.ParticleComponent.Stop`

停止发射；存活粒子继续按生命周期消退。


## `T:Atom.RaycastHit`

射线命中结果，包含命中对象、世界坐标命中点、法线和距离。

- `Object`：命中对象。
- `Point`：世界坐标命中点。
- `Normal`：世界坐标表面法线。
- `Distance`：从射线起点到命中点的距离。

## `M:Atom.RaycastHit.#ctor(Atom.GameObject,System.Numerics.Vector3,System.Numerics.Vector3,System.Single)`

射线命中结果，包含命中对象、世界坐标命中点、法线和距离。

- `Object`：命中对象。
- `Point`：世界坐标命中点。
- `Normal`：世界坐标表面法线。
- `Distance`：从射线起点到命中点的距离。

## `P:Atom.RaycastHit.Object`

命中对象。


## `P:Atom.RaycastHit.Point`

世界坐标命中点。


## `P:Atom.RaycastHit.Normal`

世界坐标表面法线。


## `P:Atom.RaycastHit.Distance`

从射线起点到命中点的距离。


## `T:Atom.PointerState`

当前游戏视口的鼠标位置，编辑器与独立游戏均以渲染像素计。


## `M:Atom.PointerState.#ctor(System.Boolean,System.Numerics.Vector2,System.Numerics.Vector2)`

当前游戏视口的鼠标位置，编辑器与独立游戏均以渲染像素计。


## `T:Atom.SaveStore`

按游戏命名空间隔离的通用JSON存档服务，支持任意可序列化类型与独立槽位。


## `M:Atom.SaveStore.#ctor(System.String)`

创建游戏专属存档服务，数据保存在用户LocalApplicationData。

- `gameId`：稳定的游戏标识，只允许字母、数字、下划线和连字符。

## `M:Atom.SaveStore.Exists(System.String)`

检查槽位是否存在。

- `slot`：槽位标识。

## `M:Atom.SaveStore.Load``1(System.String)`

读取指定类型；不存在返回默认值，损坏或类型不匹配抛出异常并保留原文件。

- `slot`：槽位标识。
- `T`：游戏定义的可序列化存档类型。

## `M:Atom.SaveStore.Save``1(System.String,``0)`

先序列化，再以同目录临时文件原子替换槽位；失败保留旧文件。

- `slot`：槽位标识。
- `data`：需要保存的数据，版本与迁移策略由游戏定义。
- `T`：游戏定义的可序列化存档类型。

## `M:Atom.SaveStore.Delete(System.String)`

删除单个槽位，不存在时无需操作。

- `slot`：槽位标识。

## `T:Atom.SceneManager`

访问当前项目的场景清单，并在下一帧安全切换游戏场景。


## `P:Atom.SceneManager.CurrentScene`

当前游戏场景相对于项目目录的路径。


## `P:Atom.SceneManager.LastLoadError`

最近一次异步场景加载的错误；新请求或成功加载后清空。


## `P:Atom.SceneManager.IsLoading`

是否存在尚待下一帧处理的切换请求；加载失败会保留旧场景及脚本状态。


## `P:Atom.SceneManager.Scenes`

项目构建清单内可加载的场景路径。


## `M:Atom.SceneManager.LoadScene(System.String)`

请求切换场景；当前回调完成后在下一帧执行，旧场景对象及脚本实例不再有效。

- `nameOrPath`：场景名称或项目清单中的完整相对路径；名称重复时必须传入路径。

## `T:Atom.ScriptInput`

具名键盘、鼠标和手柄输入接口；所有查询遵守引擎游戏输入焦点限制。


## `M:Atom.ScriptInput.IsDown(Atom.KeyCode)`

查询指定按键是否持续按下。

- `key`：要查询的键盘按键；None 返回假。

## `M:Atom.ScriptInput.WasPressed(Atom.KeyCode)`

查询指定按键是否在当前帧首次按下。

- `key`：要查询的键盘按键；None 返回假。

## `M:Atom.ScriptInput.WasReleased(Atom.KeyCode)`

查询指定按键是否在当前帧松开。

- `key`：要查询的键盘按键；None 返回假。

## `M:Atom.ScriptInput.IsDown(Atom.MouseButton)`

查询指定按键是否持续按下。

- `button`：要查询的鼠标或手柄按键。

## `M:Atom.ScriptInput.WasPressed(Atom.MouseButton)`

查询指定按键是否在当前帧首次按下。

- `button`：要查询的鼠标或手柄按键。

## `M:Atom.ScriptInput.WasReleased(Atom.MouseButton)`

查询指定按键是否在当前帧松开。

- `button`：要查询的鼠标或手柄按键。

## `M:Atom.ScriptInput.IsDown(Atom.GamepadButton)`

查询指定按键是否持续按下。

- `button`：要查询的鼠标或手柄按键。

## `M:Atom.ScriptInput.WasPressed(Atom.GamepadButton)`

查询指定按键是否在当前帧首次按下。

- `button`：要查询的鼠标或手柄按键。

## `M:Atom.ScriptInput.WasReleased(Atom.GamepadButton)`

查询指定按键是否在当前帧松开。

- `button`：要查询的鼠标或手柄按键。

## `M:Atom.ScriptInput.Axis(Atom.GamepadAxis)`

读取具名手柄轴，摇杆范围 -1～1，扳机范围 0～1；无游戏输入焦点时为零。

- `axis`：要读取的手柄轴。

## `M:Atom.ScriptInput.Movement(Atom.InputPreset)`

组合键盘方向与手柄摇杆得到移动向量；前进为正 Y，合成向量长度限制为 1。

- `preset`：键盘移动预设，WASD 或方向键，均合成左摇杆输入。

## `M:Atom.ScriptInput.Movement(Atom.KeyCode,Atom.KeyCode,Atom.KeyCode,Atom.KeyCode,Atom.GamepadAxis,Atom.GamepadAxis)`

组合键盘方向与手柄摇杆得到移动向量；前进为正 Y，合成向量长度限制为 1。

- `forward`：前进键，对应正 Y。
- `backward`：后退键，对应负 Y。
- `left`：左移键，对应负 X。
- `right`：右移键，对应正 X。
- `horizontal`：横向手柄轴，默认左摇杆 X。
- `vertical`：纵向手柄轴，默认左摇杆 Y。

## `T:Atom.SpawnRequest`

延迟创建或附加预制体的请求；在回调安全点后轮询，新增脚本下一帧开始执行。


## `P:Atom.SpawnRequest.IsCompleted`

请求已经成功或失败时为真；读取此属性会轮询引擎结果。


## `P:Atom.SpawnRequest.Failed`

请求执行失败或所属世界结束时为真；仍在等待时为假。


## `P:Atom.SpawnRequest.Result`

成功后的对象引用；等待时为 null，失败时抛出异常；同一请求缓存结果。


## `T:Atom.Time`

当前游戏世界的只读时间。暂停时停止计数，重新 Play 时归零，热重载保留；单位为秒。


## `P:Atom.Time.DeltaTime`

当前回调的时间间隔（秒）；固定更新及碰撞回调使用固定步长，其他回调使用当前游戏帧间隔，暂停为零。


## `P:Atom.Time.FrameDeltaTime`

当前游戏帧的间隔（秒），与 OnUpdate 的 dt 一致；暂停为零。


## `P:Atom.Time.FixedDeltaTime`

物理固定更新步长（秒），默认 1/60；即使暂停也保留配置值。


## `P:Atom.Time.ElapsedTime`

自本次 Play 开始累计的游戏帧时间（秒），不包含暂停时间；使用双精度避免长时间运行精度下降。


## `P:Atom.Time.FixedTime`

已经执行的物理固定步累计时间（秒），一帧可能包含零次或多次固定更新。


## `P:Atom.Time.FrameCount`

本次 Play 已执行的游戏帧数；第一帧为 1，多个脚本共享同一帧号。


## `P:Atom.Time.FixedFrameCount`

本次 Play 已执行的固定更新次数；每个物理步只增加一次，不受脚本数量影响。


## `P:Atom.Time.FramesPerSecond`

当前游戏帧的瞬时帧率（1/FrameDeltaTime），暂停或间隔为零时返回零；不代表平滑后的显示帧率。


## `P:Atom.Time.IsFixedUpdate`

当前是否位于 OnFixedUpdate 或物理碰撞回调中。


## `P:Atom.Time.IsPaused`

游戏是否暂停；暂停期间不执行更新回调。


## `T:Atom.UiElement`

UI 元素访问接口；所有属性写入均由引擎校验，对象需拥有 UI 组件。


## `P:Atom.UiElement.Position`

UI 布局偏移量；响应式模式下相对于锚点计算位置。


## `P:Atom.UiElement.Size`

UI 尺寸，各分量须大于零且不超过 4096。


## `P:Atom.UiElement.AnchorMin`

锚点区域的最小坐标，范围 0～1，不能大于 AnchorMax。


## `P:Atom.UiElement.AnchorMax`

锚点区域的最大坐标，范围 0～1，不能小于 AnchorMin。


## `P:Atom.UiElement.Pivot`

UI 的归一化轴心坐标，范围 0～1。


## `P:Atom.UiElement.Progress`

进度显示值，范围 0～1。


## `P:Atom.UiElement.Visible`

UI 元素是否可见。


## `P:Atom.UiElement.Interactable`

UI 元素是否接受点击和键盘、手柄导航。


## `P:Atom.UiElement.Responsive`

是否使用锚点、轴心和视口尺寸计算响应式布局。

