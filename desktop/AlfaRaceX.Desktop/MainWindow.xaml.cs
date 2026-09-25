using AlfaRaceX.Updater;
using Microsoft.Web.WebView2.Core;
using Microsoft.Win32;
using System.Diagnostics;
using System.Reflection;
using System.Security.Cryptography;
using System.Text.Json;
using System.Windows;

namespace AlfaRaceX.Desktop;

public partial class MainWindow : Window
{
    private const string UiHost = "app.alfaracex.local";
    private static string DisclaimerVersion => Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(Path.Combine(AppContext.BaseDirectory, "DISCLAIMER.md"))));
    private readonly HistoryRepository _history = new(DesktopPaths.Database);
    private readonly DesktopUpdateCoordinator _updates = new();
    private readonly BackupRestoreService _backups = new();
    private readonly Dictionary<string, PreparedFirmware> _prepared = new(StringComparer.OrdinalIgnoreCase);
    private CancellationTokenSource? _operation;
    private UpdateManifest? _manifest;

    public MainWindow()
    {
        InitializeComponent();
        DesktopPaths.Ensure();
        _history.Initialize();
        _history.AddEvent("INFO", "APP", $"Avvio AlfaRaceX Desktop {AppVersion}.");
        Loaded += OnLoaded;
        Closing += (_, e) =>
        {
            if (_operation is not null)
            {
                e.Cancel = true;
                MessageBox.Show(this, "Attendi il completamento o annulla l’operazione prima di chiudere.", "Operazione in corso");
            }
        };
    }

    private static string AppVersion =>
        Assembly.GetExecutingAssembly().GetName().Version?.ToString(3) ?? "0.0.0";

    private async void OnLoaded(object sender, RoutedEventArgs e)
    {
        try
        {
            var environment = await CoreWebView2Environment.CreateAsync(userDataFolder: DesktopPaths.WebView2);
            await Browser.EnsureCoreWebView2Async(environment);

            string webRoot = Path.Combine(AppContext.BaseDirectory, "wwwroot");
            if (!Directory.Exists(webRoot))
                throw new DirectoryNotFoundException($"UI locale non trovata: {webRoot}");

            Browser.CoreWebView2.SetVirtualHostNameToFolderMapping(
                UiHost,
                webRoot,
                CoreWebView2HostResourceAccessKind.DenyCors);

            Browser.CoreWebView2.Settings.AreDevToolsEnabled = false;
            Browser.CoreWebView2.Settings.AreDefaultContextMenusEnabled = false;
            Browser.CoreWebView2.Settings.IsStatusBarEnabled = false;
            Browser.CoreWebView2.Settings.IsZoomControlEnabled = false;
            Browser.CoreWebView2.Settings.IsWebMessageEnabled = true;

            Browser.CoreWebView2.NavigationStarting += (_, args) =>
            {
                if (!Uri.TryCreate(args.Uri, UriKind.Absolute, out Uri? uri) ||
                    !string.Equals(uri.Host, UiHost, StringComparison.OrdinalIgnoreCase))
                    args.Cancel = true;
            };
            Browser.CoreWebView2.NewWindowRequested += (_, args) => args.Handled = true;
            Browser.CoreWebView2.WebMessageReceived += OnWebMessageReceived;
            Browser.Source = new Uri($"https://{UiHost}/index.html");
        }
        catch (Exception ex)
        {
            _history.AddEvent("ERROR", "UI", ex.Message);
            MessageBox.Show(
                "Impossibile avviare l'interfaccia AlfaRaceX.\n\n" + ex.Message,
                "AlfaRaceX",
                MessageBoxButton.OK,
                MessageBoxImage.Error);
            Close();
        }
    }

    private async void OnWebMessageReceived(object? sender, CoreWebView2WebMessageReceivedEventArgs e)
    {
        try
        {
            using JsonDocument doc = JsonDocument.Parse(e.WebMessageAsJson);
            JsonElement root = doc.RootElement;
            string action = root.TryGetProperty("action", out JsonElement a) ? a.GetString() ?? "" : "";
            JsonElement payload = root.TryGetProperty("payload", out JsonElement p) ? p.Clone() : default;
            await HandleActionAsync(action, payload);
        }
        catch (Exception ex)
        {
            Log("ERROR", "BRIDGE", ex.Message);
            Post("error", new { message = ex.Message });
        }
    }

    private async Task HandleActionAsync(string action, JsonElement payload)
    {
        if (!IsDisclaimerAccepted() &&
            action is not ("initialize" or "acceptDisclaimer" or "exitApplication" or "openExternal"))
            throw new InvalidOperationException("Per utilizzare AlfaRaceX è necessario accettare il disclaimer di sicurezza.");

        switch (action)
        {
            case "initialize":
                await SendInitialStateAsync();
                break;
            case "acceptDisclaimer":
                AcceptDisclaimer();
                await SendInitialStateAsync();
                break;
            case "exitApplication":
                Close();
                break;
            case "refreshDashboard":
                await SendDashboardAsync();
                break;
            case "loadManifest":
                await SendManifestAsync(force: true);
                break;
            case "prepareUpdate":
                await PrepareUpdateAsync();
                break;
            case "flashRole":
                await FlashRoleAsync(ReadString(payload, "role"));
                break;
            case "createBackup":
                await CreateBackupAsync(ReadString(payload, "role"));
                break;
            case "listBackups":
                SendBackups();
                break;
            case "restoreBackup":
                await RestoreBackupAsync(ReadLong(payload, "id"));
                break;
            case "importBackup":
                ImportBackup(ReadString(payload, "role"));
                break;
            case "copyLogs":
                Clipboard.SetText(ExportLogText());
                break;
            case "exportLogs":
                var save = new SaveFileDialog { Filter = "Log (*.log)|*.log", FileName = $"AlfaRaceX-{DateTime.Now:yyyyMMdd-HHmmss}.log" };
                if (save.ShowDialog(this) == true) File.WriteAllText(save.FileName, ExportLogText());
                break;
            case "openLogFolder":
                OpenFolder(DesktopPaths.Logs);
                break;
            case "getLogs":
                SendLogs();
                break;
            case "clearLogs":
                _history.ClearEvents();
                Log("INFO", "LOG", "Cronologia log azzerata dall'utente.");
                SendLogs();
                break;
            case "cancelOperation":
                _operation?.Cancel();
                break;
            case "openDataFolder":
                OpenFolder(DesktopPaths.Root);
                break;
            case "openBackupFolder":
                OpenFolder(DesktopPaths.Backups);
                break;
            case "openBackupLocation":
                OpenFileLocation(ReadLong(payload, "id"));
                break;
            case "openExternal":
                OpenExternal(ReadString(payload, "url"));
                break;
            default:
                throw new InvalidOperationException($"Azione UI non supportata: {action}");
        }
    }

    private async Task SendInitialStateAsync()
    {
        Post("appInfo", new
        {
            appVersion = AppVersion,
            dataRoot = DesktopPaths.Root,
            backupRoot = DesktopPaths.Backups,
            database = DesktopPaths.Database,
            product = "AlfaRaceX",
            repository = "https://github.com/AriotaG/AlfaRaceX-Framework",
            disclaimerVersion = DisclaimerVersion,
            disclaimerAccepted = IsDisclaimerAccepted(),
            disclaimerAcceptedUtc = _history.GetSetting("DisclaimerAcceptedUtc"),
            disclaimerText = File.ReadAllText(Path.Combine(AppContext.BaseDirectory, "DISCLAIMER.md"))
        });
        if (!IsDisclaimerAccepted()) return;
        await SendDashboardAsync();
        await SendManifestAsync(force: false);
        SendBackups();
        SendLogs();
    }

    private async Task SendDashboardAsync()
    {
        int dfuCount;
        try { dfuCount = await Task.Run(() => UsbDeviceEnumerator.FindDfuPaths().Count); }
        catch { dfuCount = 0; }

        string firmwareVersion = _manifest?.Version ?? "—";
        string channel = _manifest?.Channel ?? "Release Candidate";
        Post("dashboard", new
        {
            appVersion = AppVersion,
            firmwareVersion,
            channel,
            dfuCount,
            backupCount = _history.CountBackups(),
            preparedCount = _prepared.Count,
            dataRoot = DesktopPaths.Root,
            lastCheckUtc = _history.GetSetting("LastCheckUtc")
        });
    }

    private async Task SendManifestAsync(bool force)
    {
        if (_operation is not null) throw new InvalidOperationException("Attendi la fine dell’operazione prima di cambiare manifest.");
        try
        {
            if (_manifest is null || force)
            {
                var manifest = await _updates.LoadManifestAsync(CancellationToken.None);
                _prepared.Clear();
                _manifest = manifest;
                _history.SetSetting("LastCheckUtc", DateTime.UtcNow.ToString("O"));
            }

            Post("manifest", new
            {
                version = _manifest.Version,
                releases = _updates.Releases,
                channel = _manifest.Channel,
                targets = _manifest.Targets.Select(t => new
                {
                    id = t.Id,
                    label = t.Label,
                    portHint = t.PortHint,
                    sha256 = t.Sha256,
                    prepared = _prepared.ContainsKey(t.Id)
                })
            });
            await SendDashboardAsync();
        }
        catch (Exception ex)
        {
            Log("WARN", "UPDATE", "Manifest firmware non disponibile: " + ex.Message);
            Post("manifestError", new { message = ex.Message });
        }
    }

    private async Task PrepareUpdateAsync()
    {
        await RunExclusiveAsync("UPDATE", async ct =>
        {
            _manifest ??= await _updates.LoadManifestAsync(ct);
            var progress = new Progress<(string Message, int Progress)>(p =>
                Post("operationProgress", new { kind = "prepare", message = p.Message, progress = p.Progress }));

            IReadOnlyList<PreparedFirmware> prepared = await _updates.PrepareAsync(_manifest, progress, ct);
            _prepared.Clear();
            foreach (PreparedFirmware firmware in prepared)
                _prepared[firmware.Target.Id] = firmware;

            Log("INFO", "UPDATE", $"Firmware {_manifest.Version} scaricati e verificati SHA-256.");
            Post("operationComplete", new { kind = "prepare", message = "Firmware scaricati e verificati." });
            await SendManifestAsync(force: false);
        });
    }

    private async Task FlashRoleAsync(string role)
    {
        role = NormalizeRole(role);
        if (!_prepared.TryGetValue(role, out PreparedFirmware? firmware))
            throw new InvalidOperationException("Prima scarica e verifica il pacchetto firmware.");

        if (MessageBox.Show(this, $"Programmare {role}? Verifica fisicamente la porta del modulo: il bootloader DFU non identifica il ruolo. Verrà creato e catalogato un backup prima della scrittura.", "Conferma programmazione", MessageBoxButton.YesNo, MessageBoxImage.Warning) != MessageBoxResult.Yes) return;
        await RunExclusiveAsync("FLASH", async ct =>
        {
            var progress = new Progress<(string Message, int Progress)>(p =>
                Post("operationProgress", new { kind = "flash", role, message = p.Message, progress = p.Progress }));

            Log("INFO", "FLASH", $"Avvio programmazione {role} firmware {_manifest?.Version}.");
            await SafeFlashAsync(firmware, progress, ct);
            _history.SetSetting($"LastFlashedVersion:{role}", _manifest?.Version ?? "unknown");
            SendBackups();
            Log("INFO", "FLASH", $"Programmazione {role} completata e verificata.");
            Post("operationComplete", new { kind = "flash", role, message = $"{role} programmato e verificato." });
        });
    }

    private async Task CreateBackupAsync(string role)
    {
        role = NormalizeRole(role);
        await RunExclusiveAsync("BACKUP", async ct =>
        {
            var progress = new Progress<(string Message, int Progress)>(p =>
                Post("operationProgress", new { kind = "backup", role, message = p.Message, progress = p.Progress }));

            Log("INFO", "BACKUP", $"Avvio backup completo modulo {role}.");
            BackupResult result = await _backups.BackupAsync(
                role,
                DesktopPaths.Backups,
                progress,
                msg => Log("INFO", "DFU", msg),
                ct);

            _history.AddBackup(
                result.Role,
                result.BinPath,
                result.MetadataPath,
                result.Sha256,
                result.Size,
                DateTime.UtcNow,
                "local");

            Log("INFO", "BACKUP", $"Backup {role} catalogato: {Path.GetFileName(result.BinPath)}.");
            Post("operationComplete", new { kind = "backup", role, message = $"Backup {role} completato.", sha256 = result.Sha256 });
            SendBackups();
            await SendDashboardAsync();
        });
    }

    private async Task RestoreBackupAsync(long id)
    {
        BackupRecord backup = _history.GetBackup(id)
            ?? throw new FileNotFoundException("Backup non presente nel catalogo.");
        if (!File.Exists(backup.BinPath))
            throw new FileNotFoundException("Il file del backup catalogato non esiste più.", backup.BinPath);

        if (MessageBox.Show(this, $"Ripristinare {backup.Role}? La Flash verrà riscritta. Verifica la porta: il ruolo fisico non è identificabile dal bootloader.", "Conferma ripristino", MessageBoxButton.YesNo, MessageBoxImage.Warning) != MessageBoxResult.Yes) return;
        await RunExclusiveAsync("RESTORE", async ct =>
        {
            var progress = new Progress<(string Message, int Progress)>(p =>
                Post("operationProgress", new { kind = "restore", role = backup.Role, message = p.Message, progress = p.Progress }));

            await using (var input = File.OpenRead(backup.BinPath))
            {
                var hash = Convert.ToHexString(await SHA256.HashDataAsync(input, ct));
                if (!hash.Equals(backup.Sha256, StringComparison.OrdinalIgnoreCase))
                    throw new InvalidDataException("Il backup è stato modificato: SHA-256 diverso dal catalogo.");
            }
            Log("WARN", "RESTORE", $"Avvio ripristino {backup.Role} da {Path.GetFileName(backup.BinPath)}.");
            await _backups.RestoreAsync(
                backup.Role,
                backup.BinPath,
                progress,
                msg => Log("INFO", "DFU", msg),
                ct);

            Log("INFO", "RESTORE", $"Ripristino {backup.Role} completato e verificato.");
            Post("operationComplete", new { kind = "restore", role = backup.Role, message = $"Ripristino {backup.Role} completato." });
        });
    }

    private void ImportBackup(string role)
    {
        role = NormalizeRole(role);
        var dialog = new OpenFileDialog
        {
            Title = $"Importa backup {role}",
            Filter = "Backup AlfaRaceX (*.bin)|*.bin|Tutti i file (*.*)|*.*",
            CheckFileExists = true,
            Multiselect = false
        };
        if (dialog.ShowDialog(this) != true)
            return;

        var info = new FileInfo(dialog.FileName);
        if (info.Length != BackupRestoreService.FlashSize)
            throw new InvalidDataException($"Backup non valido: attesi {BackupRestoreService.FlashSize} byte, trovati {info.Length}.");

        string sha = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(dialog.FileName))).ToLowerInvariant();
        string destination = Path.Combine(DesktopPaths.Backups, $"AlfaRaceX-{role}-import-{Guid.NewGuid():N}.bin");
        string metadata = Path.ChangeExtension(dialog.FileName, ".json");
        if (File.Exists(metadata))
        {
            var meta = JsonSerializer.Deserialize<BackupMetadata>(File.ReadAllText(metadata));
            if (meta is null || !meta.Role.Equals(role, StringComparison.OrdinalIgnoreCase) || !meta.Sha256.Equals(sha, StringComparison.OrdinalIgnoreCase) || meta.FlashSize != info.Length || meta.FlashStart != BackupRestoreService.FlashStart)
                throw new InvalidDataException("Metadati backup incompatibili con il file o il ruolo scelto.");
        }
        File.Copy(dialog.FileName, destination);
        string savedMeta = Path.ChangeExtension(destination, ".json");
        if (File.Exists(metadata)) File.Copy(metadata, savedMeta);
        _history.AddBackup(role, destination, File.Exists(savedMeta) ? savedMeta : string.Empty, sha, info.Length, DateTime.UtcNow, "imported");
        Log("INFO", "BACKUP", $"Importato backup {role}: {Path.GetFileName(dialog.FileName)} ({sha}).");
        SendBackups();
    }

    private async Task RunExclusiveAsync(string category, Func<CancellationToken, Task> work)
    {
        if (_operation is not null)
            throw new InvalidOperationException("È già in corso un'operazione. Attendi il completamento o annullala.");

        _operation = new CancellationTokenSource();
        Post("busy", new { value = true, category });
        try
        {
            await work(_operation.Token);
        }
        catch (OperationCanceledException)
        {
            Log("WARN", category, "Operazione annullata.");
            Post("operationCancelled", new { category });
        }
        catch (Exception ex)
        {
            Log("ERROR", category, ex.ToString());
            Post("operationFailed", new { category, message = ex.Message });
        }
        finally
        {
            _operation.Dispose();
            _operation = null;
            Post("busy", new { value = false, category });
            SendLogs();
        }
    }

    private bool IsDisclaimerAccepted() =>
        string.Equals(
            _history.GetSetting("DisclaimerVersion"),
            DisclaimerVersion,
            StringComparison.Ordinal);

    private void AcceptDisclaimer()
    {
        string acceptedUtc = DateTime.UtcNow.ToString("O");
        _history.SetSetting("DisclaimerVersion", DisclaimerVersion);
        _history.SetSetting("DisclaimerAcceptedUtc", acceptedUtc);
        _history.AddEvent("INFO", "LEGAL", $"Disclaimer {DisclaimerVersion} accettato dall'utente.");
        Post("disclaimerAccepted", new { version = DisclaimerVersion, acceptedUtc });
    }

    private void SendBackups()
    {
        var items = _history.GetBackups().Select(b => new
        {
            id = b.Id,
            role = b.Role,
            fileName = Path.GetFileName(b.BinPath),
            path = b.BinPath,
            sha256 = b.Sha256,
            size = b.Size,
            createdUtc = b.CreatedUtc,
            source = b.Source,
            exists = File.Exists(b.BinPath)
        });
        Post("backups", items);
    }

    private void SendLogs()
    {
        Post("logs", _history.GetEvents().Select(x => new
        {
            id = x.Id,
            createdUtc = x.CreatedUtc,
            level = x.Level,
            category = x.Category,
            message = x.Message
        }));
    }

    private void Log(string level, string category, string message)
    {
        _history.AddEvent(level, category, message);
        Post("log", new { createdUtc = DateTime.UtcNow, level, category, message });
    }

    private void Post(string type, object data)
    {
        if (!Dispatcher.CheckAccess())
        {
            Dispatcher.BeginInvoke(new Action(() => Post(type, data)));
            return;
        }
        if (Browser.CoreWebView2 is null)
            return;
        string json = JsonSerializer.Serialize(new { type, data });
        Browser.CoreWebView2.PostWebMessageAsJson(json);
    }

    private string ExportLogText() => string.Join(Environment.NewLine,
        _history.GetEvents(5000).Select(x => $"{x.CreatedUtc:O}\t{x.Level}\t{x.Category}\t{x.Message}"));

    private Task SafeFlashAsync(PreparedFirmware firmware, IProgress<(string Message, int Progress)> progress, CancellationToken ct) => Task.Run(() =>
    {
        var drive = new DriveInfo(Path.GetPathRoot(DesktopPaths.Backups)!);
        if (drive.AvailableFreeSpace < 4 * 1024 * 1024) throw new IOException("Spazio insufficiente per il backup preventivo.");
        string hash = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(firmware.LocalPath)));
        if (!hash.Equals(firmware.Target.Sha256, StringComparison.OrdinalIgnoreCase)) throw new InvalidDataException("Firmware locale modificato dopo il download.");
        using var dfu = DfuDevice.OpenSingle();
        progress.Report(("Backup automatico prima della scrittura…", 0));
        byte[] data = dfu.ReadMemory(BackupRestoreService.FlashStart, BackupRestoreService.FlashSize,
            new InlineProgress<int>(p => progress.Report(("Backup automatico…", p))), ct);
        string sha = Convert.ToHexString(SHA256.HashData(data)).ToLowerInvariant();
        string path = Path.Combine(DesktopPaths.Backups, $"AlfaRaceX-{firmware.Target.Id}-preflash-{DateTime.UtcNow:yyyyMMdd-HHmmss}-{Guid.NewGuid():N}.bin");
        File.WriteAllBytes(path, data);
        if (!SHA256.HashData(File.ReadAllBytes(path)).SequenceEqual(SHA256.HashData(data))) throw new IOException("Verifica backup su disco fallita.");
        string metadata = Path.ChangeExtension(path, ".json");
        File.WriteAllText(metadata, JsonSerializer.Serialize(new BackupMetadata { Role = firmware.Target.Id, CreatedUtc = DateTime.UtcNow, FlashStart = BackupRestoreService.FlashStart, FlashSize = data.Length, Sha256 = sha, FileName = Path.GetFileName(path), UpdaterVersion = AppVersion }));
        _history.AddBackup(firmware.Target.Id, path, metadata, sha, data.Length, DateTime.UtcNow, "automatic-preflash");
        ct.ThrowIfCancellationRequested();
        // Keep the same USB handle from backup through readback: do not reopen another device.
        dfu.ProgramAndVerify(firmware.Image, AppConstants.FlashPageSize,
            new InlineProgress<int>(p => progress.Report(("Programmazione e verifica…", p))), msg => Log("INFO", "DFU", msg), ct);
        try { dfu.Leave(firmware.Target.ApplicationStart); }
        catch (IOException) { Log("INFO", "DFU", "Disconnessione dopo il comando di avvio."); }
    }, ct);

    private sealed class InlineProgress<T>(Action<T> report) : IProgress<T>
    {
        public void Report(T value) => report(value);
    }

    private static string NormalizeRole(string role)
    {
        role = (role ?? string.Empty).Trim().ToUpperInvariant();
        if (role is not ("BH" or "C2" or "C1"))
            throw new ArgumentException("Ruolo modulo non valido.");
        return role;
    }

    private static string ReadString(JsonElement payload, string property)
    {
        if (payload.ValueKind == JsonValueKind.Object &&
            payload.TryGetProperty(property, out JsonElement value) &&
            value.ValueKind == JsonValueKind.String)
            return value.GetString() ?? string.Empty;
        return string.Empty;
    }

    private static long ReadLong(JsonElement payload, string property)
    {
        if (payload.ValueKind == JsonValueKind.Object &&
            payload.TryGetProperty(property, out JsonElement value) &&
            value.TryGetInt64(out long result))
            return result;
        throw new ArgumentException($"Parametro {property} non valido.");
    }

    private static void OpenFolder(string path)
    {
        Directory.CreateDirectory(path);
        Process.Start(new ProcessStartInfo("explorer.exe", $"\"{path}\"") { UseShellExecute = true });
    }

    private void OpenFileLocation(long id)
    {
        BackupRecord backup = _history.GetBackup(id)
            ?? throw new FileNotFoundException("Backup non presente nel catalogo.");
        if (File.Exists(backup.BinPath))
            Process.Start(new ProcessStartInfo("explorer.exe", $"/select,\"{backup.BinPath}\"") { UseShellExecute = true });
        else
            OpenFolder(Path.GetDirectoryName(backup.BinPath) ?? DesktopPaths.Backups);
    }

    private static void OpenExternal(string url)
    {
        if (!Uri.TryCreate(url, UriKind.Absolute, out Uri? uri) || uri.Scheme != Uri.UriSchemeHttps)
            throw new InvalidOperationException("URL esterno non consentito.");
        Process.Start(new ProcessStartInfo(uri.ToString()) { UseShellExecute = true });
    }
}
