# 项目、场景与 Windows 发布

## 编辑器使用

1. 在“文件 → 新建项目”选择 `.atproj` 文件。项目文件所在目录是资源根目录，编辑器生成 `Asset/Scenes/Start.scene`、资源目录、`Managed/GameScripts/GameScripts.csproj` 和 VS Code 工作区。
2. “文件 → 打开项目”加载 `.atproj` 和启动场景。也可启动 `AtomEngine.exe --project "D:\Games\MyGame\MyGame.atproj"`。
3. 在“项目 → 项目场景”管理构建清单。输入项目内的 `.scene` 路径或点击“加入当前场景”，用单选按钮指定启动场景，点击列表中的场景可编辑它。
4. “保存项目”（Ctrl+Alt+S）保存项目配置和当前已修改的场景。项目、场景切换或退出时，未保存修改提供保存、放弃、取消选项；Play 时禁止修改项目配置。
5. “设置 → 项目设置”的渲染默认值在打开项目时保存到 `.atproj`。没有打开项目时继续兼容 `Asset/ProjectSettings.xml`；已保存场景自己的设置优先。
6. “构建游戏”生成可运行的 Release 目录。“构建并发布”同时生成 ZIP。默认输出 `Build/Windows`，自定义输出路径也保存到项目；相对路径以项目目录为基准。

`.atproj` 保存项目名、场景清单、启动场景、脚本工程、附加资源、窗口尺寸/全屏模式和渲染默认值。场景及资源路径不允许离开项目目录。文件原子替换，错误配置不会覆盖已保存的项目。

新建脚本工程引用 Atom.Managed 的源码 SDK。编辑器和发布工具构建时提供 `AtomEngineSdkProject` 路径；迁移开发机器后，IDE 中的 SDK 默认路径可在脚本工程内调整。现有项目继续兼容原有脚本工程。

## 脚本切换场景

```csharp
using Atom;
public sealed class EnterLevel : Behaviour
{
    public string Destination = "Level1";
    public override void OnUiClick() => SceneManager.LoadScene(Destination);
    public override void OnUpdate(float dt)
    {
        if (Object.Input.WasPressed(KeyCode.Enter))
            SceneManager.LoadScene(Destination);
    }
}
```

`SceneManager` 也可通过 `Object.SceneManager` 访问；提供 `Scenes`、`CurrentScene`、`IsLoading` 和 `LastLoadError`，接口有中文 XML 注释和 IDE 提示。

目标必须在项目场景清单内，可传入文件名称（不含扩展名）或完整相对路径；同名场景必须用路径区分。脚本发起的请求在下一帧处理。只有新场景验证、资源预加载成功后，才清理旧脚本、物理和音频并替换世界；失败时保留原世界和原脚本实例，通过 `LastLoadError` 查询原因。成功切换会重置场景时间和帧计数，旧对象句柄失效。Stop 仍还原开始 Play 前的编辑场景及相机。

## 发布内容

独立 `AtomGame.vcxproj` 使用游戏入口，不编译 Editor、ImGui、ImGuizmo、粒子编辑器、选中轮廓和编辑器网格。游戏和编辑器使用独立的中间文件、编译跟踪记录、PDB 与输出目录。主解决方案默认构建编辑器；发布命令直接构建游戏目标。

发布从场景清单递归收集场景、材质/预制体引用、脚本字段中的资源路径、模型缓冲与纹理、运行着色器及其 include。程序化天空不再加载或打包未使用的天空纹理和预计算漫反射/镜面环境贴图。DirectWrite 游戏 UI 和游戏音效保留；编辑器配置、图标相关着色器、停靠布局、模板库、源码、PDB 和构建日志不进入发行包。场景内的 EditorCamera 也从发行副本移除，不修改源场景。

脚本在代码中拼接路径或加载未写入场景的资源时，在项目面板的“附加游戏资源”显式加入文件或目录。发布工具不会猜测任意脚本的运行分支。模型中的绝对纹理路径需先改为可移植的相对路径；OBJ/FBX/glTF 等模型材质纹理经 Assimp 收集，glTF 外部缓冲和 OBJ 材质文件另行收集。

`.atproj` 只用于编辑器打开项目和发布工具读取构建输入，不进入发行目录，游戏启动不读取项目文件。发布时提取项目名、窗口参数、启动场景、可加载场景清单和渲染默认值，写入资产包内的 `__runtime/startup.json`；脚本工程、构建路径、编辑器设置不进入运行配置。运行时使用独立的 `GameLaunchSettings`，不创建编辑器项目会话。

发行目录包含 `AtomGame.exe`、加密二进制资产包 `game.atpak`、脚本 DLL，以及随包的 .NET 10 和 VC 运行库；玩家启动无需开发 SDK。场景、模型及其缓冲/纹理、材质、预制体、着色器和音效统一收进资产包，不再生成明文 `Asset` 目录。运行目标为支持 D3D12 的 Windows x64。`Licenses` 保存仓库中随附的运行依赖许可文本，`build-manifest.json` 仅记录实际发行文件的大小与 SHA256。

## 加密资产包

`game.atpak` 的索引和每项资源使用 AES-256-GCM 加密，分别生成随机 12 字节 nonce 和 16 字节认证标签；资源路径作为附加认证数据。每次发布生成独立随机密钥，密钥与整包 SHA256 写入本次可执行文件的构建尾部，不生成密钥配置文件。游戏在 GPU 初始化前校验包摘要和索引认证，按需解密资源；资源被修改、包缺失、密钥与包不匹配时明确报错。发布前也会用生成的可执行文件完整解密校验资产包，验证失败不替换旧包。

场景/预制体/材质从内存解析 XML；Assimp 使用包内 IO 读取模型及关联文件；DXC 从包内读取着色器和相对 include；纹理从内存解码并生成源图片 mip 链，音频通过内存字节流交给 Media Foundation。游戏不把解密资产或转换后的 DDS 写到磁盘；日志仍写入发行目录的 `logs`。编辑器未挂载资产包，继续使用普通文件、纹理缓存和着色器热重载。

当前格式版本为 1，单项资源上限 512 MiB。脚本 DLL、.NET 和本地运行库属于程序加载依赖，仍以普通文件发布。离线游戏必须包含解密能力，因此加密能避免直接查看资产并检测修改，不能保证阻止逆向提取。实现使用 Windows [BCrypt 的认证加密接口](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/ns-bcrypt-bcrypt_authenticated_cipher_mode_info)。

每次发布先生成临时目录，成功后替换旧发行包。失败保留上一份目录和 ZIP；输出位置不能覆盖源码、资源、项目目录或未经本工具创建的已有目录。

## 命令行发布

从仓库根目录执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Engine/Tools/Publish-Project.ps1 `
  -Project "D:/Games/MyGame/MyGame.atproj" -Output "D:/Games/MyGame/Build/Windows"
```

`-BuildOnly` 只生成运行目录，`-SkipNativeBuild` 用于已有正确游戏二进制的开发验证。正常发布不跳过原生构建。

项目文件由编辑器“新建项目”生成；发布时使用自己的 .atproj 路径。

回归入口：编辑器 `--project-workflow-smoke-test` 覆盖项目创建/重开、保存验证、脚本切换、时间重置、失败回滚、Stop 和再次 Play；游戏 `--smoke-test` 覆盖独立运行及场景往返，`--inspect-package` 校验全部资产并输出包内路径清单。`Tests/ProjectPublisherSmoke.ps1` 验证真实模型纹理、预制体/材质/音频内存加载、编辑器资源剔除、加密索引、篡改/错误密钥/缺包拒绝、失败发布和输出保护。

## 历史验证（2026-10-14）

- Debug、Release 编辑器构建及项目工作流回归通过；Debug 游玩回归和 Release 编辑器综合回归通过。
- 脚本类型接口、中文 XML 提示与 Roslyn 补全检查通过；`SceneManager` 的场景列表、加载、错误和等待状态均有补全提示。
- 发布依赖测试通过：模型缓冲/纹理、预制体、音效进入游戏包；非法窗口配置、ZIP 被占用及非发行输出目录均被拒绝，并保留旧包。
- 原有明文发行版本的 ZIP 解压运行和文件哈希验证通过；加密发行版本的验证记录在下方补充。

加密发行版本验证结果：

- Debug、Release 编辑器重新构建与项目工作流回归通过；Debug 游玩回归、Release 综合回归通过。
- 模型使用真实 JPG 纹理；包内材质、预制体和 MP3 从内存加载通过；附加目录 `GameData` 的资源也进入加密包，不保留明文副本。
- 完整资产解密校验、加密索引、篡改/错误密钥/缺包拒绝通过；非法配置、ZIP 占用和无关输出目录保护通过。
- 当时使用的示例发行包完成解压后文件哈希、从包外目录启动、随包 .NET 和场景往返验证；该示例游戏现已移除，发布功能与独立回归夹具保留。

示例发行包为 Windows x64 Release，测试机器使用 D3D12 显卡。尚未在另一台干净机器上进行兼容性测试。
