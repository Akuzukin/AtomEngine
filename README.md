# AtomEngine

AtomEngine 是一个面向 Windows x64 的 3D 引擎与场景编辑器。项目基于 Direct3D 12，提供场景编辑、模型与材质渲染、物理模拟、C# 行为脚本及运行时 UI 等功能。当前仓库主要用于引擎功能开发和示例场景验证。

---
## 功能概览

- **场景编辑器**：Hierarchy、Inspector、Viewport、资源浏览、Console 和 Scene 面板；支持场景创建/加载/保存、对象层级编辑、撤销/重做，以及 Play、Pause、Step、Stop。
- **场景与预制体**：以 XML 格式保存 `.scene`、`.prefab` 和材质预设；支持场景加载失败回滚、预制体实例化和资源预加载。
- **模型与动画**：导入模型、网格、骨骼与动画资源；支持实例级动画播放和蒙皮模型渲染。内置 Plane、Cube、Sphere、Quad 图元。
- **材质与光照**：金属度—粗糙度 PBR 材质，包含 Base Color、Metallic、Roughness、Normal、AO、Emissive、Specular 贴图槽；支持方向光、环境光照（IBL）和材质实例覆盖。
- **物理与脚本**：Jolt 刚体模拟，支持 Box、Sphere、Capsule 和 Trigger；可使用 C# 编写 `OnCreate`、`OnUpdate`、`OnFixedUpdate`、`OnCollision`、`OnUiClick`、`OnDestroy` 生命周期脚本。
- **交互与 UI**：键盘、鼠标和手柄输入；运行时 UI 支持图片、UTF-8 文本、按钮点击及脚本回调。编辑器提供视口拾取、相机控制和 ImGuizmo 变换工具。
- **开发辅助**：HLSL/HLSLI 着色器变更检测、编译诊断和成功后管线刷新；文件日志、D3D12 调试层日志，以及 smoke-test 检查入口。

---
## 渲染器实现

- 深度预通道、静态网格与蒙皮网格渲染，以及金属度—粗糙度 PBR 着色。
- 方向光阴影、自动拟合阴影范围和 PCF 过滤；天空盒、IBL 环境贴图与 BRDF LUT。
- SSAO 环境遮蔽，用于增强物体接触和场景凹处的明暗层次。
- HDR/LDR 色调映射、亮度直方图与自适应曝光、Bloom 提取/模糊/合成。
- 相机与物体运动模糊、GPU 粒子更新与绘制、网格和调试绘制、选中对象轮廓。
- 编辑器视口通过 UI Overlay 呈现；运行时 UI 使用 DirectWrite 生成文字纹理并合成，支持中文字体回退。
---
## 基本使用

- 在 **Hierarchy** 中选择或创建场景对象，在 **Inspector** 编辑变换、网格、材质、碰撞体、刚体、脚本和 UI 组件。
- 在 **Viewport** 使用鼠标选择对象，并用移动、旋转、缩放工具调整变换；`Alt + 左键` 旋转相机，中键平移，右键缩放。`F` 聚焦选中对象，`Escape` 取消选择。
- 使用 **Play** 运行场景，Pause 暂停，Step 单步推进，Stop 恢复开始运行前的编辑快照。Stop 不会把模拟结果写回编辑状态。
- 在 **Inspector** 添加 C# Script，Assembly 默认路径为 `Scripts/AtomScripts.dll`，Class 填写完整类型名（例如 `Game.xxx`）。支持C#热重载。
- 场景通过 Scene 面板保存为 `.scene`；选中对象可保存为 `.prefab` 并实例化。快捷键包括 `Ctrl+S` 保存、`Ctrl+Z` 撤销、`Ctrl+Y` 重做、`Ctrl+D` 复制和 `Delete` 删除。
---
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
| .NET 10 Native Hosting | 加载托管运行时并调用 C# 脚本；需要本机安装 .NET 10 x64 Runtime。 |
| Direct3D 12、DXC、DirectWrite、Win32 | Windows 图形、HLSL 编译、文字排版与窗口/平台 API。 |

除 Jolt、TinyXML2 和 .NET Native Hosting 外，其余库的具体版本及许可信息请以各自随附文件和上游项目为准。更多依赖来源说明见 [`External/DEPENDENCIES.md`](External/DEPENDENCIES.md)。

---
## TODO

- [x] FXAA
- [ ] CSM SHADOW
- [ ] GI
- [ ] Raytracing
- [ ] Game Demo
