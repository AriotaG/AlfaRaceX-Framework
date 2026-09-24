using System.Security.Cryptography;
using System.Text.Json;

namespace AlfaRaceX.Updater;

internal sealed class ManifestClient
{
    private readonly HttpClient _http = new()
    {
        Timeout = TimeSpan.FromSeconds(30)
    };

    public ManifestClient()
    {
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

        if (!string.Equals(manifest.Product, AppConstants.ProductName, StringComparison.Ordinal))
            throw new InvalidDataException("Il manifest non appartiene ad AlfaRaceX.");
        if (manifest.Targets.Count != 3)
            throw new InvalidDataException("Il manifest deve contenere esattamente tre moduli.");

        return manifest;
    }

    public async Task<string> DownloadVerifiedAsync(
        FirmwareTarget target,
        string directory,
        IProgress<int>? progress,
        CancellationToken ct)
    {
        Directory.CreateDirectory(directory);
        string path = Path.Combine(directory, $"AlfaRaceX-{target.Id}.hex");

        using var response = await _http.GetAsync(
            target.Url,
            HttpCompletionOption.ResponseHeadersRead,
            ct);
        response.EnsureSuccessStatusCode();

        long total = response.Content.Headers.ContentLength ?? -1;
        long readTotal = 0;
        byte[] buffer = new byte[64 * 1024];

        await using (var input = await response.Content.ReadAsStreamAsync(ct))
        await using (var output = File.Create(path))
        {
            while (true)
            {
                int read = await input.ReadAsync(buffer, ct);
                if (read == 0) break;
                await output.WriteAsync(buffer.AsMemory(0, read), ct);
                readTotal += read;
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
            File.Delete(path);
            throw new InvalidDataException(
                $"SHA-256 non valido per {target.Label}. Download rifiutato.");
        }

        progress?.Report(100);
        return path;
    }
}
