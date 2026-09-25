using AlfaRaceX.Updater;
using System.Security.Cryptography;
using System.Text.Json;

namespace AlfaRaceX.Desktop;

internal sealed class DesktopUpdateCoordinator
{
    private readonly HttpClient _http;
    public IReadOnlyList<string> Releases { get; private set; } = Array.Empty<string>();
    public DesktopUpdateCoordinator(HttpClient? http = null)
    {
        _http = http ?? new HttpClient { Timeout = TimeSpan.FromSeconds(45) };
        _http.DefaultRequestHeaders.UserAgent.ParseAdd("AlfaRaceX-Desktop/0.2.0");
    }

    public async Task<UpdateManifest> LoadManifestAsync(CancellationToken ct)
    {
        var json = await RetryAsync(() => _http.GetStringAsync(AppConstants.DefaultManifestUrl, ct), ct);
        var manifest = JsonSerializer.Deserialize<UpdateManifest>(json) ?? throw new InvalidDataException("Manifest vuoto.");
        if (manifest.Product != "AlfaRaceX" || string.IsNullOrWhiteSpace(manifest.Version) ||
            !manifest.Targets.Select(t => t.Id).Order().SequenceEqual(new[] { "BH", "C1", "C2" }))
            throw new InvalidDataException("Identità o moduli del manifest non validi.");
        foreach (var t in manifest.Targets)
        {
            if (t.Sha256.Length != 64 || !t.Sha256.All(Uri.IsHexDigit) ||
                t.ApplicationStart != 0x08000000u || t.ApplicationLimitExclusive != (t.Id == "C1" ? 0x08018000u : 0x0800F000u) ||
                !Uri.TryCreate(t.Url, UriKind.Absolute, out var uri) || uri.Scheme != "https" || uri.Host != "github.com" ||
                !uri.AbsolutePath.StartsWith("/AriotaG/AlfaRaceX-Framework/releases/download/", StringComparison.Ordinal))
                throw new InvalidDataException($"Metadati non validi per {t.Id}.");
        }
        // The manifest explicitly selects a firmware release; never use /releases/latest (which may be desktop).
        using var release = JsonDocument.Parse(await RetryAsync(() => _http.GetStringAsync(
            "https://api.github.com/repos/AriotaG/AlfaRaceX-Framework/releases/tags/" + Uri.EscapeDataString(manifest.Version), ct), ct));
        var assets = release.RootElement.GetProperty("assets").EnumerateArray().Select(a => a.GetProperty("browser_download_url").GetString()).ToHashSet();
        if (manifest.Targets.Any(t => !assets.Contains(t.Url))) throw new InvalidDataException("Asset firmware mancanti nella release GitHub.");
        using var catalog = JsonDocument.Parse(await RetryAsync(() => _http.GetStringAsync(
            "https://api.github.com/repos/AriotaG/AlfaRaceX-Framework/releases?per_page=100", ct), ct));
        Releases = catalog.RootElement.EnumerateArray().Where(x => !x.GetProperty("draft").GetBoolean())
            .Select(x => x.GetProperty("tag_name").GetString() ?? "").ToArray();
        return manifest;
    }

    public async Task<IReadOnlyList<PreparedFirmware>> PrepareAsync(UpdateManifest manifest,
        IProgress<(string Message, int Progress)> progress, CancellationToken ct)
    {
        string root = Path.Combine(DesktopPaths.Root, "Firmware", Convert.ToHexString(SHA256.HashData(System.Text.Encoding.UTF8.GetBytes(manifest.Version))).ToLowerInvariant());
        Directory.CreateDirectory(root);
        var result = new List<PreparedFirmware>();
        foreach (var t in manifest.Targets)
        {
            string path = Path.Combine(root, $"AlfaRaceX-{t.Id}.hex");
            string temporary = path + ".part";
            try
            {
                await RetryAsync(async () =>
                {
                    using var deadline = CancellationTokenSource.CreateLinkedTokenSource(ct);
                    deadline.CancelAfter(TimeSpan.FromMinutes(2));
                    using var response = await _http.GetAsync(t.Url, HttpCompletionOption.ResponseHeadersRead, deadline.Token);
                    response.EnsureSuccessStatusCode();
                    long? total = response.Content.Headers.ContentLength;
                    await using var input = await response.Content.ReadAsStreamAsync(deadline.Token);
                    await using var output = File.Create(temporary);
                    var buffer = new byte[16384];
                    long done = 0;
                    int read;
                    while ((read = await input.ReadAsync(buffer, deadline.Token)) > 0)
                    {
                        done += read;
                        if (done > 4 * 1024 * 1024) throw new InvalidDataException("Asset firmware oltre il limite previsto.");
                        await output.WriteAsync(buffer.AsMemory(0, read), deadline.Token);
                        progress.Report(($"Download {t.Id}: {done:N0} byte", total > 0 ? (int)Math.Min(100, done * 100 / total.Value) : 0));
                    }
                    return true;
                }, ct);
                await using (var input = File.OpenRead(temporary))
                {
                    string hash = Convert.ToHexString(await SHA256.HashDataAsync(input, ct));
                    if (!hash.Equals(t.Sha256, StringComparison.OrdinalIgnoreCase)) throw new InvalidDataException($"SHA-256 errato per {t.Id}.");
                }
                var image = IntelHexImage.Load(temporary);
                image.ValidateApplicationRange(t.ApplicationStart, t.ApplicationLimitExclusive);
                File.Move(temporary, path, true);
                result.Add(new PreparedFirmware(t, path, image));
            }
            finally { if (File.Exists(temporary)) File.Delete(temporary); }
        }
        return result;
    }

    private static async Task<T> RetryAsync<T>(Func<Task<T>> action, CancellationToken ct)
    {
        for (int attempt = 0; ; attempt++)
        {
            try { return await action(); }
            catch (HttpRequestException ex) when (attempt < 2 && (ex.StatusCode is null || (int)ex.StatusCode >= 500 || (int)ex.StatusCode == 429))
            { await Task.Delay(TimeSpan.FromSeconds(attempt + 1), ct); }
        }
    }
}
