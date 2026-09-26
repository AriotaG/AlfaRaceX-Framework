using AlfaRaceX.Updater;
using System.Text.Json;

internal static class BackupImportTests
{
    public static void Run(Action<string, Action> test, string root)
    {
        string source = Path.Combine(root, "import-sources");
        Directory.CreateDirectory(source);
        byte[] bytes = Enumerable.Repeat((byte)0x5A, BackupRestoreService.FlashSize).ToArray();
        string Raw(string name) { string path = Path.Combine(source, name); File.WriteAllBytes(path, bytes); return path; }
        void Check(bool value) { if (!value) throw new Exception("Backup import integrity failure"); }
        void Reject(string path, string caseName)
        {
            string destination = Path.Combine(root, "rejected-import-" + caseName);
            try { BackupRestoreService.ImportSnapshot("BH", path, destination); }
            catch (InvalidDataException) { Check(!Directory.Exists(destination)); return; }
            throw new Exception("Invalid import accepted");
        }
        test("import survives source removal and catalog reopen", () => {
            string path = Raw("original.bin");
            var saved = BackupRestoreService.ImportSnapshot("bh", path, Path.Combine(root, "managed-import"));
            var history = new AlfaRaceX.Desktop.HistoryRepository(AlfaRaceX.Desktop.DesktopPaths.Database);
            history.AddBackup(saved.Role, saved.BinPath, saved.MetadataPath, saved.Sha256, saved.Size, DateTime.UtcNow, "imported");
            Check(File.ReadAllBytes(path).SequenceEqual(bytes));
            File.Delete(path); // Test fixture only: simulates removed source media.
            var reopened = new AlfaRaceX.Desktop.HistoryRepository(AlfaRaceX.Desktop.DesktopPaths.Database);
            var record = reopened.GetBackups().Single(b => b.BinPath == saved.BinPath);
            var meta = JsonSerializer.Deserialize<BackupMetadata>(File.ReadAllText(record.MetadataPath))!;
            Check(record.Source == "imported" && File.ReadAllBytes(record.BinPath).SequenceEqual(bytes)
                && meta.Sha256 == record.Sha256 && meta.Role == "BH" && meta.FileName == Path.GetFileName(record.BinPath));
        });
        test("repeated imports preserve independent verified copies", () => {
            string path = Raw("repeat.bin"); string dest = Path.Combine(root, "repeat-import");
            var a = BackupRestoreService.ImportSnapshot("BH", path, dest);
            var b = BackupRestoreService.ImportSnapshot("BH", path, dest);
            Check(a.BinPath != b.BinPath && File.ReadAllBytes(a.BinPath).SequenceEqual(bytes) && File.ReadAllBytes(b.BinPath).SequenceEqual(bytes));
        });
        test("import rejects conflicting filename role before copying", () => Reject(Raw("original-C1.bin"), "filename"));
        test("import rejects conflicting metadata role before copying", () => {
            var backup = BackupRestoreService.SaveSnapshot("C1", source, bytes);
            string path = Raw("role.bin"); File.Copy(backup.MetadataPath, Path.ChangeExtension(path, ".json"));
            Reject(path, "role");
        });
        test("import rejects changed bytes against original metadata", () => {
            var backup = BackupRestoreService.SaveSnapshot("BH", source, bytes);
            byte[] changed = bytes.ToArray(); changed[0] ^= 1; File.WriteAllBytes(backup.BinPath, changed);
            Reject(backup.BinPath, "hash");
        });
        test("import rejects malformed metadata instead of replacing evidence", () => {
            string path = Raw("malformed.bin"); File.WriteAllText(Path.ChangeExtension(path, ".json"), "{bad");
            Reject(path, "json");
            Check(File.ReadAllText(Path.ChangeExtension(path, ".json")) == "{bad");
        });
        test("import rejects invalid flash map", () => {
            var backup = BackupRestoreService.SaveSnapshot("BH", source, bytes);
            var meta = JsonSerializer.Deserialize<BackupMetadata>(File.ReadAllText(backup.MetadataPath))!;
            meta.FlashStart++; File.WriteAllText(backup.MetadataPath, JsonSerializer.Serialize(meta));
            Reject(backup.BinPath, "map");
        });
        test("import rejects wrong size before allocating or saving snapshot", () => {
            string path = Path.Combine(source, "large.bin");
            using (var file = File.Create(path)) file.SetLength(BackupRestoreService.FlashSize + 1);
            Reject(path, "size");
        });
    }
}
