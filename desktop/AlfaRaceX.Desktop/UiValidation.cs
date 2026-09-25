using Microsoft.Web.WebView2.Core;
using System.Text.Json;
using System.Windows;

namespace AlfaRaceX.Desktop;

internal static class UiValidation
{
    public static bool Enabled { get; set; }
    public static string Stage { get; set; } = "first";
    public static string Output => Path.Combine(DesktopPaths.Root, "validation");

    public static async Task RunAsync(MainWindow window, CoreWebView2 web)
    {
        Directory.CreateDirectory(Output);
        var checks = new List<string>();
        async Task Check(string name, string expression, int attempts = 100)
        {
            for (int i = 0; i < attempts; i++)
            {
                if (await web.ExecuteScriptAsync(expression) == "true") { checks.Add(name); return; }
                await Task.Delay(100);
            }
            throw new InvalidOperationException("UI validation failed: " + name);
        }
        async Task Capture(string name)
        {
            await Task.Delay(300);
            await using var output = File.Create(Path.Combine(Output, name + ".png"));
            await web.CapturePreviewAsync(CoreWebView2CapturePreviewImageFormat.Png, output);
        }
        try
        {
            await Check("WebView2 bridge initialized", "document.getElementById('sideVersion')?.textContent === 'v0.2.0'");
            if (Stage == "first")
            {
                await Check("First-run disclaimer visible and shell blocked", "!document.getElementById('disclaimerGate').hidden && document.getElementById('shell').inert");
                await Capture("disclaimer");
                await web.ExecuteScriptAsync("document.getElementById('disclaimerAcceptBtn').click()");
            }
            await Check("Accepted disclaimer persisted and operational UI unlocked", "document.getElementById('disclaimerGate').hidden && !document.getElementById('shell').inert");
            // This observes the real backend response; no device or firmware status is injected.
            await Check("Real GitHub manifest loaded", "document.querySelectorAll('.module-row').length === 3", 600);
            foreach (string page in new[] { "update", "backup", "restore", "logs", "info", "dashboard" })
            {
                await web.ExecuteScriptAsync($"document.querySelector('[data-page={page}]').click()");
                await Check("Navigation " + page, $"document.getElementById('page-{page}').classList.contains('active')");
            }
            window.WindowState = WindowState.Minimized;
            await Task.Delay(200);
            if (window.WindowState != WindowState.Minimized) throw new Exception("Minimize failed");
            window.WindowState = WindowState.Normal;
            window.Width = 1536; window.Height = 864;
            await Capture("dashboard-1536");
            window.Width = 1120; window.Height = 720;
            await Capture("dashboard-1120");
            window.WindowState = WindowState.Maximized;
            await Task.Delay(200);
            if (window.WindowState != WindowState.Maximized) throw new Exception("Maximize failed");
            window.WindowState = WindowState.Normal;
            checks.Add("Native window minimize, maximize, restore and resize");
            File.WriteAllText(Path.Combine(Output, Stage + ".json"), JsonSerializer.Serialize(new { passed = true, checks }));
            Application.Current.Shutdown(0);
        }
        catch (Exception ex)
        {
            File.WriteAllText(Path.Combine(Output, Stage + ".json"), JsonSerializer.Serialize(new { passed = false, checks, error = ex.ToString() }));
            Application.Current.Shutdown(3);
        }
    }
}
