using System.Security.Cryptography;
using System.Text.Json;

namespace AlfaRaceX.Updater;

internal sealed class ManifestClient
{
    private readonly HttpClient _http;

    public ManifestClient(HttpMessageHandler? handler = null)
    {
        _http = handler is null ? new HttpClient() : new HttpClient(handler);
        _http.Timeout = TimeSpan.FromSeconds(30);
        _http.DefaultRequestHeaders.UserAgent.ParseAdd(
            $"AlfaRaceX-Updater/{AppConstants.UpdaterVersion}");
    }

    public async Task<UpdaterManifest> GetUpdaterManifestAsync(string url, CancellationToken ct)
    {
        await using var stream = await _http.GetStreamAsync(url, ct);
        var manifest = await JsonSerializer.DeserializeAsync<UpdaterManifest>(
            stream, cancellationToken: ct)
            ?? throw new InvalidDataException("Manifest Updater non valido.");

        if (!string.Equals(
            manifest.Product, AppConstants.ProductName, StringComparison.Ordinal))
            throw new InvalidDataException(
                "Il manifest Updater non appartiene ad AlfaRaceX.");

        if (string.IsNullOrWhiteSpace(manifest.Version) ||
            string.IsNullOrWhiteSpace(manifest.ReleaseUrl))
            throw new InvalidDataException("Manifest Updater incompleto.");

        return manifest;
    }

    public async Task<UpdateManifest> GetManifestAsync(string url, CancellationToken ct)
    {
        await using var stream = await _http.GetStreamAsync(url, ct);
        var manifest = await JsonSerializer.DeserializeAsync<UpdateManifest>(stream, cancellationToken: ct)
            ?? throw new InvalidDataException("Manifest non valido.");

        ValidateManifest(manifest);
        return manifest;
    }

    internal static void ValidateManifest(UpdateManifest manifest)
    {
        if (!string.Equals(manifest.Product, AppConstants.ProductName, StringComparison.Ordinal))
            throw new InvalidDataException("Il manifest non appartiene ad AlfaRaceX.");
        if (string.IsNullOrWhiteSpace(manifest.Version) ||
            !System.Text.RegularExpressions.Regex.IsMatch(manifest.Version, @"\A[0-9]+\.[0-9]+\.[0-9]+(?:-[A-Za-z0-9.-]+)?\z"))
            throw new InvalidDataException("Versione manifest non valida.");
        if (manifest.Targets is null || manifest.Targets.Count != 3)
            throw new InvalidDataException("Il manifest deve contenere esattamente tre moduli.");
        var roles = new HashSet<string>(StringComparer.Ordinal);
        foreach (var target in manifest.Targets)
        {
            ValidateTarget(target);
            if (!roles.Add(target.Id))
                throw new InvalidDataException("Manifest: ruolo MCU duplicato.");
            string expected = $"https://github.com/AriotaG/AlfaRaceX-Framework/releases/download/{manifest.Version}/AlfaRaceX-{target.Id}.hex";
            if (!string.Equals(target.Url, expected, StringComparison.Ordinal))
                throw new InvalidDataException("Manifest: URL non coerente con release e MCU.");
        }
    }

    internal static void ValidateTarget(FirmwareTarget target)
    {
        if (target is null || target.Id is not ("C1" or "C2" or "BH"))
            throw new InvalidDataException("Manifest: ruolo MCU sconosciuto.");
        uint limit = target.Id == "C1" ? 0x08018000u : 0x0800F000u;
        if (target.ApplicationStart != 0x08000000u || target.ApplicationLimitExclusive != limit)
            throw new InvalidDataException("Manifest: area firmware incompatibile con il target; pagine riservate protette.");
        if (target.Sha256 is null || !System.Text.RegularExpressions.Regex.IsMatch(target.Sha256, @"\A[0-9a-fA-F]{64}\z"))
            throw new InvalidDataException("Manifest: SHA-256 non valido.");
        if (!Uri.TryCreate(target.Url, UriKind.Absolute, out var uri) ||
            uri.Scheme != "https" || uri.Host != "github.com" || !uri.IsDefaultPort ||
            uri.UserInfo.Length != 0 || uri.Query.Length != 0 || uri.Fragment.Length != 0 ||
            !uri.AbsolutePath.StartsWith("/AriotaG/AlfaRaceX-Framework/releases/download/", StringComparison.Ordinal) ||
            !uri.AbsolutePath.EndsWith($"/AlfaRaceX-{target.Id}.hex", StringComparison.Ordinal))
            throw new InvalidDataException("Manifest: URL firmware non autorizzato.");
    }

    public async Task<string> DownloadVerifiedAsync(
        FirmwareTarget target,
        string directory,
        IProgress<int>? progress,
        CancellationToken ct)
    {
        ValidateTarget(target);
        Directory.CreateDirectory(directory);
        string path = Path.Combine(directory, $"AlfaRaceX-{target.Id}-{Guid.NewGuid():N}.hex");

        using var timeout = CancellationTokenSource.CreateLinkedTokenSource(ct);
        timeout.CancelAfter(TimeSpan.FromMinutes(2));
        ct = timeout.Token;
        bool complete = false;
        bool created = false;
        try
        {
            using var response = await _http.GetAsync(
                target.Url,
                HttpCompletionOption.ResponseHeadersRead,
                ct);
            response.EnsureSuccessStatusCode();

            long total = response.Content.Headers.ContentLength ?? -1;
            const long maxHexBytes = 1024 * 1024;
            if (total > maxHexBytes) throw new InvalidDataException("Firmware HEX troppo grande.");
            long readTotal = 0;
            byte[] buffer = new byte[64 * 1024];

            await using (var input = await response.Content.ReadAsStreamAsync(ct))
            await using (var output = new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.None))
            {
                created = true;
                while (true)
                {
                    int read = await input.ReadAsync(buffer, ct);
                    if (read == 0) break;
                    await output.WriteAsync(buffer.AsMemory(0, read), ct);
                    readTotal += read;
                    if (readTotal > maxHexBytes) throw new InvalidDataException("Firmware HEX troppo grande.");
                    if (total > 0)
                        progress?.Report((int)Math.Clamp(readTotal * 100L / total, 0, 100));
                }
            }

            string actual;
            await using (var file = File.OpenRead(path))
            {
                actual = Convert.ToHexString(await SHA256.HashDataAsync(file, ct)).ToLowerInvariant();
            }

            if (!string.Equals(actual, target.Sha256, StringComparison.OrdinalIgnoreCase))
            {
                throw new InvalidDataException(
                    $"SHA-256 non valido per {target.Label}. Download rifiutato.");
            }

            progress?.Report(100);
            complete = true;
            return path;
        }
        finally
        {
            if (!complete && created && File.Exists(path)) File.Delete(path);
        }
    }
}
