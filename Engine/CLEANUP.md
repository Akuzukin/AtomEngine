# 项目文件清理记录（2026-10-13）

## 已删除

| 类别 | 文件 | 原因 |
| --- | --- | --- |
| 旧编辑器 Panel | `Source/Editor/Panels/{IPanel,ConsolePanel,HierarchyPanel,InspectorPanel,ViewportPanel}.h` | 派生类只有声明，无实现、无实例化；界面由 EditorLayer 实现。同步移除 EditorApp 的空面板列表。 |
| 空翻译单元 | `Source/Runtime/Resource/{Animation,Skeleton}.cpp` | 只有 include；动画和骨骼的真实定义仍保留在对应头文件及 AnimationPlayer 中。 |
| 遗留 ID 类 | `Source/Runtime/Function/Framework/ECS/GUID.{h,cpp}` | ObjectGUID 没有调用者；场景身份已采用 PersistentIdComponent。删除 GameObject 中的无用 include。 |
| 重复随机工具 | `Source/Runtime/Core/Utility/Random.h` | 全局 Random 类无引用；当前使用 Core/Math/Random。 |
| 旧着色器 | `Asset/Shaders/{MeshPS,MeshVS,DepthViewPS,DepthViewVS}.hlsl` | 无加载或包含入口；当前网格、深度和阴影管线使用 Object3D/DepthPass 等着色器。 |
| 旧游戏配置 | `Asset/Config/{platforms,ladders,items,goals}.json` | 已移除的游戏逻辑留下的配置，没有源码、脚本或场景引用。 |
| 旧构建产物 | `Engine/AtomEngine/x64/Debug`（相对于仓库根目录） | 44 个 obj/pdb/idb/tlog/log 等文件，共 23,823,882 字节；构建输出现统一放在 generated。 |

共清理 18 个源码/资源文件及 44 个旧构建产物。删除前的源码与资源完整备份在仓库 `generated/cleanup-backup`，包含路径清单和清理前的项目文件。

## 构建与项目清单

停止编译无调用的 ImGui 附加模块 GraphEditor、ImCurveEdit、ImGradient、ImSequencer 和 imgui_demo；保留第三方原始文件和依赖。删除 31 个空/过时过滤器，包括旧 GameInput 版本的虚拟目录。补齐 GameObject.inl 和 tinyxml2 的清单项，项目与 filters 的文件条目保持一致。

## 保留理由

手柄相机、Trail、粒子、动画、骨骼、渲染接口等有真实实现或承担扩展职责，不能仅凭编辑器未调用就认定为无用。材质、场景、预制体、模型、纹理和第三方依赖保留；没有仅凭静态引用数量删除用户资源。根目录与 Engine 下的 Assimp DLL 版本不同，Debug 与 Release 当前使用的来源也不同，未当作重复文件删除。Visual Studio/编辑器个人设置与当前 generated 构建产物保留。

抽象接口的默认空钩子、默认析构、标记/数据组件属于正常设计，不属于无实现类。本次不调整这些行为。

## 验证

验证项目与过滤器引用的文件存在、清单一致以及已删符号没有剩余引用；执行 Debug/Release 重建、ECS/场景单元检查及编辑器 GPU smoke 回归。编译和运行日志保存在 `generated/cleanup-*`。
