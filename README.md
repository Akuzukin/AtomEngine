# AtomEngine

AtomEngine 是面向 Windows x64 的 Direct3D 12 3D 引擎与场景编辑器，提供项目管理、场景编辑、PBR 渲染、物理模拟、C# 脚本和独立游戏发布。仓库包含可运行的幸存者示例项目。

---
## 功能概览

- **场景编辑器**：按面板组织的场景层级、属性、视口、资源、材质、脚本、日志、着色器、项目及项目设置；支持对象层级编辑、撤销/重做、视口拾取与变换、Play/Pause/Step/Stop、最大化预览和多视窗游戏输入。
- **项目与发布**：新建、打开和保存 `.atproj` 项目，管理场景清单及启动场景；独立游戏入口、依赖资源收集、加密二进制资产包及 Windows 运行目录/ZIP 发布。发行游戏不依赖项目文件，使用说明见 [PROJECT_WORKFLOW.md](Engine/PROJECT_WORKFLOW.md)。
- **场景与预制体**：以 XML 格式保存 `.scene`、`.prefab` 和材质预设；支持场景加载失败回滚、预制体实例化和资源预加载。
- **模型与动画**：导入模型、网格、骨骼与动画资源；支持实例级动画播放和蒙皮模型渲染。内置 Plane、Cube、Sphere、Quad 图元。
- **材质与光照**：金属度—粗糙度 PBR 材质，包含 Base Color、Metallic、Roughness、Normal、AO、Emissive、Specular 贴图槽；支持方向光、环境光照（IBL）和材质实例覆盖。
- **物理与脚本**：Jolt 刚体、碰撞和 Trigger；C# 生命周期、热重载、Inspector 参数与运行时调整、对象创建及预制体实例化、物理查询、具名按键/移动预设、音频接口、`Mathf`、`Time` 和 `SceneManager`。公开接口提供中文 XML 注释和 IDE 提示。
- **交互与 UI**：键盘、鼠标和手柄输入；运行时 UI 支持图片、UTF-8 文本、按钮点击及脚本回调。编辑器提供视口拾取、相机控制和 ImGuizmo 变换工具。
- **开发辅助**：HLSL/HLSLI 着色器变更检测、编译诊断和成功后管线刷新；文件日志、D3D12 调试层日志，以及 smoke-test 检查入口。

---
## 渲染器实现

- 深度预通道、静态网格与蒙皮网格渲染，以及金属度—粗糙度 PBR 着色。
- 方向光阴影、自动拟合阴影范围和 PCF 过滤；默认程序化天空参与漫反射与镜面环境光照，也支持替换天空纹理、IBL 环境贴图与 BRDF LUT。
- SSAO 环境遮蔽，用于增强物体接触和场景凹处的明暗层次。
- HDR/LDR 色调映射、亮度直方图与自适应曝光、Bloom 提取/模糊/合成。
- 相机与物体运动模糊、GPU 粒子更新与绘制、网格和调试绘制、选中对象轮廓。
- 编辑器视口通过 UI Overlay 呈现；运行时 UI 使用 DirectWrite 生成文字纹理并合成，支持中文字体回退。
---
## 开发环境与构建

当前工程使用 Visual Studio 2026 的 `v145` C++ 工具集、Windows SDK 和 CMake 的 `Visual Studio 18 2026` 生成器。开发机器需要 Windows x64、D3D12 显卡、Visual Studio C++ 桌面开发工具、可在命令行访问的 CMake，以及 .NET 10 x64 SDK。

打开 `Engine/AtomEngine.sln`，还原 `Engine/packages.config` 中的 Microsoft.GameInput NuGet 依赖，选择 **x64 / Debug 或 Release** 后构建。构建步骤自动编译 Jolt 和托管脚本，并部署编辑器资源。也可从仓库根目录，在 Visual Studio 开发者 PowerShell 中执行：

```powershell
msbuild Engine/AtomEngine.sln /m /p:Configuration=Release /p:Platform=x64
```

编辑器输出位于 `generated/outputs/x64/<Configuration>/AtomEngine.exe`。主解决方案默认构建编辑器，游戏由发布工具单独构建。`generated`、C++/.NET 中间文件和 IDE 缓存不作为源码提交。

## 基本使用

- 通过“文件 → 新建项目/打开项目”管理 `.atproj`；通过“项目 → 项目场景”设置启动场景和场景清单，`Ctrl+Alt+S` 保存项目。

- 在 **Hierarchy** 中选择或创建场景对象，在 **Inspector** 编辑变换、网格、材质、碰撞体、刚体、脚本和 UI 组件。
- 在 **Viewport** 使用鼠标选择对象，并用移动、旋转、缩放工具调整变换；`Alt + 左键` 旋转相机，中键平移，右键缩放。`F` 聚焦选中对象，`Escape` 取消选择。
- 使用 **Play** 运行场景，Pause 暂停，Step 单步推进，Stop 恢复开始运行前的编辑快照。Stop 不会把模拟结果写回编辑状态。
- 在 **Inspector** 添加 C# Script，Assembly 默认路径为 `Scripts/AtomScripts.dll`，Class 填写完整类型名（例如 `Game.Survivor`）。推荐使用 VS Code，打开 `Engine/Managed/AtomScripts.code-workspace`；新项目也会生成脚本工程和工作区。完整构建或编辑器脚本构建操作负责部署程序集并触发热重载，原生解决方案仍由 Visual Studio 使用。
- 场景通过 Scene 面板保存为 `.scene`；选中对象可保存为 `.prefab` 并实例化。快捷键包括 `Ctrl+S` 保存、`Ctrl+Z` 撤销、`Ctrl+Y` 重做、`Ctrl+D` 复制和 `Delete` 删除。
---
## 幸存者示例与独立发布

打开 `Engine/Survivors.atproj` 并点击 Play。Enter 或“开始”按钮进入游戏，Escape 返回标题；WASD / 手柄左摇杆移动，自动向附近敌人发射弹丸，生存 120 秒获胜。敌人死亡播放 `Asset/Sounds/se.mp3`。项目包含标题和游戏两个场景，原始场景与模板仍保留。

编辑器“构建游戏”生成 Release 运行目录，“构建并发布”额外生成 ZIP。从仓库根目录执行相同流程：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Engine/Tools/Publish-Project.ps1 `
  -Project Engine/Survivors.atproj -Output generated/published/Survivors
```

解压 ZIP 后运行 `AtomGame.exe`。发行包包含 .NET 10 和 VC 运行库，玩家无需安装开发 SDK，仍需要 Windows x64 与 D3D12 显卡。

**`.atproj` 仅用于开发流程，发行游戏不携带或读取项目文件。** 游戏需要的窗口参数、启动场景、场景清单和渲染默认值作为独立运行配置存入 `game.atpak`。编辑器代码、ImGui、编辑器网格/选中轮廓、布局、模板、源码、调试符号和构建日志不进入游戏包。

资产包的索引和资源内容采用 **AES-256-GCM** 加密，每次发布使用独立随机密钥；游戏校验完整性并按需在内存解密场景、模型、纹理、材质、预制体、着色器和音频，不释放明文资产目录。单项资源上限为 512 MiB。脚本 DLL 和系统运行库仍按程序依赖发布。离线客户端包含解密能力，加密不能保证阻止逆向提取。

脚本动态加载或拼接路径使用的资源需加入项目的“附加游戏资源”。发布失败保留上一份目录与 ZIP。完整依赖规则和加密格式见 [项目与发布文档](Engine/PROJECT_WORKFLOW.md)。

## 验证入口

已完成 Debug/Release 构建、项目与游玩回归、独立发行包场景往返、真实模型纹理/材质/预制体/音频加载、加密包完整性与错误密钥检查，以及发布失败回滚验证。具体记录见项目文档；尚未在另一台干净机器上进行发行包兼容性测试。

- 编辑器 `--smoke-test`：综合回归。
- 编辑器 `--project-workflow-smoke-test`：项目保存、场景切换、失败回滚及反复 Play/Stop。
- 编辑器 `--play-workflow-smoke-test`：视口尺寸、全屏预览及多视窗输入。
- 发行游戏 `--smoke-test`：独立运行和场景往返；`--inspect-package` 校验包内资源。
- `Engine/Tests/ProjectPublisherSmoke.ps1`：依赖收集、内存加载、篡改/错误密钥/缺包拒绝和输出保护。
- `Engine/Tests/ScriptApi`、`Engine/Tests/SurvivorsLogic`：脚本接口/补全与 Demo 逻辑检查。

## 第三方库与平台组件

| 组件 | 用途 / 说明 |
| --- | --- |
| [Jolt Physics](https://github.com/jrouwe/JoltPhysics) 5.5.0 | 刚体与碰撞模拟；仓库中包含源码，许可见 `External/Jolt/LICENSE`。 |
| [TinyXML2](https://github.com/leethomason/tinyxml2) 11.0.0 | 场景、预制体及配置 XML；许可见 `External/tinyxml2/LICENSE.txt`。 |
| [Assimp](https://github.com/assimp/assimp) | 模型导入。 |
| [DirectXTex](https://github.com/microsoft/DirectXTex) | Direct3D 纹理处理与加载。 |
| [EnTT](https://github.com/skypjack/entt) | ECS 实体与组件管理。 |
| [nlohmann/json](https://github.com/nlohmann/json) | JSON 数据处理。 |
| [Dear ImGui](https://github.com/ocornut/imgui) 与 ImGuizmo | 编辑器 UI 和视口变换 gizmo。 |
| Microsoft.GameInput 3.5.283 | GameInput 手柄输入；NuGet 包依赖。另有 XInput 回退路径。 |
| .NET 10 Native Hosting | 开发使用 .NET 10 x64 SDK；发行游戏携带运行库，加载 C# 脚本。 |
| Direct3D 12、DXC、DirectWrite、Win32 | 图形、HLSL 编译、中文文字排版和窗口。 |
| Media Foundation、XAudio2、BCrypt | 音频解码/播放及资产认证加密。 |

除 Jolt、TinyXML2 和 .NET Native Hosting 外，其余库的具体版本及许可信息请以各自随附文件和上游项目为准。更多依赖来源说明见 [`Engine/External/DEPENDENCIES.md`](Engine/External/DEPENDENCIES.md)。

---
## 文档入口

- [功能与使用说明](Engine/FEATURES.md)：当前能力、操作步骤和已知限制的统一来源。
- [架构](Engine/ARCHITECTURE.md)：模块职责与工程边界。
- [幸存者 Demo](Engine/DEMO_SURVIVORS.md)：玩法、运行方法和验证记录。
- [Demo 制作问题](Engine/DEMO_ISSUES.md)：制作过程中的问题与跟进。
- [脚本 API](Engine/SCRIPT_API.md)：时间、数学、输入、参数、对象、预制体、音频与场景接口。
- [项目与发布](Engine/PROJECT_WORKFLOW.md)：项目、场景切换、构建、加密资产包及验证记录。

CSM、GI、Raytracing 等仍是后续渲染方向，不属于当前已实现能力。
