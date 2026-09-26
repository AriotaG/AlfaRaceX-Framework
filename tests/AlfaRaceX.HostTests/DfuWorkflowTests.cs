using AlfaRaceX.Updater;

// Test-only session: verifies orchestration, never USB protocol or physical hardware.
internal static class DfuWorkflowTests
{
    public static void Run(Action<string, Action> test, string root, PreparedFirmware firmware)
    {
        var progress = new Progress<(string Message, int Progress)>();
        void Check(bool value) { if (!value) throw new Exception("Unsafe DFU workflow ordering"); }
        void Flash(Session session, string folder, Action<BackupResult>? saved = null, CancellationToken ct = default) =>
            new UpdateCoordinator(() => session).FlashAsync(firmware, progress, _ => { }, ct, folder, saved,
                (role, path) => role == firmware.Target.Id && path == session.DevicePath).GetAwaiter().GetResult();
        void Reject<T>(Action action) where T : Exception
        {
            try { action(); }
            catch (T) { return; }
            throw new Exception("Unsafe operation was not rejected");
        }
        test("flash saves and catalogs original bytes before write", () => {
            var session = new Session();
            Flash(session, Path.Combine(root, "pre-flash"), backup => {
                Check(File.ReadAllBytes(backup.BinPath).SequenceEqual(session.Original));
                Check(File.Exists(backup.MetadataPath));
                session.Events.Add("catalog");
            });
            Check(session.Events.SequenceEqual(new[] { "read", "catalog", "flash", "leave", "dispose" }));
        });
        test("flash without physical role confirmation never reads or writes", () => {
            var session = new Session();
            Reject<InvalidOperationException>(() => new UpdateCoordinator(() => session)
                .FlashAsync(firmware, progress, _ => { }, default, Path.Combine(root, "unconfirmed"))
                .GetAwaiter().GetResult());
            Check(session.Events.SequenceEqual(new[] { "dispose" }));
        });
        test("declined role confirmation closes the same opened device without writes", () => {
            var session = new Session(); bool asked = false;
            Reject<OperationCanceledException>(() => new UpdateCoordinator(() => session)
                .FlashAsync(firmware, progress, _ => { }, default, confirmTarget: (role, path) => {
                    Check(role == firmware.Target.Id && path == session.DevicePath);
                    asked = true; return false;
                }).GetAwaiter().GetResult());
            Check(asked && session.Events.SequenceEqual(new[] { "dispose" }));
        });
        test("failed backup read prevents flash", () => {
            var session = new Session { ReadError = true };
            Reject<IOException>(() => Flash(session, Path.Combine(root, "failed-read")));
            Check(session.Events.SequenceEqual(new[] { "read", "dispose" }));
        });
        test("failed backup storage prevents flash", () => {
            var session = new Session();
            string path = Path.Combine(root, "not-a-folder"); File.WriteAllText(path, "keep");
            Reject<IOException>(() => Flash(session, path));
            Check(!session.Events.Contains("flash") && File.ReadAllText(path) == "keep");
        });
        test("failed backup catalog prevents flash", () => {
            var session = new Session();
            Reject<IOException>(() => Flash(session, Path.Combine(root, "failed-catalog"), _ => throw new IOException("test catalog failure")));
            Check(!session.Events.Contains("flash") && session.Events.Last() == "dispose");
        });
        test("cancellation after backup prevents flash", () => {
            var session = new Session(); using var ct = new CancellationTokenSource();
            Reject<OperationCanceledException>(() => Flash(session, Path.Combine(root, "cancel-pre-flash"), _ => ct.Cancel(), ct.Token));
            Check(!session.Events.Contains("flash") && session.Events.Last() == "dispose");
        });
        test("catalog hash mismatch refuses restore before device access", () => {
            var backup = BackupRestoreService.SaveSnapshot("BH", Path.Combine(root, "restore-input"), new byte[BackupRestoreService.FlashSize]);
            bool opened = false;
            var service = new BackupRestoreService(() => { opened = true; return new Session(); });
            Reject<InvalidDataException>(() => service.RestoreAsync("BH", backup.BinPath, progress, _ => { }, default,
                new string('a', 64), Path.Combine(root, "pre-restore-failed")).GetAwaiter().GetResult());
            Check(!opened);
        });
        test("restore saves original bytes on the same device before write", () => {
            var backup = BackupRestoreService.SaveSnapshot("BH", Path.Combine(root, "restore-valid"), new byte[BackupRestoreService.FlashSize]);
            var session = new Session(); int opens = 0;
            var service = new BackupRestoreService(() => { opens++; return session; });
            service.RestoreAsync("BH", backup.BinPath, progress, _ => { }, default, backup.Sha256,
                Path.Combine(root, "pre-restore"), saved => {
                    Check(File.ReadAllBytes(saved.BinPath).SequenceEqual(session.Original)); session.Events.Add("catalog");
                }, (role, path) => role == "BH" && path == session.DevicePath).GetAwaiter().GetResult();
            Check(opens == 1 && session.Events.SequenceEqual(new[] { "read", "catalog", "restore", "leave", "dispose" }));
        });
        test("raw restore without registered hash or metadata is rejected", () => {
            string path = Path.Combine(root, "unregistered.bin"); File.WriteAllBytes(path, new byte[BackupRestoreService.FlashSize]);
            bool opened = false;
            var service = new BackupRestoreService(() => { opened = true; return new Session(); });
            Reject<InvalidDataException>(() => service.RestoreAsync("BH", path, progress, _ => { }, default).GetAwaiter().GetResult());
            Check(!opened);
        });
        test("valid restore without role confirmation cannot write", () => {
            var backup = BackupRestoreService.SaveSnapshot("BH", Path.Combine(root, "restore-unconfirmed"), new byte[BackupRestoreService.FlashSize]);
            var session = new Session();
            Reject<InvalidOperationException>(() => new BackupRestoreService(() => session)
                .RestoreAsync("BH", backup.BinPath, progress, _ => { }, default).GetAwaiter().GetResult());
            Check(session.Events.SequenceEqual(new[] { "dispose" }));
        });
    }

    private sealed class Session : IDfuDevice
    {
        public string DevicePath => "test-only-dfu-session";
        public readonly List<string> Events = [];
        public readonly byte[] Original = Enumerable.Repeat((byte)0xA5, BackupRestoreService.FlashSize).ToArray();
        public bool ReadError { get; init; }
        public byte[] ReadMemory(uint address, int length, IProgress<int>? progress, CancellationToken ct)
        {
            Events.Add("read");
            if (ReadError) throw new IOException("test read failure");
            ct.ThrowIfCancellationRequested();
            return Original.ToArray();
        }
        public void ProgramAndVerify(IntelHexImage image, uint pageSize, IProgress<int>? progress, Action<string>? log, CancellationToken ct) => Events.Add("flash");
        public void ProgramRawAndVerify(uint address, byte[] data, uint pageSize, IProgress<int>? progress, Action<string>? log, CancellationToken ct) => Events.Add("restore");
        public void Leave(uint address) => Events.Add("leave");
        public void Dispose() => Events.Add("dispose");
    }
}
