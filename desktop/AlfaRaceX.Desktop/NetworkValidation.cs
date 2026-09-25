using System.Text.Json;

namespace AlfaRaceX.Desktop;

internal static class NetworkValidation
{
    public static async Task<int> RunAsync()
    {
        DesktopPaths.ValidationRoot = Path.Combine(Path.GetTempPath(), "AlfaRaceX-Network-Test");
        DesktopPaths.Ensure();
        try
        {
            using var timeout = new CancellationTokenSource(TimeSpan.FromMinutes(3));
            var client = new DesktopUpdateCoordinator();
            var manifest = await client.LoadManifestAsync(timeout.Token);
            var files = await client.PrepareAsync(manifest, new Progress<(string Message, int Progress)>(), timeout.Token);
            if (files.Count != 3) throw new Exception("Missing downloaded firmware.");
            var offline = new DesktopUpdateCoordinator(new HttpClient(new OfflineHandler()));
            try { await offline.LoadManifestAsync(timeout.Token); throw new Exception("Offline error was ignored."); }
            catch (HttpRequestException) { }
            using var cancelled = new CancellationTokenSource();
            cancelled.Cancel();
            try { await client.LoadManifestAsync(cancelled.Token); throw new Exception("Cancellation was ignored."); }
            catch (OperationCanceledException) { }
            File.WriteAllText(Path.Combine(DesktopPaths.Root, "network-validation.json"), JsonSerializer.Serialize(new
            {
                passed = true, version = manifest.Version, checks = new[] { "GitHub release assets", "Real download", "SHA-256", "HEX address validation", "Offline failure", "Cancellation" },
                files = files.Select(f => new { role = f.Target.Id, sha256 = f.Target.Sha256, bytes = new FileInfo(f.LocalPath).Length })
            }));
            return 0;
        }
        catch (Exception ex)
        {
            File.WriteAllText(Path.Combine(DesktopPaths.Root, "network-validation.json"), JsonSerializer.Serialize(new { passed = false, error = ex.ToString() }));
            return 5;
        }
    }
    private sealed class OfflineHandler : HttpMessageHandler
    {
        protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken cancellationToken)
            => throw new HttpRequestException("Synthetic offline condition for test only.");
    }
}
