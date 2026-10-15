using System.Globalization;
using System.Numerics;
using System.Reflection;
using System.Text.Json;
namespace Atom.Managed;
internal static class FieldCodec
{
    internal static string Kind(Type type) => type==typeof(ParticleComponent)?"particleComponent":type==typeof(AudioClip)?"audioClip":type.IsEnum ? "enum" : type == typeof(bool) ? "bool" :
        type == typeof(string) ? "string" : type == typeof(float) ? "float" : type == typeof(double) ? "double" :
        type == typeof(int) ? "int" : type == typeof(uint) ? "uint" : type == typeof(long) ? "long" : type == typeof(ulong) ? "ulong" :
        type == typeof(Vector2) ? "vector2" : type == typeof(Vector3) ? "vector3" : type == typeof(Vector4) ? "vector4" :
        type == typeof(Quaternion) ? "quaternion" : "unsupported";
    internal static FieldInfo[] Fields(Type type) => type.GetFields(BindingFlags.Public | BindingFlags.Instance)
        .Where(f => !f.IsInitOnly && !f.IsLiteral).OrderBy(f => f.Name).ToArray();
    internal static object Parse(Type type, string value)
    {
        var kind=Kind(type);
        if (kind=="unsupported") throw new InvalidOperationException("Unsupported field type: " + type.Name);
        if(type==typeof(ParticleComponent))return new ParticleComponent(value.StartsWith("particle:")?value[9..]:value);
        if(type==typeof(AudioClip))return new AudioClip(value);
        if (type.IsEnum) {
            if (!Enum.GetNames(type).Contains(value)) throw new ArgumentException("Unknown enum value: " + value);
            return Enum.Parse(type,value);
        }
        if (kind is "vector2" or "vector3" or "vector4" or "quaternion") {
            var values=JsonSerializer.Deserialize<float[]>(value) ?? throw new ArgumentException("Expected a numeric array");
            int count=kind=="vector2"?2:kind=="vector3"?3:4;
            if(values.Length!=count || values.Any(v=>!float.IsFinite(v))) throw new ArgumentException("Invalid vector field");
            return kind switch { "vector2" => new Vector2(values[0],values[1]), "vector3" => new Vector3(values[0],values[1],values[2]),
                "vector4" => new Vector4(values[0],values[1],values[2],values[3]), _ => new Quaternion(values[0],values[1],values[2],values[3]) };
        }
        object result=Convert.ChangeType(value,type,CultureInfo.InvariantCulture);
        if(result is float f && !float.IsFinite(f) || result is double d && !double.IsFinite(d)) throw new ArgumentException("Field must be finite");
        return result;
    }
    internal static string Format(object? value) => value switch {
        ParticleComponent particle=>string.IsNullOrEmpty(particle.PersistentId)?"":"particle:"+particle.PersistentId,
        AudioClip clip=>clip.Path,
        Vector2 v => JsonSerializer.Serialize(new[]{v.X,v.Y}), Vector3 v=>JsonSerializer.Serialize(new[]{v.X,v.Y,v.Z}),
        Vector4 v=>JsonSerializer.Serialize(new[]{v.X,v.Y,v.Z,v.W}), Quaternion q=>JsonSerializer.Serialize(new[]{q.X,q.Y,q.Z,q.W}),
        bool v=>v?"true":"false", IFormattable v=>v.ToString(null,CultureInfo.InvariantCulture), _=>value?.ToString()??"" };
    internal static void BindReferences(Behaviour script){foreach(var field in Fields(script.GetType()))if(field.FieldType==typeof(ParticleComponent))field.SetValue(script,((ParticleComponent)field.GetValue(script)!).Bind(script.Object));}
    internal static List<(FieldInfo Field,object Value)> Prepare(Type type, JsonElement fields)
    {
        var result=new List<(FieldInfo,object)>();
        foreach(var p in fields.EnumerateObject()) {
            var field=type.GetField(p.Name,BindingFlags.Public|BindingFlags.Instance) ?? throw new ArgumentException("Unknown field: "+p.Name);
            if(field.IsInitOnly || field.IsLiteral) throw new ArgumentException("Readonly field: "+p.Name);
            try {result.Add((field,Parse(field.FieldType,p.Value.GetString()??"")));}
            catch(Exception error){throw new ArgumentException($"Invalid field {p.Name} ({Kind(field.FieldType)}): {error.Message}",error);}
        }
        return result;
    }
}
