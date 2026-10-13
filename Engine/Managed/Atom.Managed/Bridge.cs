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
        public ScriptLoadContext(string path) : base("GameScripts:" + path) { resolver = new(path); }
        protected override Assembly? Load(AssemblyName name)
        {
            if (name.Name == typeof(Behaviour).Assembly.GetName().Name) return typeof(Behaviour).Assembly;
            var path = resolver.ResolveAssemblyToPath(name); return path == null ? null : LoadFromAssemblyPath(path);
        }
    }
    private static readonly Dictionary<ulong, Behaviour> Instances = new();
    private static readonly Dictionary<string, Assembly> Assemblies = new(StringComparer.OrdinalIgnoreCase);
    [UnmanagedCallersOnly(CallConvs = new[] { typeof(System.Runtime.CompilerServices.CallConvCdecl) })]
    public static int Dispatch(byte* request, void* apiPointer, nint context)
    {
        var api = *(NativeApi*)apiPointer;
        try
        {
            if (api.Version != 3 || api.Size != sizeof(NativeApi)) throw new InvalidOperationException("Native script ABI mismatch");
            using var doc = JsonDocument.Parse(Marshal.PtrToStringUTF8((nint)request)!); var r = doc.RootElement;
            ulong id = r.GetProperty("id").GetUInt64(); string op = r.GetProperty("op").GetString()!;
            if (op == "create")
            {
                string path = Path.GetFullPath(r.GetProperty("assembly").GetString()!);
                if (!Assemblies.TryGetValue(path, out var assembly)) { assembly = new ScriptLoadContext(path).LoadFromAssemblyPath(path); Assemblies.Add(path, assembly); }
                var type = assembly.GetType(r.GetProperty("type").GetString()!, true)!;
                if (!typeof(Behaviour).IsAssignableFrom(type) || type.IsAbstract) throw new InvalidOperationException("Script must derive from Atom.Behaviour");
                var script = (Behaviour)Activator.CreateInstance(type)!; script.Object = new GameObject { Context = context, Entity = r.GetProperty("entity").GetUInt32(), Api = api };
                foreach (var property in r.GetProperty("fields").EnumerateObject())
                {
                    var field = type.GetField(property.Name, BindingFlags.Public | BindingFlags.Instance) ?? throw new InvalidOperationException("Unknown script field: " + property.Name);
                    if (field.IsInitOnly) throw new InvalidOperationException("Script field is readonly: " + property.Name);
                    field.SetValue(script, field.FieldType.IsEnum ? Enum.Parse(field.FieldType, property.Value.GetString()!) : Convert.ChangeType(property.Value.GetString(), field.FieldType, CultureInfo.InvariantCulture));
                }
                script.OnCreate(); Instances.Add(id, script);
            }
            else if (Instances.TryGetValue(id, out var script))
            {
                switch (op) { case "update": script.OnUpdate(r.GetProperty("dt").GetSingle()); break; case "fixed": script.OnFixedUpdate(r.GetProperty("dt").GetSingle()); break; case "uiClick": script.OnUiClick(); break; case "collision": script.OnCollision(r.GetProperty("other").GetUInt32(), r.GetProperty("trigger").GetBoolean()); break; case "destroy": try { script.OnDestroy(); } finally { Instances.Remove(id); } break; }
            }
            return 0;
        }
        catch (Exception ex) { var bytes = Encoding.UTF8.GetBytes(ex.ToString() + "\0"); fixed (byte* p = bytes) api.Log(context, p); return -1; }
    }
}
