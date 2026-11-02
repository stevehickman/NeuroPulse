// NPProtocolLibrary.cs — the Windows protocol library's mutation guards (SWR-PL-04, SWR-PL-02, SWR-FM-03, SWR-FM-05).
// What a save, a delete or a clone may do is decided once, in the shared core (NppsCore.Library); this class holds the
// entries and applies the core's answer. A predefined (bundled) protocol is never saved over or deleted: the user clones
// it first. The Windows protocol model carries no family id yet, so every protocol is a family of one (the core keys an
// entry with no family id by its own id).

using System.Text.Json.Nodes;

namespace NeurOne.Protocol;

enum SaveResult { Saved, ReadOnly, RefusedFamilyUsed }

enum DeleteResult { Deleted, ReadOnly, NotFound, RefusedFamilyUsed }

sealed class NPProtocolLibrary
{
    private readonly List<NPProtocolDefinition> _bundled;
    private readonly List<NPProtocolDefinition> _user = [];
    private readonly HashSet<Guid> _used = [];

    public NPProtocolLibrary(IEnumerable<NPProtocolDefinition> bundled) => _bundled = bundled.ToList();

    public IReadOnlyList<NPProtocolDefinition> All => [.. _bundled, .. _user];

    public bool CanEdit(NPProtocolDefinition p) => !IsReadOnly(p);
    public bool CanDelete(NPProtocolDefinition p) => !IsReadOnly(p);

    public bool IsUsed(Guid id) => _used.Contains(id);
    public void MarkUsed(Guid id) => _used.Add(id);

    private bool IsReadOnly(NPProtocolDefinition p) =>
        p.IsPredefined || p.IsReadOnly || _bundled.Any(b => b.Id == p.Id);

    private JsonArray Summaries() => new(All.Select(e => (JsonNode)new JsonObject
    {
        ["id"] = e.Id.ToString(),
        ["familyId"] = null,
        ["used"] = _used.Contains(e.Id),
    }).ToArray());

    /// Save over an entry (SWR-PL-04: never a predefined one; SWR-PL-02: never one of a used family, the refusal offers a clone).
    public SaveResult Save(NPProtocolDefinition entry)
    {
        if (IsReadOnly(entry)) return SaveResult.ReadOnly;
        var idx = _user.FindIndex(u => u.Id == entry.Id);
        if (idx >= 0)
        {
            var check = NppsCore.Library(new JsonObject { ["op"] = "saveCheck", ["id"] = entry.Id.ToString(), ["entries"] = Summaries() });
            if (!(bool)check["allowed"]!) return SaveResult.RefusedFamilyUsed;
            _user[idx] = entry;
        }
        else
        {
            _user.Add(entry);
        }
        return SaveResult.Saved;
    }

    public DeleteResult Delete(Guid id)
    {
        if (All.Any(e => e.Id == id && IsReadOnly(e))) return DeleteResult.ReadOnly;
        var idx = _user.FindIndex(u => u.Id == id);
        if (idx < 0) return DeleteResult.NotFound;
        var check = NppsCore.Library(new JsonObject { ["op"] = "deleteCheck", ["id"] = id.ToString(), ["entries"] = Summaries() });
        if (!(bool)check["allowed"]!) return DeleteResult.RefusedFamilyUsed;
        _user.RemoveAt(idx);
        return DeleteResult.Deleted;
    }

    /// An editable, unused copy of [id]'s family (SWR-FM-03). The copy is the user's: not predefined, not read-only.
    public IReadOnlyList<NPProtocolDefinition> CloneFamily(Guid id)
    {
        var source = All.FirstOrDefault(e => e.Id == id);
        if (source is null) return [];
        var newId = Guid.NewGuid();
        // The core checks that the family and every new id are new before anything is made.
        NppsCore.Library(new JsonObject
        {
            ["op"] = "cloneFamily", ["id"] = id.ToString(), ["entries"] = Summaries(),
            ["newFamilyId"] = Guid.NewGuid().ToString(), ["newIds"] = new JsonArray(newId.ToString()),
        });
        var taken = All.Select(e => e.Name).ToHashSet();
        var name = source.Name + " (copy)";
        for (var n = 2; taken.Contains(name); n++) name = $"{source.Name} (copy {n})";
        var copy = new NPProtocolDefinition
        {
            Id = newId, Name = name, Description = source.Description, Author = source.Author, Version = source.Version,
            Tags = source.Tags, CreatedAt = DateTime.UtcNow, ModifiedAt = DateTime.UtcNow,
            IsPredefined = false, IsReadOnly = false, Timing = source.Timing, TimingValue = source.TimingValue,
            Modalities = source.Modalities,
        };
        _user.Add(copy);
        return [copy];
    }
}
