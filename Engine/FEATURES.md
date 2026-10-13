# 引擎功能与使用说明

## 构建与运行

环境：Windows x64、Visual Studio 2026 C++ 工具链、Windows SDK、CMake、.NET 10 x64 SDK，以及已恢复的 Microsoft.GameInput 3.5.283 NuGet 包。项目使用 UTF-8 源码和字符串字面量。

构建 `Engine/AtomEngine.sln` 的 Debug 或 Release x64 配置。MSBuild 自动构建 Jolt 与两个 C# 项目，并将脚本 DLL、运行配置及 nethost 复制到输出目录。首次构建 Jolt 时间较长。运行需要 .NET 10 x64 Runtime。

Debug 运行工作目录为 `Engine`；Release 输出附带 `Asset` 目录，可直接从输出目录运行。运行 `AtomEngine.exe --smoke-test` 执行集成检查并渲染 120 帧。`--runtime-smoke-test` 通过普通 GameApp 运行同样检查，验证无 ImGui 编辑器上下文的运行路径。

## 输入

`Input` 提供键盘、鼠标和手柄按下、首次按下、松开状态、鼠标位置/滚轮、双摇杆和扳机。GameInput 与 XInput 的实现分别位于 `Function/Input/GameInputBackend.cpp` 和 `XInputBackend.cpp`，通过 `InputBackend` 接口接入。

每帧按设备类型选择输入来源：GameInput 有有效且仍连接的设备读数时优先使用；手柄不受支持或断开时使用 XInput。XInput 不提供键鼠协议，回退适配器通过 Win32 读取键鼠。窗口失去焦点时清空按键与摇杆状态。编辑器 Scene 面板显示当前手柄后端。当前公共接口提供一个活动手柄。

## C# 行为脚本

`Managed/Atom.Managed` 是原生引擎与 C# 之间的宿主桥；`Managed/GameScripts` 编译为 `AtomScripts.dll`。类继承 `Atom.Behaviour`，支持 `OnCreate`、`OnUpdate`、`OnFixedUpdate`、`OnCollision`、`OnUiClick` 和 `OnDestroy`。`Object` 提供局部 Position、Rotation、Scale、LinearVelocity、UI Text，以及键鼠和手柄查询。读写已销毁对象、缺少刚体或非有限变换会被拒绝。

在 Inspector 添加 C# Script，Assembly 默认 `Scripts/AtomScripts.dll`，Class 使用完整类型名，例如 `Game.Rotate` 或 `Game.KeyboardMover`。公开字段可在 Fields 中以字符串配置；桥接支持基础数值、布尔、字符串和枚举。Assembly 路径相对于可执行文件解析。脚本异常显示在 Scene 面板，发生异常的实例停止后续回调。

`Game.Rotate.Speed` 控制旋转速度；`Game.KeyboardMover.Speed` 控制动态刚体在 X/Z 方向的速度，使用 WASD 或左摇杆。脚本的固定更新与物理按 1/60 秒交替推进。更改 C# 代码后重新构建并重启引擎；当前没有 C# 程序集热重载。

## 物理与碰撞

Jolt Physics 5.5.0 负责刚体模拟。Collider 支持 Box、Sphere、Capsule、局部偏移、旋转、尺寸和 Trigger。只有 Collider 的物体作为静态碰撞体；添加 Rigid Body 后可选择 Static、Kinematic 或 Dynamic，并配置质量、摩擦、弹性、重力和初始线速度。

运行模式同步脚本变换与物理姿态，并将动态刚体结果写回 GameObject。碰撞开始会触发脚本 `OnCollision(otherEntity, trigger)`。碰撞可视化使用调试绘制；选中物体描边单独使用着色器实现。球体采用最大轴缩放，胶囊半径采用 X/Z 最大缩放，与 Jolt 的实际形状保持一致。当前碰撞形状是简单凸体，不包含网格碰撞体和关节约束。

## 场景、预制体与编辑器

默认加载 `Asset/Scenes/MaterialTest.scene`：Grid、平面、49 个不同金属度/粗糙度的球、带脚本的动态立方体及中文文本和 C# 点击计数按钮。`Asset/Prefabs/PhysicsCube.prefab` 提供可实例化示例。

Hierarchy 可添加、删除与选择对象；Inspector 可修改名称、层级、三维变换、模型/内置图元、PBR 材质、碰撞体、刚体、脚本与 UI 元素。背景左键点击通过射线与三角形相交选择最近对象，支持蒙皮后的顶点；选中网格写入 mask，再由全屏着色器生成描边。Alt + 左键旋转相机，中键平移，右键缩放，Shift 切换飞行操作。

Scene 面板保存、加载 `.scene`。文件内容为版本化 XML，包含稳定对象 ID、父子关系、变换、模型来源、材质参数/贴图/UV、脚本与字段、碰撞体、刚体、UI 和预制体来源。加载在临时 World 中完成，验证失败不会覆盖原场景。Play 前保存编辑快照，Stop 恢复快照，避免模拟结果改写编辑状态。

Save selected prefab 将对象及其后代保存为 XML `.prefab`；Instantiate prefab 为每个实例重建 ID 和内部父子关系。当前预制体保存完整数据快照，没有资产修改传播、覆盖差异管理或编辑器撤销/重做。相机和环境参数尚未写入场景文件。

## 渲染资源与材质

`Primitive` 统一生成 Plane、Cube、Sphere、Quad 的基础网格，通过标准 Model/Mesh 渲染路径提交；旧 PrimitiveMesh 与独立 MeshRenderer 路径已移除。`MeshGeometry` 保存 CPU 顶点、索引及蒙皮权重；`Model` 组合网格、材质、骨骼和动画资源；`AnimationPlayer` 保存每实例播放状态；`ModelGpuBuilder` 和 `ModelRenderResources` 管理 DX12 构建与提交。TextureRef 管理共享纹理生命周期；Material 与 MaterialInstance 分别保存资源默认值和物体实例参数。

当前实现使用 PBR 金属度—粗糙度模型，包含 BaseColor、Metallic、Roughness、Normal、AO、Emissive、Specular 七个贴图槽，支持导入的 packed metallic/roughness 与独立实例贴图覆盖。Specular 调节介电 F0，Roughness 保持最小值避免数值不稳定。ShadingModel 为后续 BRDF 扩展保留标识，Glass/Cloth 尚无对应渲染实现。

## UI 与着色器

运行时 UI 使用 UiElementComponent 配置屏幕位置、尺寸、颜色、图像和 UTF-8 文本；DirectWrite 布局/字形覆盖率生成透明文字纹理，通过独立 OverlayPass 合成。支持中文字体回退。支持按 layer 排序的点击命中、按钮背景与 C# OnUiClick 回调，脚本可通过 Object.Text 更新文本。当前不包含布局容器或键盘/手柄 UI 导航。

`Shader` 封装 DXC 编译结果与反射信息，包括资源 register/space、常量缓冲区字段大小和偏移。ShaderLibrary 每 0.5 秒检查 HLSL/HLSLI 修改，成功后刷新关联 Graphics/Compute PSO；失败保留最后可用的字节码或 PSO，并在 Shaders 面板显示诊断。

低优先级效果预览保留 `ShaderPreviewRequest` 和 `Shader::preview` 回调；未注册预览渲染器时按钮禁用，当前没有独立 preview 视窗。

## 验证与依赖来源

框架回归：`Tests/run-framework-tests.cmd`。集成检查覆盖场景 XML 往返、父子关系、预制体 ID、加载失败回滚、C# 更新/固定更新、刚体落地/碰撞、PBR 反射布局、着色器失败回滚/修复重载、视口拾取、UI 层级命中/C# 点击回调和 UI 纹理实际像素。手柄设备兼容性与物理按键操作仍需真实设备验证。

Jolt、TinyXML2 与 .NET 原生宿主头文件保留各自许可证。参考：[Jolt Physics](https://github.com/jrouwe/JoltPhysics)、[TinyXML2](https://github.com/leethomason/tinyxml2)、[.NET 原生宿主](https://learn.microsoft.com/en-us/dotnet/core/tutorials/netcore-hosting)、[DirectWrite 字形 alpha 纹理](https://learn.microsoft.com/en-us/windows/win32/api/dwrite/nf-dwrite-idwriteglyphrunanalysis-createalphatexture)。

## 文件启动场景与预加载

编辑器启动读取 `Asset/Editor.xml` 的 `startupScene` 属性，只实例化指定 `.scene` 文件中的物体。Camera 节点保存编辑器初始视角；已删除 RenderTestScene 的硬编码物体生成。文件缺失或加载失败会报告错误，不再生成替代测试场景。

SceneSerializer::CollectResources 可扫描 `.scene` / `.prefab` 中的模型、内置图元、材质覆盖贴图和 UI 图片。加载场景及实例化预制体前自动去重并预加载，复用 AssetManager 缓存；模型自身的材质纹理由模型加载器加载。贴图的 sRGB 与 fallback 参数保持与渲染路径一致。失败报告包含资源路径，加载失败保留当前 World。

提前预加载其他场景可使用：

```cpp
auto batch = std::make_shared<AssetPreloadBatch>(
    SceneSerializer::CollectResources("Asset/Scenes/Next.scene"));
// 每帧在引擎线程推进；单个资源内部加载仍为同步操作。
batch->Step(2);
if (batch->Report().Success()) {
    sceneManager.LoadScene("Asset/Scenes/Next.scene");
    // 保持 batch 存活到场景激活，避免弱缓存图元提前释放。
}
```

`Report` 提供 total、completed、succeeded、milliseconds、errors。`AssetManager::Preload` 是一次完成的便捷接口。当前不含后台线程 GPU 上传、脚本程序集预热或磁盘着色器缓存；预加载将工作提前，不保证缩短程序首次启动的总工作量。

## 日志

日志在 Debug 和 Release 均可用，默认写入可执行文件目录的 `logs/AtomEngine.log`。UTF-8 文件包含毫秒时间、等级、分类、线程 ID；Error/Critical 附调用文件与行号。默认按 8 MiB 轮转，保留当前文件加四个备份。内存保留最近 2000 条，线程安全，支持长消息与宽字符格式化。Warning 及以上立即刷新；关闭或 FlushLog 刷新其他消息。

`StartLog(LogOptions)` 可配置目录、最低等级、轮转大小/数量和历史容量；`WriteLog(LogLevel::Error, "Assets", message)` 为分类接口，原 Log 接口兼容。ECS 的逐物体/组件消息改为 Debug，默认不刷屏。编辑器 Console 支持等级过滤、文本过滤、清除历史和刷新文件；D3D12 Debug Layer 回调接入日志，仍保留原验证及断点设置。

Startup 分类记录平台、D3D12、Renderer/着色器、场景初始化、首帧呈现耗时。ShaderLibrary、场景加载和预加载失败会记录诊断。日志独立测试：`Tests/run-log-tests.cmd`；集成 smoke 检查新增预加载进度、缓存复用、贴图依赖去重、失败回滚和长 UTF-8/并发日志。

## 编辑器工作流

默认工作区采用停靠布局，包含 Hierarchy、Viewport、Inspector、Assets、Console 和 Scene；View 菜单可切换资源、日志、着色器、网格和碰撞体显示，支持重置布局。窗口布局保存在可执行文件旁的 AtomEditor.ini，面板显示与最近十个场景保存在 EditorPreferences.xml。加载 Windows 微软雅黑字体以显示中文物体名、资源名和日志。

File 提供新建、打开、最近场景、保存和另存为，使用原生文件对话框。星号表示未保存；打开、新建或关闭窗口时提供保存、丢弃、取消。编辑使用内存 XML 快照撤销/重做，覆盖场景物体、组件、材质、脚本字段和层级；最多保留 100 个状态或 16 MiB（单个超大场景至少保留一个状态）。连续拖动完成后合并成一次编辑，保存点通过状态 ID 跟踪。大型场景的全量快照仍有开销，后续可改为组件级命令。

Hierarchy 支持递归父子树、搜索、右键复制/删除，以及拖拽改变父级；拖到 Scene root 可解除父级。复制整个子树会生成新 ID、重映射内部父子关系并复制组件属性。调整父级保持世界变换，拒绝循环父子关系，以及 TRS 无法表示的剪切/镜像变换。Inspector 支持角度制旋转、重置变换、移除组件和删除脚本字段；导入模型先验证成功再应用。

Viewport 保持渲染画面的宽高比，支持 ImGuizmo 移动、旋转、缩放、局部/世界坐标和步进吸附。手柄操作时抑制拾取与相机输入。F 将相机对准选中物体，Escape 取消选择。输入文字、操作控件或显示模态对话框时抑制场景快捷键。

Assets 可浏览 Asset 和输出目录的 Scripts，支持搜索、刷新、复制路径、外部打开和纹理预览。双击场景打开，双击预制体实例化；模型/预制体可拖到视口，纹理和脚本程序集可拖到兼容 Inspector 字段。这里只浏览和选择已有资源，暂不提供资源导入转换、重命名或删除。

Play 保存内存中的编辑快照；Stop 恢复编辑状态、选择和原有撤销历史。支持暂停、继续和单帧运行；单帧使用 1/60 秒推进脚本、物理和动画，暂停期间冻结运行时更新。运行中禁用场景属性修改。

| 快捷键 | 操作 |
| --- | --- |
| Ctrl+N / Ctrl+O | 新建 / 打开场景 |
| Ctrl+S / Ctrl+Shift+S | 保存 / 另存为 |
| Ctrl+Z / Ctrl+Y（或 Ctrl+Shift+Z） | 撤销 / 重做 |
| Ctrl+D / Delete | 复制 / 删除选中子树 |
| F / Escape | 聚焦选中物体 / 取消选择 |
| T / R / S | 视口移动 / 旋转 / 缩放 |

Help 菜单包含操作提示。EditorSmokeChecks 覆盖撤销保存点与分支、层级保留世界变换、循环拒绝、子树复制与删除、变换转换、场景快照往返和暂停；--smoke-test 同时输出 Smoke/editor.raw 供完整界面检查。


### 图元、天空盒与相机修复

Cube 的六个面采用朝外的三角形绕序，保留每面独立的法线、UV 和切线。天空盒使用不剔除的全屏三角形和 Reverse-Z 深度只读测试，优先绘制 Environment cubemap；未配置时回退到 specular IBL，LOD 偏移初始化为零。天空射线从近裁剪面反投影，避免无限远投影的 w=0。

编辑器相机直接使用当前 ImGui 视口的鼠标位置、按键和滚轮增量，适用于分离的 Viewport 窗口。Alt+左键绕目标旋转，中键平移，右键拖动或滚轮缩放；按 Shift 进入自由移动，Shift+右键旋转，WASD 移动、Q/E 升降。相机手势期间关闭变换手柄交互，避免手柄抢占鼠标。轨道和自由旋转切换保持当前视角，四元数插值使用最短路径。GameInput 的左右 Shift/Ctrl/Alt 同时映射为通用修饰键。

RenderCameraSmokeChecks 验证 Cube 全部三角形绕序和切线、修饰键、相机空闲/轨道/自由旋转/滚轮，以及实际天空盒像素与深度遮挡；包含在编辑器和独立运行时 smoke 中。


### 阴影与材质预设

平行光阴影采用 Reverse-Z 正交投影，按可见物体的世界包围盒自动拟合光空间范围，中心按阴影纹素对齐。阴影 PSO 与正常几何的绕序一致；使用适合小场景的负深度偏移、3×3 PCF，并将投影范围之外的区域视为未遮挡。Rendering diagnostics 中可关闭阴影或自动拟合。当前仍为单张平行光阴影贴图。

Asset/Materials 提供 Gold、BrushedSteel、RedPlastic、BlueCeramic 和 Rubber 五种 PBR 预设。独立 .mat 文件采用 XML，保存基础颜色、金属度、粗糙度、F0、法线强度、AO、自发光、UV 和七个贴图槽。保存选中物体材质时会展开导入模型的贴图，并保留 glTF 打包贴图的 B/G 通道。预设替换整个槽的材质，未配置贴图使用默认值。应用前验证并预加载贴图，失败时保留原材质。

View → Materials 打开材质面板；Assets 中点击 Materials 浏览预设，单击 .mat 加载预览。Preview selected object 可以实时预览选中槽，左键拖动材质球旋转视角。预览在独立 256×256 颜色/深度目标上使用与场景共用的 PBR 着色代码与 IBL，不改变主视口目标。可以保存选中槽为新 .mat，也可以通过按钮应用到选中槽或全部槽。

从 Assets 将 .mat 拖到 Viewport 中的物体或 Hierarchy 的物体行，替换全部材质槽；拖到 Inspector 的 Material N 标题，仅替换该槽。应用支持撤销/重做，材质数据随 .scene 和 prefab 保存。已有布局可用 View 的 Reset layout 将 Materials 停靠到 Inspector 下方。

ShadowMaterialSmokeChecks 覆盖预设 XML 往返、单槽/全槽应用、失败回滚、撤销/重做、实际材质球像素、Reverse-Z 投影及开启/关闭阴影的接收面像素差异；编辑器和独立运行时 smoke 均执行这些检查。


### 场景相机与编辑器细节

Hierarchy 默认显示 Main Camera 与 Sun，可使用 Create 添加相机或定位太阳光。相机的 Transform、FOV、近/远裁剪面、启用状态和主相机标记保存在 .scene/prefab 中；Inspector 支持对齐编辑视角、从相机查看和切换主相机。编辑视角与游戏相机分开，Play 使用场景主相机，Stop 恢复编辑视角。太阳光的旋转决定光照方向，支持颜色、强度、启用、投影与自动拟合阴影；参数同样支持保存和撤销。复制相机默认不设为主相机，复制太阳光默认禁用。旧场景加载时补充默认相机和太阳光。

Inspector 中材质标题改为 Material。默认停靠布局将 Materials 放在 Inspector 下方，将 Scene/Console/Shaders 信息窗口放在 Assets 右侧；旧布局首次迁移，也可通过 View → Reset layout 重置。标签栏默认隐藏窗口菜单图标，仍支持拖动停靠。

Assets 默认采用图标网格，文件夹使用文件夹图标，二维图片显示保持比例的缩略图；可切回列表。每帧最多加载两张新缩略图、每个目录最多缓存 128 项，Refresh/换目录释放缓存。多层纹理和 cubemap 显示类型图标。保留搜索、双击打开与拖拽应用操作。

变换快捷键为移动 T、旋转 R、缩放 S，删除独立选择工具模式；鼠标点击选中和 Escape 取消选中仍可用。Play/Stop、Pause/Resume 与 Step 移至 Viewport 导航栏，并用颜色显示运行/暂停状态。Debug 提供 Maximize on Play，在运行时铺满应用窗口，保留 Stop、Pause、Step 控制，关闭此选项可返回停靠布局。暂停冻结脚本、物理和动画，单步按 1/60 秒推进。

SceneViewSmokeChecks 覆盖相机位姿和投影应用、太阳光 XML、撤销/重做、非法相机加载回滚。编辑器 smoke 额外验证 Play 铺满窗口、暂停与 Stop 还原状态，并输出 editor-folders、editor、editor-play、editor-pause、editor-restored 五组界面捕获。


编辑视口默认显示场景相机的视锥线条：近/远裁剪面与四条侧边跟随 FOV、裁剪距离、视口宽高比和世界位姿更新；不受相机自身缩放影响，与运行时取景保持一致。相机位置附带小型线框、向上标记和朝向线，避免远裁剪面很远时难以定位相机。普通相机为青色，选中为黄色，禁用相机为灰色。View → Camera frustums 可以切换显示，偏好会保存；Play 时隐藏这些编辑辅助线。SceneViewSmokeChecks 同时验证视锥八个角点与实际投影矩阵的对应关系和 GPU 线条像素。


### ImGui / overlay 颜色空间

UI 颜色与着色器输出按线性 RGB 处理。ImGui 主窗口改为在独立 R8G8B8A8_UNORM_SRGB 目标中混合，硬件在混合时解码目标值、写入时执行 sRGB 编码；最终呈现到 R10G10B10A2_UNORM 主交换链。字体覆盖率和 Alpha 不参与 gamma 转换。分离的 ImGui 窗口使用 UNORM 交换链与显式 SRGB RTV，保持相同颜色与混合规则。平台格式转换依据 Microsoft 的 [颜色空间说明](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/converting-data-color-space)。

资源图片使用 sRGB SRV 解码后参与线性混合；视口和材质球预览已经是显示编码数据，必须使用 ImGuiCommon::ImageDisplayEncoded（或 AddImageDisplayEncoded）标记，在绘制时解码，防止重复 gamma。普通线性纹理使用常规 ImGui::Image。ImGui 目标为 8-bit sRGB，主交换链保持原来的 10-bit 格式；嵌入图片会经过该 UI 目标量化。

运行时 gOverlayBuffer 保存预乘线性颜色，CompositePS / ScaleAndCompositePS 在线性空间计算 overlay.rgb + scene.rgb * (1 - overlay.a)，最后执行一次 sRGB 转换。移除了之前计算合成后又被 Present/Sharpen/BlendUI 覆盖的重复路径，缩放时同样使用线性滤波。

--ui-color-smoke-test 运行独立颜色回归，检查透明/半透明/不透明 overlay、原尺寸/缩放呈现，以及 ImGui 灰阶、半透明混合、线性图片、已编码预览图与回调状态恢复，并创建分离窗口验证 SRGB RTV。GPU 原始像素位于输出目录 Smoke，运行日志位于 generated/ui-color-*。

脚本编辑期间可使用 MSBuild 参数 /p:AtomSkipManagedBuild=true 仅验证原生构建，继续使用已有托管程序集；默认完整构建仍会编译 C#。颜色专用回归不执行脚本行为断言。当前脚本由用户编辑，完整托管构建及脚本行为回归需在编辑完成后验证；本轮未修改脚本源文件。

### 物理、预制体、脚本与多视图扩展

Collider 新增 Triangle Mesh 与 Convex Hull，使用模型的 CPU 几何、节点变换及绑定姿态。Triangle Mesh 仅用于静态刚体；移动物体应选择 Convex Hull。碰撞体中心、旋转和物体缩放参与形状创建；修改形状或删除物体时先移除关联约束。碰撞体可视化显示三角网格边或凸包外壳，不跟随蒙皮动画变形。

Inspector 的 Add component → Joint 支持 Fixed、Distance、Hinge，连接对象采用持久 ID；可选择目标物体、启用/禁用，配置距离范围或铰链轴、角度限制。Fixed 焊接当前相对姿态，Distance/Hinge 使用两个对象的局部锚点。连接 ID 在复制与预制体实例化时重映射，关节设置保存到 XML。

新实例化的预制体记录源对象 ID、实例组 ID、上次源数据基线和子物体映射。属性/组件覆盖根据当前实例与基线计算，刷新时采用三方合并：未修改值接收源更新，覆盖值保留；新增源子物体生成实例，本地删除子物体保持删除覆盖。Scene 面板提供 Refresh prefab instances、Apply instance to prefab、Revert prefab instance，显示选中对象的字段覆盖数量，可展开 Override differences 查看属性路径与基线/实例值。Apply 会把当前实例树写回源文件并更新场景中的其他实例；Revert 恢复源数据和被删除的源子物体。编辑器每秒检查源文件变化，场景加载时刷新链接；不在 Play、文本输入或 gizmo 操作期间自动传播。刷新通过暂存 World 提交，源文件损坏不会覆盖当前场景。旧版本只有 source 路径的数据快照继续可读；重新实例化可建立完整链接，未推断旧快照的子物体对应关系。嵌套预制体和冲突交互界面尚未提供。

.scene 新增可选 Environment 与 EditorCamera，保存天空/漫反射 IBL/镜面 IBL/BRDF LUT 路径、IBL 强度/mip bias、自动/手动曝光、FXAA 开关及编辑视角位置/旋转/FOV/裁剪面。相机和太阳光组件仍通过各自 Camera/DirectionalLight 节点保存。旧文件保留自动曝光并默认启用 FXAA；场景的已保存编辑视角优先于 Editor.xml 的启动默认视角。Scene → Environment 可编辑环境与 FXAA。

Assets 与脚本 Inspector 提供 Create C# script，可生成 Game 命名空间下的 Behaviour 类，勾选生成六个 override 回调；可挂载到选中物体。新挂载项的程序集、类型和字段默认为空。生成文件采用 CREATE_NEW，不覆盖已有源码；源码位于 Managed/GameScripts，生成后 Assets 转到源码目录。Build scripts 在后台构建，并将成功的 AtomScripts 输出原子替换到运行目录 Scripts；构建失败保留运行程序集，日志为 Scripts/ScriptBuild.log。Play 工具栏也提供 Build scripts。

脚本使用可卸载 AssemblyLoadContext，通过内存流加载程序集，避免锁定 DLL。运行中每隔 0.5 秒检查活跃程序集，连续两次稳定时间戳后重载；加载/类型/字段准备失败保留旧程序集。初次缺少 DLL 或类型的挂载项会在成功重载后重新创建。实例实体 ID 与兼容的公开字段值保留，旧实例 OnDestroy 后新实例 OnCreate，再继续 Update/FixedUpdate/碰撞/UI 回调。接口类型 Atom.Managed 保持在宿主上下文，不参与游戏程序集热重载。私有字段、属性、委托及跨程序集的自定义对象图不迁移；生命周期回调的用户副作用不提供事务回滚。

AssetManager::PreloadAsync 将文件读取、模型 CPU 导入与纹理转换放到后台，Step 在引擎线程轮询结果并按资产预算提交 GPU 资源。编辑器打开场景与图片缩略图使用此路径；缩略图最多四项准备中，每帧最多两项 GPU 提交。GPU 上传不在工作线程执行，纹理转换通过互斥量避免重复写 DDS。场景加载期间若旧场景发生修改，保留旧场景并提示重新打开，防止覆盖未保存编辑。原有同步 API 保留，初次 GPU 构建、材质内部依赖、资源销毁等待和启动加载仍可能阻塞。

默认启用 FXAA：对后处理后的场景在线性颜色缓冲中采样，以 sRGB 亮度判断边缘；完成后再合成 runtime UI 与 ImGui，不模糊编辑器文本。没有时间历史、运动重投影或 DLAA 依赖。RenderGraph 通过各 pass 声明的读/写资源构建依赖、检查未初始化读取、重复/无效资源和环，按序插入 D3D12 状态屏障；Forward、Postprocess、FXAA/resolve、Overlay 已接入。现有 Forward/后处理内部子阶段保留自己的细粒度屏障；图采用导入资源，不做显存 aliasing 或临时资源池。

ViewRenderResources 负责每个视图的颜色/深度、法线、阴影、AO 中间缓冲、线性深度历史、速度、Bloom、亮度/直方图/曝光、选中遮罩、FXAA、Overlay 和光照网格/局部光阴影资源，以及各自的 pass 实例、公共描述符表和相机重投影历史。Resize 只重建自身资源并清除自身历史；主视图重建时刷新 AO/阴影/光照网格描述符及阴影预览，重置主视图历史。RenderView 必须使用其 resources 中的颜色/深度；省略 resources 使用主视图。共享的是只读管线、纹理/模型资源和世界粒子模拟数据。当前通过同一渲染线程串行提交视图，不支持并行录制多个视图的命令列表。兼容的 g* 缓冲名路由到当前 ScopedViewBuffers，逐步迁移旧 effect 接口。

隔离回归：先执行 Tests/BuildReloadFixtures.ps1 -Configuration Debug（或 Release），再运行对应 AtomEngine.exe --enhancement-smoke-test。覆盖网格/凸包落地、三类关节及删除连接对象、预制体传播/覆盖/子物体增删/应用/还原、环境和编辑视角序列化、后台纹理与模型导入、自动/显式程序集替换及失败保留、字段保留、两视图输出/历史/resize 隔离、图非法读取与 FXAA 对角线/平坦区域像素，同时运行已有原生和 UI 颜色回归。


编辑器多语言：默认提供简体中文和 English，通过主菜单“语言 / Language”即时切换，偏好写入输出目录 EditorPreferences.xml 的 language 属性。缺少词条时回退到英文；用户对象名、脚本字段名、资源路径和引擎日志不改写。控件使用译文加 ###原始键，保证中英文切换后停靠布局、焦点和控件 ID 一致，单帧执行和吸附步长仍使用不同 ID。

扩展语言：在 Asset/Localization 中加入 UTF-8 XML 文件，启动时自动发现，无需修改菜单代码。格式示例：

```xml
<?xml version="1.0" encoding="utf-8"?>
<Language id="fr" name="Français">
  <Text key="File">Fichier</Text>
  <Text key="Viewport">Vue</Text>
  <Text key="%zu items">%zu éléments</Text>
</Language>
```

EditorLocalization::Text(key) 返回显示文本或原英文；Label(key) 返回带稳定 ID 的 ImGui 标签；Combo 支持翻译英文选项列表。Initialize(directory)、RegisterCatalog(path)、Languages()、CurrentLanguage() 和 SetLanguage(id) 供未来扩展。词条键为源码中的英文文本，区分大小写；使用上下文不同的隐藏 ID 键可提供独立译文，例如 Step##TransformSnapStep。翻译必须保留 printf 占位符的类型、数量、顺序和格式，且不能包含 ImGui 保留的 ## 分隔符；非法词条记录警告并回退。XML 的引号和换行使用标准实体转义；新增语言需确保所用字体覆盖相应字符（当前使用微软雅黑中文字体）。词库在初始化时注册，重注册已有语言应在帧间进行。

--ui-color-smoke-test 额外检查词库加载、缺失词条与未知语言、中英文切换、标签指针和窗口 ID 稳定性；运行过程中切换到英文再恢复中文，Smoke/editor-english.raw 与 editor-restored.raw 分别保留两种界面的像素输出。
