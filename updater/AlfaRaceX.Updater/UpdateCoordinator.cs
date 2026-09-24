namespace AlfaRaceX.Updater;

internal sealed class UpdateCoordinator
{
    private readonly ManifestClient _client = new();

    public Task<UpdateManifest> LoadManifestAsync(CancellationToken ct) =>
        _client.GetManifestAsync(AppConstants.DefaultManifestUrl, ct);

    public async Task<IReadOnlyList<PreparedFirmware>> PrepareAsync(
        UpdateManifest manifest,
        IProgress<(string Message, int Progress)> progress,
        CancellationToken ct)
    {
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
        CancellationToken ct)
    {
        return Task.Run(() =>
        {
            progress.Report(($"Connessione {firmware.Target.Label}...", 0));
            using var dfu = DfuDevice.OpenSingle();

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
            catch (IOException)
            {
                log($"{firmware.Target.Label}: disconnessione avvenuta dopo il comando di avvio.");
            }

            progress.Report(($"{firmware.Target.Label} completato.", 100));
        }, ct);
    }
}
