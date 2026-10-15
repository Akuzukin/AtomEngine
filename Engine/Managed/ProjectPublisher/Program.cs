using System.Diagnostics;
using System.IO.Compression;
using System.Security.Cryptography;
using System.Text.Json;
using System.Text.RegularExpressions;
using System.Xml.Linq;

// 发布时使用白名单依赖图，不复制编辑器部署目录。失败的发布保留上一份完整发行包。
try { new Publisher(args).Publish(); return 0; }
catch (Exception error) { Console.Error.WriteLine(error.Message); return 1; }

sealed class Publisher
{
    readonly string engine, projectFile, root, output, native, managed;
    readonly bool archive, skipNative;
    readonly XDocument project;
    readonly Dictionary<string, string> files = new(StringComparer.OrdinalIgnoreCase);
    readonly HashSet<string> scanned = new(StringComparer.OrdinalIgnoreCase);
    readonly Queue<string> queue = new();
    static readonly HashSet<string> EditorShaders = new(StringComparer.OrdinalIgnoreCase)
    { "ImGuiPresentPS.hlsl", "DepthViewPS.hlsl", "DepthViewVS.hlsl", "GridVS.hlsl", "GridPS.hlsl", "SelectionMaskPS.hlsl", "SelectionOutline.hlsl" };
    public Publisher(string[] args)
    {
        string Option(string key) { var i = Array.IndexOf(args, key); if (i < 0 || i + 1 >= args.Length) throw new ArgumentException("Missing " + key); return Path.GetFullPath(args[i + 1]); }
        engine = Option("--engine"); projectFile = Option("--project"); output = Option("--output"); root = Path.GetDirectoryName(projectFile)!;
        archive = !args.Contains("--build-only"); skipNative = args.Contains("--skip-native-build");
        native = Path.GetFullPath(Path.Combine(engine, "../generated/game/x64/Release"));
        managed = Path.GetFullPath(Path.Combine(engine, "../generated/publisher/managed-" + Guid.NewGuid().ToString("N")));
        project = XDocument.Load(projectFile);
    }
    static bool Inside(string path, string directory) => Path.GetFullPath(path).StartsWith(Path.GetFullPath(directory).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase);
    static string Relative(string value)
    {
        value = value.Replace('\\', '/');
        if (string.IsNullOrWhiteSpace(value) || Path.IsPathRooted(value) || value.Split('/').Contains("..") || value.Contains(':')) throw new Exception("Invalid project asset path: " + value);
        return value.TrimStart('.', '/');
    }
    static string Run(string exe, string work, params string[] args)
    {
        var start = new ProcessStartInfo(exe) { WorkingDirectory = work, UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true, RedirectStandardError = true };
        foreach (var arg in args) start.ArgumentList.Add(arg);
        using var process = Process.Start(start) ?? throw new Exception("Cannot launch " + exe);
        var stdout = process.StandardOutput.ReadToEndAsync(); var stderr = process.StandardError.ReadToEndAsync();
        process.WaitForExit(); Task.WaitAll(stdout, stderr);
        if (!exe.EndsWith("vswhere.exe", StringComparison.OrdinalIgnoreCase) && !args.Contains("--list-runtimes") && !args.Contains("--inspect-package") && !args.Contains("--model-dependencies")) Console.Write(stdout.Result);
        Console.Error.Write(stderr.Result);
        if (process.ExitCode != 0) throw new Exception(exe + " failed (" + process.ExitCode + ")");
        return stdout.Result;
    }
    static bool EditorAsset(string value) => value.Equals("Asset/Editor.xml", StringComparison.OrdinalIgnoreCase) || value.Equals("Asset/ProjectSettings.xml", StringComparison.OrdinalIgnoreCase)
        || Path.GetExtension(value).ToLowerInvariant() is ".meta" or ".tmp" or ".pdb" or ".log" or ".cs" or ".csproj" or ".atproj";
    void Add(string relative, string? physical = null)
    {
        relative = Relative(relative);
        if (EditorAsset(relative)) return;
        if (physical == null && files.ContainsKey(relative)) return;
        physical ??= File.Exists(Path.Combine(root, relative)) || Directory.Exists(Path.Combine(root, relative)) ? Path.Combine(root, relative) : Path.Combine(engine, relative);
        physical = Path.GetFullPath(physical);
        if (!Inside(physical, root) && !Inside(physical, engine) && !Inside(physical, native) && !Inside(physical, Path.Combine(engine, "../generated/publisher"))) throw new Exception("Asset is outside allowed roots: " + physical);
        if ((File.GetAttributes(physical) & FileAttributes.ReparsePoint) != 0) throw new Exception("Asset links are unsupported: " + physical);
        if (Directory.Exists(physical))
        {
            foreach (var entry in Directory.EnumerateFileSystemEntries(physical)) Add(relative + "/" + Path.GetFileName(entry), entry);
            return;
        }
        if (!File.Exists(physical)) throw new Exception("Missing game dependency: " + relative);
        if (files.TryGetValue(relative, out var previous)) { if (!previous.Equals(physical, StringComparison.OrdinalIgnoreCase)) throw new Exception("Conflicting asset: " + relative); return; }
        files.Add(relative, physical); queue.Enqueue(relative);
    }
    void Reference(string value)
    {
        value = value.Trim();
        if (Path.IsPathRooted(value) && File.Exists(value))
        {
            if (Path.GetExtension(value) == ".dll" && files.ContainsKey("Scripts/" + Path.GetFileName(value))) Add("Scripts/" + Path.GetFileName(value));
            else if (Inside(value, root)) Add(Path.GetRelativePath(root, value).Replace('\\', '/'), value);
            else if (Inside(value, Path.Combine(engine, "Asset"))) Add("Asset/" + Path.GetRelativePath(Path.Combine(engine, "Asset"), value).Replace('\\', '/'), value);
            else throw new Exception("Non-portable absolute asset reference: " + value);
            return;
        }
        if ((value.EndsWith(".hlsl") || value.EndsWith(".hlsli")) && !value.StartsWith("Asset/")) { Add("Asset/Shaders/" + value); return; }
        if (value.StartsWith("Asset/", StringComparison.OrdinalIgnoreCase) || value.StartsWith("Scripts/", StringComparison.OrdinalIgnoreCase)) Add(value);
        else if (value.StartsWith('"') || value.StartsWith('{') || value.StartsWith('['))
        {
            try { using var json = JsonDocument.Parse(value); JsonRefs(json.RootElement); } catch (JsonException) { }
        }
        else if (value.StartsWith('<')) { try { XmlRefs(XElement.Parse(value)); } catch (System.Xml.XmlException) { } }
    }
    void JsonRefs(JsonElement node)
    {
        if (node.ValueKind == JsonValueKind.String) Reference(node.GetString()!);
        else if (node.ValueKind == JsonValueKind.Array) foreach (var child in node.EnumerateArray()) JsonRefs(child);
        else if (node.ValueKind == JsonValueKind.Object) foreach (var child in node.EnumerateObject()) JsonRefs(child.Value);
    }
    void XmlRefs(XElement node)
    {
        foreach (var element in node.DescendantsAndSelf())
        {
            foreach (var attr in element.Attributes())
            {
                if (element.Name == "Environment" && ((int?)element.Attribute("skyMode") ?? 0) == 0 && attr.Name.LocalName is "sky" or "diffuse" or "specular") continue;
                Reference(attr.Value);
            }
            if (!element.HasElements) Reference(element.Value);
        }
    }
    void Dependencies(string path)
    {
        if (!scanned.Add(path)) return;
        var file = files[path]; var extension = Path.GetExtension(path).ToLowerInvariant();
        if (extension is ".scene" or ".prefab" or ".mat" or ".xml") XmlRefs(XDocument.Load(file).Root!);
        if (extension is ".json" or ".particle") { using var json = JsonDocument.Parse(File.ReadAllText(file)); JsonRefs(json.RootElement); }
        if (extension is ".hlsl" or ".hlsli" or ".h")
        {
            foreach (Match include in Regex.Matches(Regex.Replace(File.ReadAllText(file), @"(?ms)^\s*#ifdef __cplusplus\b.*?^\s*#endif[^\r\n]*", ""), "#\\s*include\\s*\"([^\"]+)\""))
            {
                var name = include.Groups[1].Value;
                var nearby = Path.GetFullPath(Path.Combine(Path.GetDirectoryName(file)!, name));
                var target = Path.GetFullPath(Path.Combine(root, Path.GetDirectoryName(path)!, name));
                if (File.Exists(nearby)) Add(Path.GetRelativePath(root, target).Replace('\\', '/'), nearby);
                else
                {
                    var shader = Path.Combine(engine, "Asset/Shaders", name);
                    if (!File.Exists(shader)) shader = Path.Combine(engine, "Source/Runtime/Platform/DirectX12/Shader", name);
                    Add("Asset/Shaders/" + name, shader);
                }
            }
        }
        if (extension is ".obj" or ".fbx" or ".gltf" or ".glb" or ".dae" or ".3ds" or ".blend")
        {
            var result = Run(Path.Combine(native, "AtomGame.exe"), native, "--model-dependencies", file);
            var dependencies = JsonSerializer.Deserialize<string[]>(result.Trim()) ?? [];
            foreach (var name in dependencies) ModelRef(name);
            if (extension == ".obj") foreach (var line in File.ReadLines(file)) if (line.TrimStart().StartsWith("mtllib ")) ModelRef(line.Trim()[7..].Trim());
            if (extension == ".gltf")
            {
                using var json = JsonDocument.Parse(File.ReadAllText(file));
                foreach (var key in new[] { "buffers", "images" }) if (json.RootElement.TryGetProperty(key, out var items))
                    foreach (var item in items.EnumerateArray()) if (item.TryGetProperty("uri", out var uri) && !uri.GetString()!.StartsWith("data:")) ModelRef(Uri.UnescapeDataString(uri.GetString()!));
            }
            void ModelRef(string name)
            {
                if (name.StartsWith('*')) return;
                if (Path.IsPathRooted(name)) throw new Exception("Model contains an absolute texture path: " + name);
                var physical = Path.GetFullPath(Path.Combine(Path.GetDirectoryName(file)!, name));
                var destination = Path.GetFullPath(Path.Combine(root, Path.GetDirectoryName(path)!, name));
                if (!Inside(destination, root)) throw new Exception("Model dependency leaves project: " + name);
                Add(Path.GetRelativePath(root, destination).Replace('\\', '/'), physical);
            }
        }
    }
    static void CopyTree(string source, string destination)
    {
        foreach (var file in Directory.EnumerateFiles(source, "*", SearchOption.AllDirectories))
        {
            if (Path.GetExtension(file) is ".pdb" or ".xml") continue;
            var target = Path.Combine(destination, Path.GetRelativePath(source, file)); Directory.CreateDirectory(Path.GetDirectoryName(target)!); File.Copy(file, target, true);
        }
    }
    public void Publish()
    {
        try { PublishCore(); }
        finally { if (Directory.Exists(managed)) Directory.Delete(managed, true); }
    }
    void PublishCore()
    {
        if (Path.GetExtension(projectFile) != ".atproj" || project.Root?.Name != "AtomProject" || (int?)project.Root.Attribute("version") != 1) throw new Exception("Invalid .atproj file");
        var data = project.Root;
        var scenes = data.Element("Scenes")?.Elements("Scene").Select(n => Relative((string?)n.Attribute("path") ?? "")).ToArray() ?? [];
        var startup = (string?)data.Attribute("startupScene") ?? "";
        if (scenes.Length == 0 || !scenes.Contains(startup) || scenes.Distinct(StringComparer.OrdinalIgnoreCase).Count() != scenes.Length) throw new Exception("Invalid project scene list/startup scene");
        string gameName = (string?)data.Attribute("name") ?? "Atom Game";
        int windowWidth = (int?)data.Attribute("width") ?? 1280, windowHeight = (int?)data.Attribute("height") ?? 720;
        if (string.IsNullOrEmpty(gameName) || System.Text.Encoding.UTF8.GetByteCount(gameName) > 128) throw new Exception("Invalid project name");
        if (windowWidth < 320 || windowWidth > 7680 || windowHeight < 240 || windowHeight > 4320) throw new Exception("Invalid game window size");
        foreach (string scene in scenes) if (Path.GetExtension(scene) != ".scene" || !File.Exists(Path.Combine(root, scene))) throw new Exception("Missing project scene: " + scene);
        if (data.Element("Scene")?.Element("Object") != null) throw new Exception("Project defaults cannot contain scene objects");
        // 输出只能覆盖本工具创建的发行目录，不能覆盖项目、源码、资源或任意现有目录。
        if (output.Equals(root, StringComparison.OrdinalIgnoreCase) || Inside(root, output) || output.Equals(engine, StringComparison.OrdinalIgnoreCase) || Inside(engine, output)
            || Inside(output, Path.Combine(root, "Asset")) || Inside(output, Path.Combine(root, "Managed")) || Inside(output, Path.Combine(engine, "Source")) || Inside(output, Path.Combine(engine, "External"))) throw new Exception("Unsafe build output directory");
        if (Directory.Exists(output) && (!File.Exists(Path.Combine(output, ".atom-publish")) || (File.GetAttributes(output) & FileAttributes.ReparsePoint) != 0)) throw new Exception("Output exists and is not an Atom publish directory");
        if (!skipNative)
        {
            var vswhere = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "Microsoft Visual Studio/Installer/vswhere.exe");
            var install = Run(vswhere, engine, "-latest", "-products", "*", "-requires", "Microsoft.Component.MSBuild", "-property", "installationPath").Trim();
            Run(Path.Combine(install, "MSBuild/Current/Bin/MSBuild.exe"), engine, "AtomGame.vcxproj", "/m", "/p:Configuration=Release", "/p:Platform=x64", "/p:AtomSkipManagedBuild=true", "/nologo", "/v:minimal");
        }
        var scripts = (string?)data.Attribute("scriptProject") ?? "";
        Directory.CreateDirectory(managed);
        Run("dotnet", engine, "build", Path.Combine(engine, "Managed/Atom.Managed/Atom.Managed.csproj"), "-c", "Release", "-o", managed, "--nologo");
        if (!string.IsNullOrEmpty(scripts)) Run("dotnet", root, "build", Path.Combine(root, Relative(scripts)), "-c", "Release", "-o", managed, "--nologo", "-p:AtomEngineSdkProject=" + Path.Combine(engine, "Managed/Atom.Managed/Atom.Managed.csproj"));
        foreach (var file in Directory.EnumerateFiles(managed)) if (Path.GetExtension(file) == ".dll" || file.EndsWith(".json")) Add("Scripts/" + Path.GetFileName(file), file);
        foreach (var scene in scenes) Add(scene);
        if (data.Element("Scene") is { } defaults) XmlRefs(defaults);
        foreach (var item in data.Element("Include")?.Elements("Asset") ?? []) Add((string?)item.Attribute("path") ?? "");
        // 从运行代码实际引用的着色器入口开始，再递归收集 include。编辑器入口不参与。
        foreach (var source in Directory.EnumerateFiles(Path.Combine(engine, "Source/Runtime"), "*", SearchOption.AllDirectories).Where(p => Path.GetExtension(p) is ".cpp" or ".h" or ".hpp" or ".inl"))
        {
            if (Path.GetFileName(source) is "SelectionRenderer.cpp" or "GridRenderer.cpp" or "ParticleComponentEditor.cpp") continue;
            foreach (Match shader in Regex.Matches(File.ReadAllText(source), "\"([^\"]+\\.hlsli?)\""))
            {
                var name = shader.Groups[1].Value;
                if (!EditorShaders.Contains(Path.GetFileName(name))) Add("Asset/Shaders/" + name);
            }
        }
        foreach (var size in new[] { 8, 16, 24, 32 }) Add($"Asset/Shaders/Light/FillLightGridCS_{size}.hlsl");
        // 渲染器初始化和缺省环境实际使用的运行资源，即使场景没有 Environment 节点也必须保留。
        Add("Asset/Textures/Environment/NightSkyHDRI004_4K/NightSkyHDRI004_4KBrdf.dds");
        while (queue.TryDequeue(out var path)) Dependencies(path);
        var stage = output + ".staging-" + Guid.NewGuid().ToString("N"); Directory.CreateDirectory(stage);
        try
        {
            foreach (var pair in files)
            {
                var target = Path.Combine(stage, pair.Key); Directory.CreateDirectory(Path.GetDirectoryName(target)!); if (Path.GetExtension(target).ToLowerInvariant() is ".scene" or ".prefab" or ".mat" or ".xml")
                {
                    var scene = XDocument.Load(pair.Value); scene.Root?.Element("EditorCamera")?.Remove();
                    foreach (var attr in scene.Root!.DescendantsAndSelf().Attributes())
                    {
                        if (!Path.IsPathRooted(attr.Value) || !File.Exists(attr.Value)) continue;
                        if (Path.GetExtension(attr.Value) == ".dll" && files.ContainsKey("Scripts/" + Path.GetFileName(attr.Value))) attr.Value = "Scripts/" + Path.GetFileName(attr.Value);
                        else if (Inside(attr.Value, root)) attr.Value = Path.GetRelativePath(root, attr.Value).Replace('\\', '/');
                        else if (Inside(attr.Value, Path.Combine(engine, "Asset"))) attr.Value = "Asset/" + Path.GetRelativePath(Path.Combine(engine, "Asset"), attr.Value).Replace('\\', '/');
                    }
                    scene.Save(target);
                }
                else File.Copy(pair.Value, target);
            }
            foreach (var name in new[] { "AtomGame.exe", "assimp-vc143-mt.dll", "dxcompiler.dll", "dxil.dll", "nethost.dll" }) File.Copy(Path.Combine(native, name), Path.Combine(stage, name));
            // dotnet run 使用项目 apphost；以已安装的 x64 Microsoft.NETCore.App 列表定位可再分发运行时。
            var runtimes = Run("dotnet", engine, "--list-runtimes");
            var match = Regex.Matches(runtimes, @"Microsoft.NETCore.App (10\.\d+\.\d+) \[([^\]]+)\]").Cast<Match>().OrderByDescending(m => Version.Parse(m.Groups[1].Value)).FirstOrDefault() ?? throw new Exception(".NET 10 x64 runtime is required for publishing");
            var version = match.Groups[1].Value; var shared = match.Groups[2].Value; var dotnetRoot = Directory.GetParent(shared)!.Parent!.FullName;
            CopyTree(Path.Combine(shared, version), Path.Combine(stage, "Runtime/shared/Microsoft.NETCore.App", version));
            var host = Directory.GetDirectories(Path.Combine(dotnetRoot, "host/fxr")).OrderByDescending(p => Version.TryParse(Path.GetFileName(p), out var v) ? v : new Version()).First();
            CopyTree(host, Path.Combine(stage, "Runtime/host/fxr", Path.GetFileName(host)));
            var vswhere = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "Microsoft Visual Studio/Installer/vswhere.exe");
            var install = Run(vswhere, engine, "-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath").Trim();
            var redist = Directory.GetDirectories(Path.Combine(install, "VC/Redist/MSVC")).Where(p => Directory.Exists(Path.Combine(p, "x64"))).OrderDescending().First();
            var crt = Directory.GetDirectories(Path.Combine(redist, "x64"), "Microsoft.VC*.CRT").First();
            foreach (var file in Directory.EnumerateFiles(crt, "*.dll")) File.Copy(file, Path.Combine(stage, Path.GetFileName(file)), true);
            var notices = new[] { ("External/Jolt/LICENSE", "Jolt.txt"), ("External/tinyxml2/LICENSE.txt", "TinyXML2.txt"), ("External/dotnet/LICENSE.txt", "DotNet.txt"), ("packages/Microsoft.GameInput.3.5.283/LICENSE.txt", "GameInput.txt") };
            Directory.CreateDirectory(Path.Combine(stage, "Licenses"));
            foreach (var (source, name) in notices) if (File.Exists(Path.Combine(engine, source))) File.Copy(Path.Combine(engine, source), Path.Combine(stage, "Licenses", name));
            foreach (var name in new[] { "LICENSE.txt", "ThirdPartyNotices.txt" }) if (File.Exists(Path.Combine(dotnetRoot, name))) File.Copy(Path.Combine(dotnetRoot, name), Path.Combine(stage, "Licenses", "Runtime-" + name));
            var runtimeDefaults = new XElement(data.Element("Scene") ?? new XElement("Scene", new XAttribute("version", 1)));
            runtimeDefaults.Element("EditorCamera")?.Remove();
            AssetArchiveWriter.Write(stage, JsonSerializer.Serialize(new
            {
                version = 1,
                name = gameName,
                startupScene = startup,
                scenes,
                width = windowWidth,
                height = windowHeight,
                fullscreen = (bool?)data.Attribute("fullscreen") ?? false,
                defaults = runtimeDefaults.ToString(SaveOptions.DisableFormatting)
            }), files.Keys.Where(path => !path.StartsWith("Scripts/", StringComparison.OrdinalIgnoreCase)));
            Run(Path.Combine(stage, "AtomGame.exe"), stage, "--inspect-package");
            File.WriteAllText(Path.Combine(stage, ".atom-publish"), "AtomEngine publisher v2\n");
            var report = Directory.EnumerateFiles(stage, "*", SearchOption.AllDirectories).Select(file => new { Path = Path.GetRelativePath(stage, file).Replace('\\', '/'), Bytes = new FileInfo(file).Length, SHA256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(file))) }).ToArray();
            File.WriteAllText(Path.Combine(stage, "build-manifest.json"), JsonSerializer.Serialize(report, new JsonSerializerOptions { WriteIndented = true }));
            string? zipStage = null;
            if (archive) { zipStage = stage + ".zip"; ZipFile.CreateFromDirectory(stage, zipStage, CompressionLevel.Optimal, false); }
            // 目录与 ZIP 一起提交；ZIP 被占用等提交失败也必须恢复上一份发行包。
            var old = output + ".previous-" + Guid.NewGuid().ToString("N");
            var oldZip = old + ".zip";
            bool backedUpDirectory = false, backedUpZip = false, installedDirectory = false, installedZip = false;
            try
            {
                if (Directory.Exists(output)) { Directory.Move(output, old); backedUpDirectory = true; }
                if (zipStage != null && File.Exists(output + ".zip")) { File.Move(output + ".zip", oldZip); backedUpZip = true; }
                Directory.Move(stage, output); installedDirectory = true;
                if (zipStage != null) { File.Move(zipStage, output + ".zip"); installedZip = true; }
            }
            catch
            {
                if (installedZip) File.Delete(output + ".zip");
                if (installedDirectory) Directory.Delete(output, true);
                if (backedUpDirectory) Directory.Move(old, output);
                if (backedUpZip) File.Move(oldZip, output + ".zip");
                throw;
            }
            if (backedUpDirectory) Directory.Delete(old, true);
            if (backedUpZip) File.Delete(oldZip);
            Console.WriteLine($"Published {files.Count} game assets to {output}" + (archive ? " and ZIP" : ""));
        }
        finally { if (Directory.Exists(stage)) Directory.Delete(stage, true); if (File.Exists(stage + ".zip")) File.Delete(stage + ".zip"); }
    }
}
