using AlfaRaceX.Updater;
using System.Text.Json;

int failed = 0;
string root = Path.Combine(Path.GetTempPath(), "arx-tests-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(root);
string Record(byte type, ushort address, params byte[] data)
{
    byte[] bytes = [(byte)data.Length, (byte)(address >> 8), (byte)address, type, .. data];
    return ":" + Convert.ToHexString(bytes) + ((byte)(-bytes.Sum(b => b))).ToString("X2");
}
string upper = Record(4, 0, 8, 0);
string payload = Record(0, 0, 0, 64, 0, 32, 9, 0, 0, 8, 0, 191);
string eof = Record(1, 0);
IntelHexImage Load(params string[] lines)
{
    string path = Path.Combine(root, Guid.NewGuid() + ".hex");
    File.WriteAllLines(path, lines);
    return IntelHexImage.Load(path);
}
void Test(string name, Action test)
{
    try { test(); Console.WriteLine("PASS " + name); }
    catch (Exception e) { failed++; Console.WriteLine("FAIL " + name + ": " + e.Message); }
}
void Reject(Action action)
{
    try { action(); }
    catch (InvalidDataException) { return; }
    throw new Exception("Invalid input was accepted");
}
try
{
    Test("valid HEX range and segments", () => {
        var image = Load(upper, payload, eof);
        image.ValidateApplicationRange(0x08000000, 0x0800F000);
        if (image.Bytes.Count != 10 || image.Segments().Single().Data.Length != 10) throw new Exception("Data changed");
    });
    Test("missing EOF", () => Reject(() => Load(upper, payload)));
    Test("empty EOF-only image", () => Reject(() => Load(eof)));
    Test("trailing data after EOF", () => Reject(() => Load(upper, payload, eof, payload)));
    Test("duplicate address", () => Reject(() => Load(upper, payload, payload, eof)));
    Test("malformed EOF", () => Reject(() => Load(upper, payload, Record(1, 1, 1))));
    Test("invalid start record", () => Reject(() => Load(upper, payload, Record(5, 0, 1), eof)));
    Test("address overflow", () => Reject(() => Load(Record(4, 0, 255, 255), Record(0, 65535, 1, 2), eof)));
    Test("bad checksum", () => Reject(() => Load(upper, payload[..^2] + "FF", eof)));
    Test("outside application", () => Reject(() => Load(upper, payload, eof).ValidateApplicationRange(0x08000800, 0x0800F000)));
    UpdateManifest Manifest() => new() { Product = "AlfaRaceX", Version = "1.0.0-rc5",
        Targets = new[] { "BH", "C2", "C1" }.Select(role => new FirmwareTarget {
            Id = role, Sha256 = new string('a', 64), ApplicationStart = 0x08000000,
            ApplicationLimitExclusive = role == "C1" ? 0x08018000u : 0x0800F000u,
            Url = $"https://github.com/AriotaG/AlfaRaceX-Framework/releases/download/1.0.0-rc5/AlfaRaceX-{role}.hex"
        }).ToList() };
    Test("valid manifest", () => ManifestClient.ValidateManifest(Manifest()));
    Test("duplicate MCU", () => Reject(() => { var m = Manifest(); m.Targets[1] = m.Targets[0]; ManifestClient.ValidateManifest(m); }));
    Test("reserved flash area", () => Reject(() => { var m = Manifest(); m.Targets[0].ApplicationLimitExclusive = 0x08020000; ManifestClient.ValidateManifest(m); }));
    Test("manifest path traversal", () => Reject(() => { var m = Manifest(); m.Version = "../../outside"; ManifestClient.ValidateManifest(m); }));
    Test("wrong role asset URL", () => Reject(() => { var m = Manifest(); m.Targets[0].Url = m.Targets[1].Url; ManifestClient.ValidateManifest(m); }));
    Test("untrusted download URL", () => Reject(() => { var m = Manifest(); m.Targets[0].Url = "http://example.com/file.hex"; ManifestClient.ValidateManifest(m); }));
    Test("invalid manifest hash", () => Reject(() => { var m = Manifest(); m.Targets[0].Sha256 = "bad"; ManifestClient.ValidateManifest(m); }));
    Test("null targets", () => Reject(() => { var m = Manifest(); m.Targets = null!; ManifestClient.ValidateManifest(m); }));
    void DownloadCase(byte[] bytes, bool validHash, bool reject)
    {
        string dir = Path.Combine(root, Guid.NewGuid().ToString("N"));
        var target = Manifest().Targets[0];
        target.Sha256 = validHash ? Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(bytes)) : new string('0', 64);
        var client = new ManifestClient(new TestResponseHandler(bytes));
        void Run() => client.DownloadVerifiedAsync(target, dir, null, CancellationToken.None).GetAwaiter().GetResult();
        if (reject)
        {
            Reject(Run);
            if (Directory.EnumerateFiles(dir).Any()) throw new Exception("Partial download left behind");
        }
        else
        {
            Run();
            if (!File.ReadAllBytes(Directory.EnumerateFiles(dir).Single()).SequenceEqual(bytes)) throw new Exception("Download bytes changed");
        }
    }
    Test("verified download preserves bytes", () => DownloadCase([1, 2, 3, 4], true, false));
    Test("download wrong hash leaves no partial file", () => DownloadCase([1, 2, 3, 4], false, true));
    Test("oversized download rejected", () => DownloadCase(new byte[1024 * 1024 + 1], true, true));
    AlfaRaceX.Desktop.DesktopPaths.TestRoot = Path.Combine(root, "desktop");
    AlfaRaceX.Desktop.DesktopPaths.Ensure();
    var history = new AlfaRaceX.Desktop.HistoryRepository(AlfaRaceX.Desktop.DesktopPaths.Database);
    history.Initialize();
    Test("SQLite setting survives reopen", () => {
        history.SetSetting("test'key", "persisted' 123");
        var reopened = new AlfaRaceX.Desktop.HistoryRepository(AlfaRaceX.Desktop.DesktopPaths.Database);
        if (reopened.GetSetting("test'key") != "persisted' 123") throw new Exception("Setting lost");
    });
    Test("interrupted operation recovered once", () => {
        history.BeginOperation("FLASH");
        var reopened = new AlfaRaceX.Desktop.HistoryRepository(AlfaRaceX.Desktop.DesktopPaths.Database);
        if (reopened.RecoverInterruptedOperations() != 1 || reopened.RecoverInterruptedOperations() != 0 || reopened.CountInterruptedOperations() != 1)
            throw new Exception("Recovery state incorrect");
    });
    Test("completed failed cancelled operations not recovered", () => {
        foreach (string status in new[] { "Completed", "Failed", "Cancelled" })
            history.FinishOperation(history.BeginOperation("BACKUP"), status);
        if (history.RecoverInterruptedOperations() != 0) throw new Exception("Finished operation recovered");
    });
    Test("clearing logs preserves operation journal", () => {
        history.AddEvent("INFO", "TEST", "CI fixture"); history.ClearEvents();
        if (history.GetEvents().Count != 0 || history.CountInterruptedOperations() != 1) throw new Exception("Journal erased with logs");
    });
    string backupPath = Path.Combine(root, "AlfaRaceX-C1.bin");
    string hash = new string('a', 64);
    BackupMetadata Meta() => new() { Role = "C1", FlashStart = 0x08000000, FlashSize = 131072,
        Sha256 = hash, FileName = Path.GetFileName(backupPath), CreatedUtc = DateTime.UtcNow };
    void Metadata(BackupMetadata m) => File.WriteAllText(Path.ChangeExtension(backupPath, ".json"), JsonSerializer.Serialize(m));
    Test("missing backup metadata", () => Reject(() => BackupRestoreService.VerifyRequiredMetadata("C1", backupPath, hash)));
    Test("valid backup metadata", () => { Metadata(Meta()); BackupRestoreService.VerifyRequiredMetadata("C1", backupPath, hash); });
    Test("backup other MCU", () => Reject(() => BackupRestoreService.VerifyRequiredMetadata("BH", backupPath, hash)));
    Test("corrupt backup hash", () => Reject(() => BackupRestoreService.VerifyRequiredMetadata("C1", backupPath, new string('b', 64))));
    Test("backup wrong memory map", () => Reject(() => { var m = Meta(); m.FlashSize = 65536; Metadata(m); BackupRestoreService.VerifyRequiredMetadata("C1", backupPath, hash); }));
    Test("backup wrong file", () => Reject(() => { var m = Meta(); m.FileName = "other.bin"; Metadata(m); BackupRestoreService.VerifyRequiredMetadata("C1", backupPath, hash); }));
}
finally { Microsoft.Data.Sqlite.SqliteConnection.ClearAllPools(); Directory.Delete(root, true); }
Console.WriteLine($"Failed: {failed}");
return failed == 0 ? 0 : 1;

// In-memory HTTP fixture: exercises download validation without a network or device.
sealed class TestResponseHandler(byte[] bytes) : HttpMessageHandler
{
    protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken cancellationToken) =>
        Task.FromResult(new HttpResponseMessage(System.Net.HttpStatusCode.OK) { Content = new ByteArrayContent(bytes) });
}
