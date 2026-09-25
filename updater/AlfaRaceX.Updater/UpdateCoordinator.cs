namespace AlfaRaceX.Updater;

internal sealed class UpdateCoordinator
{
    private readonly ManifestClient _client = new();

    public Task<UpdaterManifest> LoadUpdaterManifestAsync(CancellationToken ct) =>
        _client.GetUpdaterManifestAsync(AppConstants.UpdaterManifestUrl, ct);

    public Task<UpdateManifest> LoadManifestAsync(CancellationToken ct) =>
        _client.GetManifestAsync(AppConstants.DefaultManifestUrl, ct);

    public async Task<IReadOnlyList<PreparedFirmware>> PrepareAsync(
        UpdateManifest manifest,
        IProgress<(string Message, int Progress)> progress,
        CancellationToken ct)
    {
        ManifestClient.ValidateManifest(manifest);
        string root = Path.Combine(Path.GetTempPath(), "AlfaRaceX-Updater", manifest.Version);
        var prepared = new List<PreparedFirmware>();

        foreach (FirmwareTarget target in manifest.Targets)
        {
            progress.Report(($"Download {target.Label}...", 0));
            var downloadProgress = new Progress<int>(p =>
                progress.Report(($"Download {target.Label}...", p)));

            string path = await _client.DownloadVerifiedAsync(
                target, root, downloadProgress, ct);

            IntelHexImage image = IntelHexImage.Load(path);
            image.ValidateApplicationRange(
                target.ApplicationStart,
                target.ApplicationLimitExclusive);

            prepared.Add(new PreparedFirmware(target, path, image));
        }

        progress.Report(("Firmware verificati.", 100));
        return prepared;
    }

    public Task FlashAsync(
        PreparedFirmware firmware,
        IProgress<(string Message, int Progress)> progress,
        Action<string> log,
        CancellationToken ct,
        BackupResult? safetyBackup = null)
    {
        return Task.Run(async () =>
        {
            ManifestClient.ValidateTarget(firmware.Target);
            firmware.Image.ValidateApplicationRange(firmware.Target.ApplicationStart, firmware.Target.ApplicationLimitExclusive);
            safetyBackup ??= await new BackupRestoreService().BackupAsync(firmware.Target.Id,
                Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "AlfaRaceX", "Backups"),
                progress, log, ct);
            progress.Report(($"Connessione {firmware.Target.Label}...", 0));
            using var dfu = DfuDevice.OpenSingle();
            if (!string.Equals(dfu.DevicePath, safetyBackup.DevicePath, StringComparison.OrdinalIgnoreCase))
                throw new InvalidOperationException("Dispositivo cambiato dopo il backup: programmazione rifiutata.");
            byte[] current = dfu.ReadMemory(BackupRestoreService.FlashStart, BackupRestoreService.FlashSize, null, ct);
            string currentHash = Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(current));
            if (!string.Equals(currentHash, safetyBackup.Sha256, StringComparison.OrdinalIgnoreCase))
                throw new InvalidDataException("Contenuto Flash cambiato dopo il backup: programmazione rifiutata.");

            log($"Rilevato dispositivo DFU per {firmware.Target.Label}.");
            log($"HEX: 0x{firmware.Image.MinAddress:X8} - 0x{firmware.Image.MaxAddress:X8}.");

            var flashProgress = new Progress<int>(p =>
                progress.Report(($"Programmazione {firmware.Target.Label}...", p)));

            dfu.ProgramAndVerify(
                firmware.Image,
                AppConstants.FlashPageSize,
                flashProgress,
                log,
                ct);

            log($"{firmware.Target.Label}: programmazione e verifica completate.");

            try
            {
                dfu.Leave(firmware.Target.ApplicationStart);
            }
            catch (IOException ex)
            {
                log($"{firmware.Target.Label}: Flash verificata; riavvio non confermato: {ex.Message}");
            }

            progress.Report(($"{firmware.Target.Label} completato.", 100));
        }, ct);
    }
}
