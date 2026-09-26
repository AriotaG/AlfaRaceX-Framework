using System.Security.Cryptography;
using System.Text.Json;
using System.Text.RegularExpressions;

namespace AlfaRaceX.Updater;

internal sealed record BackupResult(
    string Role,
    string BinPath,
    string MetadataPath,
    string Sha256,
    int Size);

internal sealed class BackupMetadata
{
    public string Product { get; set; } = "AlfaRaceX";
    public string Role { get; set; } = "";
    public string UpdaterVersion { get; set; } = "";
    public DateTime CreatedUtc { get; set; }
    public uint FlashStart { get; set; }
    public int FlashSize { get; set; }
    public string Sha256 { get; set; } = "";
    public string FileName { get; set; } = "";
    public string RoleEvidence { get; set; } = "user-selected; physical MCU role not verified";
    public string Scope { get; set; } = "internal-flash-only; excludes option bytes, OTP and system ROM";
}

internal sealed class BackupRestoreService
{
    public const uint FlashStart = 0x08000000u;
    public const int FlashSize = 0x20000;

    private static readonly HashSet<string> ValidRoles =
        new(StringComparer.OrdinalIgnoreCase) { "BH", "C2", "C1" };

    public Task<BackupResult> BackupAsync(
        string role,
        string destinationFolder,
        IProgress<(string Message, int Progress)> progress,
        Action<string> log,
        CancellationToken ct)
    {
        role = NormalizeRole(role);

        return Task.Run(() =>
        {
            Directory.CreateDirectory(destinationFolder);

            progress.Report(($"Connessione DFU {role}...", 0));
            using var dfu = DfuDevice.OpenSingle();

            log($"Backup {role}: lettura completa Flash interna.");
            var readProgress = new Progress<int>(p =>
                progress.Report(($"Backup {role}: lettura Flash...", p)));

            byte[] data = dfu.ReadMemory(
                FlashStart,
                FlashSize,
                readProgress,
                ct);

            BackupResult result = SaveSnapshot(role, destinationFolder, data);
            log($"Backup {role}: SHA-256 {result.Sha256}.");
            progress.Report(($"Backup {role} completato.", 100));
            return result;
        }, ct);
    }

    // Saves the bytes already read from the device. This does not prove physical MCU identity.
    internal static BackupResult SaveSnapshot(string role, string destinationFolder, byte[] data)
    {
        role = NormalizeRole(role);
        if (data.Length != FlashSize)
            throw new InvalidDataException($"Backup incompleto: attesi {FlashSize} byte, trovati {data.Length}.");
        Directory.CreateDirectory(destinationFolder);
        string hash = Convert.ToHexString(SHA256.HashData(data)).ToLowerInvariant();
        DateTime created = DateTime.UtcNow;
        string name = $"AlfaRaceX-{role}-backup-{created:yyyyMMdd-HHmmss-fffffff}-{Guid.NewGuid():N}.bin";
        string binPath = Path.Combine(destinationFolder, name);
        using (var file = new FileStream(binPath, FileMode.CreateNew, FileAccess.Write, FileShare.None))
        {
            file.Write(data);
            file.Flush(flushToDisk: true);
        }
        using (var saved = File.OpenRead(binPath))
        {
            if (!string.Equals(Convert.ToHexString(SHA256.HashData(saved)), hash, StringComparison.OrdinalIgnoreCase))
                throw new IOException("Verifica del backup scritto su disco fallita.");
        }
        var metadata = new BackupMetadata
        {
            Role = role, UpdaterVersion = AppConstants.UpdaterVersion, CreatedUtc = created,
            FlashStart = FlashStart, FlashSize = data.Length, Sha256 = hash, FileName = name
        };
        string metadataPath = Path.ChangeExtension(binPath, ".json");
        using (var file = new FileStream(metadataPath, FileMode.CreateNew, FileAccess.Write, FileShare.None))
        {
            JsonSerializer.Serialize(file, metadata, new JsonSerializerOptions { WriteIndented = true });
            file.Flush(flushToDisk: true);
        }
        return new BackupResult(role, binPath, metadataPath, hash, data.Length);
    }

    public Task RestoreAsync(
        string role,
        string binPath,
        IProgress<(string Message, int Progress)> progress,
        Action<string> log,
        CancellationToken ct)
    {
        role = NormalizeRole(role);

        return Task.Run(() =>
        {
            if (!File.Exists(binPath))
                throw new FileNotFoundException(
                    "Il file di backup selezionato non esiste.",
                    binPath);

            byte[] data = File.ReadAllBytes(binPath);
            if (data.Length != FlashSize)
                throw new InvalidDataException(
                    $"Backup non valido: attesi {FlashSize} byte, trovati {data.Length}.");

            string? detectedRole = DetectRole(binPath);
            if (!string.IsNullOrWhiteSpace(detectedRole) &&
                !string.Equals(role, detectedRole, StringComparison.OrdinalIgnoreCase))
            {
                throw new InvalidDataException(
                    $"Il backup risulta associato al modulo {detectedRole}, " +
                    $"ma hai selezionato {role}.");
            }

            string hash = Convert.ToHexString(
                SHA256.HashData(data)).ToLowerInvariant();

            VerifyMetadataIfPresent(role, binPath, hash);

            progress.Report(($"Connessione DFU {role}...", 0));
            using var dfu = DfuDevice.OpenSingle();

            log($"Ripristino {role}: SHA-256 {hash}.");
            var restoreProgress = new Progress<int>(p =>
                progress.Report(($"Ripristino {role}: programmazione e verifica...", p)));

            dfu.ProgramRawAndVerify(
                FlashStart,
                data,
                AppConstants.FlashPageSize,
                restoreProgress,
                log,
                ct);

            try
            {
                dfu.Leave(FlashStart);
            }
            catch (IOException)
            {
                log($"{role}: disconnessione dopo comando di avvio.");
            }

            progress.Report(($"Ripristino {role} completato.", 100));
        }, ct);
    }

    private static void VerifyMetadataIfPresent(
        string role,
        string binPath,
        string actualHash)
    {
        string metadataPath = Path.ChangeExtension(binPath, ".json");
        if (!File.Exists(metadataPath))
            return;

        BackupMetadata? metadata = JsonSerializer.Deserialize<BackupMetadata>(
            File.ReadAllText(metadataPath));

        if (metadata is null)
            throw new InvalidDataException("Metadati backup non validi.");

        if (!string.Equals(metadata.Role, role, StringComparison.OrdinalIgnoreCase))
            throw new InvalidDataException(
                $"I metadati indicano il modulo {metadata.Role}, non {role}.");

        if (metadata.FlashStart != FlashStart ||
            metadata.FlashSize != FlashSize)
            throw new InvalidDataException(
                "I metadati del backup non corrispondono alla Flash STM32F072 attesa.");

        if (!string.Equals(
            metadata.Sha256,
            actualHash,
            StringComparison.OrdinalIgnoreCase))
            throw new InvalidDataException(
                "SHA-256 del backup diverso da quello registrato nei metadati.");
    }

    private static string NormalizeRole(string role)
    {
        role = (role ?? string.Empty).Trim().ToUpperInvariant();
        if (!ValidRoles.Contains(role))
            throw new ArgumentException("Ruolo modulo non valido.", nameof(role));
        return role;
    }

    private static string? DetectRole(string binPath)
    {
        string name = Path.GetFileName(binPath).ToUpperInvariant();

        foreach (string role in ValidRoles)
        {
            if (Regex.IsMatch(
                name,
                $@"(^|[-_]){Regex.Escape(role)}([-_.]|$)",
                RegexOptions.CultureInvariant))
                return role;
        }

        return null;
    }
}
