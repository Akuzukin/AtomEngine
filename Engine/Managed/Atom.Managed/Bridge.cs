using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Loader;
using System.Text;
using System.Text.Json;
using System.Globalization;
namespace Atom.Managed;

/// <summary>引擎内部托管桥，负责程序集加载、反射和脚本生命周期分派；游戏脚本使用 Atom 命名空间的接口。</summary>
public static unsafe class Bridge
{
    private sealed class ScriptLoadContext : AssemblyLoadContext
    {
        private readonly AssemblyDependencyResolver resolver;
        public ScriptLoadContext(string path) : base("GameScripts:" + path, isCollectible: true) { resolver = new(path); }
        protected override Assembly? Load(AssemblyName name)
        {
            if (name.Name == typeof(Behaviour).Assembly.GetName().Name) return typeof(Behaviour).Assembly;
            var path = resolver.ResolveAssemblyToPath(name);
            if (path == null) return null;
            using var stream = new MemoryStream(File.ReadAllBytes(path)); return LoadFromStream(stream);
        }
    }
    private sealed record LoadedAssembly(ScriptLoadContext Context, Assembly Assembly);
    private static readonly Dictionary<ulong, Behaviour> Instances = new();
    private static readonly HashSet<ulong> Enabled=new();
    private static readonly Dictionary<ulong,Dictionary<string,string>> Defaults=new();
    private static readonly Dictionary<ulong,HashSet<string>> Overrides=new();
    private static readonly Dictionary<ulong, string> InstancePaths = new();
    private static readonly Dictionary<string, LoadedAssembly> Assemblies = new(StringComparer.OrdinalIgnoreCase);
    private static LoadedAssembly LoadAssembly(string path)
    {
        var context = new ScriptLoadContext(path);
        try { using var stream = new MemoryStream(File.ReadAllBytes(path)); return new(context, context.LoadFromStream(stream)); }
        catch { context.Unload(); throw; }
    }
    private static void Reload(string path)
    {
        path = Path.GetFullPath(path);
        var next = LoadAssembly(path);
        var replacements = new Dictionary<ulong, Behaviour>();
        var states=new Dictionary<ulong,string?>();
        var nextDefaults=new Dictionary<ulong,Dictionary<string,string>>();
        try
        {
            foreach (var pair in Instances)
            {
                if (!string.Equals(InstancePaths[pair.Key], path, StringComparison.OrdinalIgnoreCase)) continue;
                var old = pair.Value;
                var type = next.Assembly.GetType(old.GetType().FullName!, true)!;
                if (!typeof(Behaviour).IsAssignableFrom(type) || type.IsAbstract) throw new InvalidOperationException("Invalid reloaded Behaviour");
                var fresh = (Behaviour)Activator.CreateInstance(type)!;
                fresh.Object = old.Object;
                nextDefaults.Add(pair.Key,FieldCodec.Fields(type).Where(f=>FieldCodec.Kind(f.FieldType)!="unsupported").ToDictionary(f=>f.Name,f=>FieldCodec.Format(f.GetValue(fresh))));
                foreach (var field in old.GetType().GetFields(BindingFlags.Public | BindingFlags.Instance))
                {
                    var target = type.GetField(field.Name);
                    if (target == null || target.IsInitOnly || field.IsInitOnly) continue;
                    var value = field.GetValue(old);
                    if (target.FieldType.IsEnum && value != null) value = Enum.Parse(target.FieldType, value.ToString()!);
                    if (value == null || target.FieldType.IsInstanceOfType(value)) target.SetValue(fresh, value);
                }
                var state=old.CaptureReloadState();
                if(state!=null)fresh.RestoreReloadState(state);
                states.Add(pair.Key,state);replacements.Add(pair.Key, fresh);
            }
        }
        catch { next.Context.Unload(); throw; }
        Assemblies.TryGetValue(path, out var previous);
        Assemblies[path] = next;
        var failures=new List<Exception>();
        foreach(var pair in replacements)try {if(Enabled.Contains(pair.Key))Instances[pair.Key].OnDisable();Instances[pair.Key].OnDestroy();}catch(Exception error){failures.Add(error);}
        foreach(var pair in replacements){Instances[pair.Key]=pair.Value;Defaults[pair.Key]=nextDefaults[pair.Key];}
        foreach(var pair in replacements)try {if(states[pair.Key]==null)pair.Value.OnCreate();else pair.Value.OnReload();if(Enabled.Contains(pair.Key))pair.Value.OnEnable();}catch(Exception error){failures.Add(error);}
        previous?.Context.Unload();
        if(failures.Count!=0)throw new AggregateException("Reload committed; lifecycle callback failed",failures);
    }
    /// <summary>接收原生引擎的 JSON 请求并调用对应脚本接口，返回零表示成功。</summary>
    /// <param name="request">以零结尾的 UTF-8 请求。</param>
    /// <param name="apiPointer">版本匹配的原生函数表。</param>
    /// <param name="context">当前世界的原生上下文。</param>
    [UnmanagedCallersOnly(CallConvs = new[] { typeof(System.Runtime.CompilerServices.CallConvCdecl) })]
    public static int Dispatch(byte* request, void* apiPointer, nint context)
    {
        var api = *(NativeApi*)apiPointer;
        try
        {
            if (api.Version != 7 || api.Size != sizeof(NativeApi)) throw new InvalidOperationException("Native script ABI mismatch");
            using var doc = JsonDocument.Parse(Marshal.PtrToStringUTF8((nint)request)!); var r = doc.RootElement;
            string op = r.GetProperty("op").GetString()!;
            if (r.TryGetProperty("time", out var time)) Time.Apply(time, op is "fixed" or "collision");
            if (op == "time") return 0;
            if (op is "inspect" or "validate") {
                var loaded=LoadAssembly(Path.GetFullPath(r.GetProperty("assembly").GetString()!));
                try {
                    if(op=="validate") {
                        var type=loaded.Assembly.GetType(r.GetProperty("type").GetString()!,true)!;
                        if(!typeof(Behaviour).IsAssignableFrom(type) || type.IsAbstract || type.ContainsGenericParameters || type.GetConstructor(Type.EmptyTypes)==null)
                            throw new ArgumentException("Class must be a concrete Behaviour with a public parameterless constructor");
                        _=FieldCodec.Prepare(type,r.GetProperty("fields"));
                    } else {
                        var schema=loaded.Assembly.GetTypes().Where(t=>typeof(Behaviour).IsAssignableFrom(t) && !t.IsAbstract &&
                            !t.ContainsGenericParameters && t.GetConstructor(Type.EmptyTypes)!=null).OrderBy(t=>t.FullName)
                            .Select(t=>new { name=t.FullName, fields=FieldCodec.Fields(t).Select(f=>new {name=f.Name,kind=FieldCodec.Kind(f.FieldType),
                                choices=f.FieldType.IsEnum?Enum.GetNames(f.FieldType):Array.Empty<string>()}) });
                        var bytes=Encoding.UTF8.GetBytes(JsonSerializer.Serialize(schema)+"\0");fixed(byte* p=bytes)api.Reply(context,p);
                    }
                } finally {loaded.Context.Unload();}
                return 0;
            }
            if (op == "reload") { Reload(r.GetProperty("assembly").GetString()!); return 0; }
            ulong id = r.GetProperty("id").GetUInt64();
            if (op == "create")
            {
                string path = Path.GetFullPath(r.GetProperty("assembly").GetString()!);
                if (!Assemblies.TryGetValue(path, out var assembly)) { assembly = LoadAssembly(path); Assemblies.Add(path, assembly); }
                else if(!InstancePaths.Values.Any(p=>string.Equals(p,path,StringComparison.OrdinalIgnoreCase))) {var next=LoadAssembly(path);Assemblies[path]=next;assembly.Context.Unload();assembly=next;}
                var type = assembly.Assembly.GetType(r.GetProperty("type").GetString()!, true)!;
                if (!typeof(Behaviour).IsAssignableFrom(type) || type.IsAbstract) throw new InvalidOperationException("Script must derive from Atom.Behaviour");
                var script = (Behaviour)Activator.CreateInstance(type)!; script.Object = new GameObject { Context = context, Entity = r.GetProperty("entity").GetUInt32(), Api = api, Generation = api.Epoch(context) };
                var initial=FieldCodec.Fields(type).Where(f=>FieldCodec.Kind(f.FieldType)!="unsupported").ToDictionary(f=>f.Name,f=>FieldCodec.Format(f.GetValue(script)));
                foreach(var item in FieldCodec.Prepare(type,r.GetProperty("fields"))) item.Field.SetValue(script,item.Value);
                script.OnCreate(); Instances.Add(id, script); InstancePaths.Add(id, path);Defaults.Add(id,initial);Overrides.Add(id,r.GetProperty("fields").EnumerateObject().Select(p=>p.Name).ToHashSet());
            }
            else if (Instances.TryGetValue(id, out var script))
            {
                if(op=="readFields") {
                    var values=FieldCodec.Fields(script.GetType()).Where(f=>FieldCodec.Kind(f.FieldType)!="unsupported").ToDictionary(f=>f.Name,f=>FieldCodec.Format(f.GetValue(script)));
                    var bytes=Encoding.UTF8.GetBytes(JsonSerializer.Serialize(values)+"\0");fixed(byte* p=bytes)api.Reply(context,p);return 0;
                }
                if(op=="patchFields") {
                    var fields=r.GetProperty("fields");var converted=FieldCodec.Prepare(script.GetType(),fields);var names=fields.EnumerateObject().Select(p=>p.Name).ToHashSet();
                    foreach(var name in Overrides[id].Except(names)) {var field=script.GetType().GetField(name);if(field!=null && Defaults[id].TryGetValue(name,out var value))converted.Add((field,FieldCodec.Parse(field.FieldType,value)));}
                    foreach(var value in converted)value.Field.SetValue(script,value.Value);Overrides[id]=names;return 0;
                }
                switch (op) { case "enable":Enabled.Add(id);script.OnEnable();break;case "disable":Enabled.Remove(id);script.OnDisable();break;case "update": script.OnUpdate(r.GetProperty("dt").GetSingle()); break; case "fixed": script.OnFixedUpdate(r.GetProperty("dt").GetSingle()); break; case "uiClick": script.OnUiClick(); break; case "collision": script.OnCollision(r.GetProperty("other").GetUInt32(), r.GetProperty("trigger").GetBoolean()); break; case "destroy": try { script.OnDestroy(); } finally { Instances.Remove(id); InstancePaths.Remove(id);Enabled.Remove(id);Defaults.Remove(id);Overrides.Remove(id); } break; }
            }
            return 0;
        }
        catch (Exception ex) { var bytes = Encoding.UTF8.GetBytes(ex.ToString() + "\0"); fixed (byte* p = bytes) api.Log(context, p); return -1; }
        finally { Time.EndCallback(); }
    }
}
