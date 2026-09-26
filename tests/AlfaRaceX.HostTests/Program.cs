using AlfaRaceX.Updater;

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
    Test("invalid hex characters", () => Reject(() => Load(upper, ":GG", eof)));
    Test("odd hex length", () => Reject(() => Load(upper, ":0", eof)));
    Test("unknown record type", () => Reject(() => Load(upper, payload, Record(6, 0), eof)));
    Test("invalid extended address offset", () => Reject(() => Load(Record(4, 1, 8, 0), payload, eof)));
    Test("exclusive upper limit", () => Reject(() => Load(upper, Record(0, 0xF000, 1), eof).ValidateApplicationRange(0x08000000, 0x0800F000)));
    Test("segmented address and sparse image preserved", () => {
        var image = Load(Record(2, 0, 0x12, 0x34), Record(0, 5, 1, 2), Record(0, 9, 3), eof);
        var segments = image.Segments().ToArray();
        if (segments.Length != 2 || segments[0].Address != 0x12345 ||
            !segments[0].Data.SequenceEqual(new byte[] {1, 2}) || segments[1].Address != 0x12349 ||
            segments[1].Data.Single() != 3) throw new Exception("Sparse data changed");
    });
    Test("highest address without overflow", () => {
        var image = Load(Record(4, 0, 255, 255), Record(0, 65535, 7), eof);
        if (image.MaxAddress != uint.MaxValue || image.Bytes[uint.MaxValue] != 7) throw new Exception("Boundary changed");
    });
    Test("page coverage and zero page size", () => {
        var image = Load(upper, Record(0, 0x7FF, 1, 2), eof);
        if (!image.TouchedPages(2048).SequenceEqual(new uint[] {0x08000000, 0x08000800})) throw new Exception("Page coverage changed");
        try { image.TouchedPages(0); }
        catch (ArgumentOutOfRangeException) { return; }
        throw new Exception("Zero page size accepted");
    });
    Test("start records and blank lines accepted", () => {
        var image = Load("", upper, payload, Record(3, 0, 0, 0, 0, 0), Record(5, 0, 8, 0, 0, 1), eof, " ");
        if (image.Bytes.Count != 10) throw new Exception("Start record changed data");
    });
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
    Test("null target entry", () => Reject(() => { var m = Manifest(); m.Targets[0] = null!; ManifestClient.ValidateManifest(m); }));
    Test("foreign product", () => Reject(() => { var m = Manifest(); m.Product = "Other"; ManifestClient.ValidateManifest(m); }));
    Test("wrong application start", () => Reject(() => { var m = Manifest(); m.Targets[0].ApplicationStart++; ManifestClient.ValidateManifest(m); }));
    Test("repository manifest remains compatible", () => {
        var m = System.Text.Json.JsonSerializer.Deserialize<UpdateManifest>(File.ReadAllText(
            Path.Combine(AppContext.BaseDirectory, "release-candidate.json")))!;
        ManifestClient.ValidateManifest(m);
    });
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
    Test("repeated download preserves earlier file", () => {
        byte[] bytes = [1, 2, 3, 4];
        var target = Manifest().Targets[0];
        target.Sha256 = Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(bytes));
        var client = new ManifestClient(new TestResponseHandler(bytes));
        string dir = Path.Combine(root, "repeat");
        string first = client.DownloadVerifiedAsync(target, dir, null, CancellationToken.None).GetAwaiter().GetResult();
        string second = client.DownloadVerifiedAsync(target, dir, null, CancellationToken.None).GetAwaiter().GetResult();
        if (first == second || !File.ReadAllBytes(first).SequenceEqual(bytes) || !File.ReadAllBytes(second).SequenceEqual(bytes))
            throw new Exception("Existing download overwritten");
    });
    Test("cancelled download leaves no file", () => {
        using var cancellation = new CancellationTokenSource();
        cancellation.Cancel();
        string dir = Path.Combine(root, "cancelled");
        var client = new ManifestClient(new TestResponseHandler([1, 2]));
        try { client.DownloadVerifiedAsync(Manifest().Targets[0], dir, null, cancellation.Token).GetAwaiter().GetResult(); }
        catch (OperationCanceledException) {
            if (Directory.EnumerateFiles(dir).Any()) throw new Exception("Cancelled download retained");
            return;
        }
        throw new Exception("Cancellation ignored");
    });
    Test("backup saves bytes and bound metadata", () => {
        byte[] data = new byte[BackupRestoreService.FlashSize];
        for (int i = 0; i < data.Length; i++) data[i] = (byte)i;
        var result = BackupRestoreService.SaveSnapshot("c1", Path.Combine(root, "backups"), data);
        var meta = System.Text.Json.JsonSerializer.Deserialize<BackupMetadata>(File.ReadAllText(result.MetadataPath))!;
        if (!File.ReadAllBytes(result.BinPath).SequenceEqual(data) || result.Role != "C1" ||
            meta.Role != result.Role || meta.FileName != Path.GetFileName(result.BinPath) ||
            meta.FlashStart != BackupRestoreService.FlashStart || meta.FlashSize != data.Length ||
            meta.Sha256 != result.Sha256 || meta.CreatedUtc.Kind != DateTimeKind.Utc ||
            result.Sha256 != Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(data)).ToLowerInvariant())
            throw new Exception("Backup metadata or bytes changed");
    });
    Test("parallel backups never overwrite", () => {
        string dir = Path.Combine(root, "parallel-backups");
        var paths = new System.Collections.Concurrent.ConcurrentBag<string>();
        Parallel.For(0, 12, i => {
            byte[] data = new byte[BackupRestoreService.FlashSize]; data[0] = (byte)i;
            var result = BackupRestoreService.SaveSnapshot("BH", dir, data);
            paths.Add(result.BinPath);
            if (!File.ReadAllBytes(result.BinPath).SequenceEqual(data)) throw new Exception("Backup overwritten");
        });
        if (paths.Distinct().Count() != 12 || Directory.EnumerateFiles(dir).Count() != 24)
            throw new Exception("Backup files collided");
    });
    Test("incomplete backup rejected before writing", () => {
        string dir = Path.Combine(root, "incomplete-backup");
        Reject(() => BackupRestoreService.SaveSnapshot("BH", dir, new byte[1]));
        if (Directory.Exists(dir)) throw new Exception("Incomplete backup published");
    });
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
    Test("USB path preserves native prefix on x64", () => {
        string expected = @"\\?\usb#vid_0483&pid_df11#test";
        byte[] bytes = System.Text.Encoding.Unicode.GetBytes(expected + "\0");
        IntPtr buffer = System.Runtime.InteropServices.Marshal.AllocHGlobal(bytes.Length + 4);
        try {
            System.Runtime.InteropServices.Marshal.WriteInt32(buffer, IntPtr.Size == 8 ? 8 : 6);
            System.Runtime.InteropServices.Marshal.Copy(bytes, 0, IntPtr.Add(buffer, 4), bytes.Length);
            if (UsbDeviceEnumerator.ReadDevicePath(buffer, (uint)(bytes.Length + 4)) != expected)
                throw new Exception("Native device path truncated");
            Reject(() => UsbDeviceEnumerator.ReadDevicePath(buffer, (uint)(bytes.Length + 2)));
        } finally { System.Runtime.InteropServices.Marshal.FreeHGlobal(buffer); }
    });
    BackupImportTests.Run(Test, root);
    DfuWorkflowTests.Run(Test, root, new PreparedFirmware(Manifest().Targets[0], "test-only", Load(upper, payload, eof)));
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
