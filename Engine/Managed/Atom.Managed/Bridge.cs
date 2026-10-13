using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Loader;
using System.Text;
using System.Text.Json;
using System.Globalization;
namespace Atom.Managed;

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
                foreach (var field in old.GetType().GetFields(BindingFlags.Public | BindingFlags.Instance))
                {
                    var target = type.GetField(field.Name);
                    if (target == null || target.IsInitOnly || field.IsInitOnly) continue;
                    var value = field.GetValue(old);
                    if (target.FieldType.IsEnum && value != null) value = Enum.Parse(target.FieldType, value.ToString()!);
                    if (value == null || target.FieldType.IsInstanceOfType(value)) target.SetValue(fresh, value);
                }
                replacements.Add(pair.Key, fresh);
            }
        }
        catch { next.Context.Unload(); throw; }
        Assemblies.TryGetValue(path, out var previous);
        Assemblies[path] = next;
        foreach (var pair in replacements)
        {
            try { Instances[pair.Key].OnDestroy(); } finally { Instances[pair.Key] = pair.Value; }
            pair.Value.OnCreate();
        }
        previous?.Context.Unload();
    }
    [UnmanagedCallersOnly(CallConvs = new[] { typeof(System.Runtime.CompilerServices.CallConvCdecl) })]
    public static int Dispatch(byte* request, void* apiPointer, nint context)
    {
        var api = *(NativeApi*)apiPointer;
        try
        {
            if (api.Version != 3 || api.Size != sizeof(NativeApi)) throw new InvalidOperationException("Native script ABI mismatch");
            using var doc = JsonDocument.Parse(Marshal.PtrToStringUTF8((nint)request)!); var r = doc.RootElement;
            string op = r.GetProperty("op").GetString()!;
            if (op == "reload") { Reload(r.GetProperty("assembly").GetString()!); return 0; }
            ulong id = r.GetProperty("id").GetUInt64();
            if (op == "create")
            {
                string path = Path.GetFullPath(r.GetProperty("assembly").GetString()!);
                if (!Assemblies.TryGetValue(path, out var assembly)) { assembly = LoadAssembly(path); Assemblies.Add(path, assembly); }
                var type = assembly.Assembly.GetType(r.GetProperty("type").GetString()!, true)!;
                if (!typeof(Behaviour).IsAssignableFrom(type) || type.IsAbstract) throw new InvalidOperationException("Script must derive from Atom.Behaviour");
                var script = (Behaviour)Activator.CreateInstance(type)!; script.Object = new GameObject { Context = context, Entity = r.GetProperty("entity").GetUInt32(), Api = api };
                foreach (var property in r.GetProperty("fields").EnumerateObject())
                {
                    var field = type.GetField(property.Name, BindingFlags.Public | BindingFlags.Instance) ?? throw new InvalidOperationException("Unknown script field: " + property.Name);
                    if (field.IsInitOnly) throw new InvalidOperationException("Script field is readonly: " + property.Name);
                    field.SetValue(script, field.FieldType.IsEnum ? Enum.Parse(field.FieldType, property.Value.GetString()!) : Convert.ChangeType(property.Value.GetString(), field.FieldType, CultureInfo.InvariantCulture));
                }
                script.OnCreate(); Instances.Add(id, script); InstancePaths.Add(id, path);
            }
            else if (Instances.TryGetValue(id, out var script))
            {
                switch (op) { case "update": script.OnUpdate(r.GetProperty("dt").GetSingle()); break; case "fixed": script.OnFixedUpdate(r.GetProperty("dt").GetSingle()); break; case "uiClick": script.OnUiClick(); break; case "collision": script.OnCollision(r.GetProperty("other").GetUInt32(), r.GetProperty("trigger").GetBoolean()); break; case "destroy": try { script.OnDestroy(); } finally { Instances.Remove(id); InstancePaths.Remove(id); } break; }
            }
            return 0;
        }
        catch (Exception ex) { var bytes = Encoding.UTF8.GetBytes(ex.ToString() + "\0"); fixed (byte* p = bytes) api.Log(context, p); return -1; }
    }
}
