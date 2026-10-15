using System.Security.Cryptography;
using System.Text;
using System.Text.Json;

// 资产索引和每项内容分别使用随机 nonce 加密；运行配置是包内资产，不携带编辑器工程。
static class AssetArchiveWriter
{
    public static void Write(string stage, string launchConfig, IEnumerable<string> assets)
    {
        byte[] key = RandomNumberGenerator.GetBytes(32);
        string archive = Path.Combine(stage, "game.atpak");
        string payload = archive + ".payload";
        var files = new List<object>();
        string[] paths = assets.Order(StringComparer.OrdinalIgnoreCase).ToArray();
        using var aes = new AesGcm(key, 16);
        try
        {
            using (var stream = File.Create(payload))
            {
                foreach (string path in paths)
                {
                    string file = Path.Combine(stage, path);
                    if (new FileInfo(file).Length > 512L * 1024 * 1024) throw new Exception("Asset exceeds 512 MiB: " + path);
                    Add(path, File.ReadAllBytes(file));
                }
                Add("__runtime/startup.json", Encoding.UTF8.GetBytes(launchConfig));
                void Add(string path, byte[] plain)
                {
                    if (plain.Length > 512 * 1024 * 1024) throw new Exception("Asset exceeds 512 MiB: " + path);
                    byte[] nonce = RandomNumberGenerator.GetBytes(12), tag = new byte[16], cipher = new byte[plain.Length];
                    aes.Encrypt(nonce, plain, cipher, tag, Encoding.UTF8.GetBytes(path));
                    files.Add(new { path, offset = stream.Position, size = plain.Length, nonce = Convert.ToHexString(nonce), tag = Convert.ToHexString(tag) });
                    stream.Write(cipher); CryptographicOperations.ZeroMemory(plain);
                }
            }
            byte[] index = JsonSerializer.SerializeToUtf8Bytes(new { version = 1, files });
            byte[] indexNonce = RandomNumberGenerator.GetBytes(12), indexTag = new byte[16], indexCipher = new byte[index.Length];
            aes.Encrypt(indexNonce, index, indexCipher, indexTag, "ATPAK001"u8);
            using (var stream = File.Create(archive))
            using (var writer = new BinaryWriter(stream))
            {
                writer.Write("ATPAK001"u8); writer.Write(indexCipher.Length); writer.Write(indexNonce); writer.Write(indexTag); writer.Write(indexCipher);
                using var contents = File.OpenRead(payload); contents.CopyTo(stream);
            }
            using (var contents = File.OpenRead(archive))
            using (var executable = new FileStream(Path.Combine(stage, "AtomGame.exe"), FileMode.Append))
            {
                executable.Write("ATRUN001"u8); executable.Write(key); executable.Write(SHA256.HashData(contents));
            }
            // 临时发行目录由发布工具创建；明文资产不会进入最终目录或 ZIP。
            foreach (string path in paths)
            {
                string file = Path.Combine(stage, path); File.Delete(file);
                string? directory = Path.GetDirectoryName(file);
                while (directory != null && !directory.Equals(stage, StringComparison.OrdinalIgnoreCase) && !Directory.EnumerateFileSystemEntries(directory).Any())
                { Directory.Delete(directory); directory = Path.GetDirectoryName(directory); }
            }
        }
        finally { CryptographicOperations.ZeroMemory(key); if (File.Exists(payload)) File.Delete(payload); }
    }
}
