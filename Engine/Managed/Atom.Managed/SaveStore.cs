using System.Text.Json;
namespace Atom;
/// <summary>按游戏命名空间隔离的通用JSON存档服务，支持任意可序列化类型与独立槽位。</summary>
public sealed class SaveStore
{
    private readonly string directory;
    /// <summary>创建游戏专属存档服务，数据保存在用户LocalApplicationData。</summary>
    /// <param name="gameId">稳定的游戏标识，只允许字母、数字、下划线和连字符。</param>
    public SaveStore(string gameId){Validate(gameId);directory=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"AtomEngine","Saves",gameId);}
    private static void Validate(string name){if(string.IsNullOrWhiteSpace(name)||name.Length>128||name.Any(c=>!char.IsLetterOrDigit(c)&&c!='_'&&c!='-'))throw new ArgumentException("Invalid save identifier");}
    private string PathFor(string slot){Validate(slot);return Path.Combine(directory,slot+".json");}
    /// <summary>检查槽位是否存在。</summary>
    /// <param name="slot">槽位标识。</param>
    public bool Exists(string slot)=>File.Exists(PathFor(slot));
    /// <summary>读取指定类型；不存在返回默认值，损坏或类型不匹配抛出异常并保留原文件。</summary>
    /// <typeparam name="T">游戏定义的可序列化存档类型。</typeparam>
    /// <param name="slot">槽位标识。</param>
    public T? Load<T>(string slot){string path=PathFor(slot);return File.Exists(path)?JsonSerializer.Deserialize<T>(File.ReadAllText(path),Options):default;}
    private static readonly JsonSerializerOptions Options=new(){IncludeFields=true,WriteIndented=true};
    /// <summary>先序列化，再以同目录临时文件原子替换槽位；失败保留旧文件。</summary>
    /// <typeparam name="T">游戏定义的可序列化存档类型。</typeparam>
    /// <param name="slot">槽位标识。</param>
    /// <param name="data">需要保存的数据，版本与迁移策略由游戏定义。</param>
    public void Save<T>(string slot,T data){string path=PathFor(slot),json=JsonSerializer.Serialize(data,Options);Directory.CreateDirectory(directory);string temp=path+"."+Guid.NewGuid().ToString("N")+".tmp";try{File.WriteAllText(temp,json);if(File.Exists(path))File.Copy(path,path+".bak",true);File.Move(temp,path,true);}finally{if(File.Exists(temp))File.Delete(temp);}}
    /// <summary>删除单个槽位，不存在时无需操作。</summary>
    /// <param name="slot">槽位标识。</param>
    public void Delete(string slot)=>File.Delete(PathFor(slot));
}
