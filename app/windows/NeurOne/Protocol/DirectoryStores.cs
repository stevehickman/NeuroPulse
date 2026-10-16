// DirectoryStores.cs — a folder on disk as the app's two storage roles (SWR-MG-06, SWR-MG-10, issue #569 package 6k):
// DirectoryObjectStore is the route for a local computer, a USB drive or a shared folder, and DirectoryFileStore is the app's
// own files for the old phone's clear. They use the same file names and raw bytes as Kotlin's (DirectoryStores.kt) and Swift's
// (DirectoryStores.swift), so a folder one platform wrote another can read. Object bytes cross the IObjectStore role as hex and
// are raw on disk.

namespace NeurOne.Protocol;

/// An IObjectStore over one folder: each object is a file named by the object. A claim is created with FileMode.CreateNew, which
/// the operating system arbitrates (exactly one of two simultaneous creates succeeds, SWR-MG-07); the content is written under
/// an exclusive share, and a create that fails part-way removes its file, so a half-written claim is not left behind. A folder on
/// a network share is only as atomic as that share's exclusive create.
sealed class DirectoryObjectStore : IObjectStore
{
    public const string TempPrefix = ".tmp-";
    private readonly string _root;

    public DirectoryObjectStore(string root)
    {
        _root = Path.GetFullPath(root);
        Directory.CreateDirectory(_root);
    }

    /// A name is one path segment of ASCII letters, digits, dot, dash and underscore, and does not start with a dot.
    public static bool IsSafeName(string name) =>
        name.Length is > 0 and <= 255 && !name.StartsWith('.') && name.All(c => c is (>= 'a' and <= 'z') or (>= 'A' and <= 'Z') or (>= '0' and <= '9') or '.' or '_' or '-');

    private string PathOf(string name) =>
        IsSafeName(name) ? Path.Combine(_root, name) : throw new ArgumentException($"unsafe object name '{name}'");

    public async Task<string?> GetAsync(string name)
    {
        var path = PathOf(name);
        return File.Exists(path) ? Convert.ToHexString(await File.ReadAllBytesAsync(path)).ToLowerInvariant() : null;
    }

    public async Task PutAsync(string name, string data)
    {
        var path = PathOf(name);
        var temp = Path.Combine(_root, TempPrefix + Guid.NewGuid().ToString("N"));
        try
        {
            await File.WriteAllBytesAsync(temp, Convert.FromHexString(data));
            File.Move(temp, path, overwrite: true);
        }
        finally
        {
            if (File.Exists(temp)) File.Delete(temp);
        }
    }

    public async Task<bool> CreateIfAbsentAsync(string name, string data)
    {
        var path = PathOf(name);
        var bytes = Convert.FromHexString(data);
        FileStream stream;
        try
        {
            stream = new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.None);
        }
        catch (IOException) when (File.Exists(path))
        {
            return false;
        }
        try
        {
            await using (stream)
            {
                await stream.WriteAsync(bytes);
                await stream.FlushAsync();
            }
            return true;
        }
        catch
        {
            File.Delete(path);
            throw;
        }
    }

    public Task DeleteAsync(string name)
    {
        var path = PathOf(name);
        if (File.Exists(path)) File.Delete(path);
        return Task.CompletedTask;
    }

    public Task<IReadOnlyList<string>> ListAsync(string prefix) =>
        Task.FromResult<IReadOnlyList<string>>(
            Directory.EnumerateFiles(_root).Select(Path.GetFileName).OfType<string>()
                .Where(n => n.StartsWith(prefix, StringComparison.Ordinal) && !n.StartsWith(TempPrefix, StringComparison.Ordinal))
                .OrderBy(n => n, StringComparer.Ordinal).ToList());

    public Task<long?> FreeSpaceAsync() => Task.FromResult<long?>(new DriveInfo(Path.GetPathRoot(_root)!).AvailableFreeSpace);
}

/// An IFileStore over one folder tree: ListAsync reports every regular file under it with its path relative to the folder
/// (forward slashes), and no path may leave the folder. WriteAsync overwrites in place and SyncAsync forces the file to storage.
sealed class DirectoryFileStore : IFileStore
{
    private readonly string _root;

    public DirectoryFileStore(string root) => _root = Path.GetFullPath(root);

    private string Resolve(string path)
    {
        var full = Path.GetFullPath(Path.Combine(_root, path));
        var prefix = _root.TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
        return full.StartsWith(prefix, StringComparison.Ordinal) ? full : throw new ArgumentException($"path leaves the folder: '{path}'");
    }

    public Task<IReadOnlyList<StoredFile>> ListAsync()
    {
        if (!Directory.Exists(_root)) return Task.FromResult<IReadOnlyList<StoredFile>>([]);
        return Task.FromResult<IReadOnlyList<StoredFile>>(
            Directory.EnumerateFiles(_root, "*", SearchOption.AllDirectories)
                .Select(f => new StoredFile(Path.GetRelativePath(_root, f).Replace('\\', '/'), new FileInfo(f).Length))
                .OrderBy(f => f.Path, StringComparer.Ordinal).ToList());
    }

    public async Task WriteAsync(string path, long offset, byte[] bytes)
    {
        await using var stream = new FileStream(Resolve(path), FileMode.Open, FileAccess.Write, FileShare.None);
        stream.Seek(offset, SeekOrigin.Begin);
        await stream.WriteAsync(bytes);
    }

    public Task SyncAsync(string path)
    {
        using var stream = new FileStream(Resolve(path), FileMode.Open, FileAccess.Write, FileShare.ReadWrite);
        stream.Flush(flushToDisk: true);
        return Task.CompletedTask;
    }

    public Task DeleteAsync(string path)
    {
        var full = Resolve(path);
        if (File.Exists(full)) File.Delete(full);
        return Task.CompletedTask;
    }
}
