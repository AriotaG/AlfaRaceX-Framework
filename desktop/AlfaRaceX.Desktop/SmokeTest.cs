namespace AlfaRaceX.Desktop;

internal static class SmokeTest
{
    public static int Run()
    {
        try
        {
            DesktopPaths.Ensure();
            var repository = new HistoryRepository(DesktopPaths.Database);
            repository.Initialize();

            if (!File.Exists(DesktopPaths.Database))
                throw new InvalidOperationException("Database SQLite non creato.");
            foreach (string asset in new[] { "index.html", "js/app.js", "css/app.css",
                "vendor/bootstrap/css/bootstrap.min.css", "vendor/bootstrap/js/bootstrap.bundle.min.js", "img/arx.svg", "img/hero.svg" })
                if (!File.Exists(Path.Combine(AppContext.BaseDirectory, "wwwroot", asset)))
                    throw new FileNotFoundException("Risorsa UI mancante: " + asset);
            _ = Microsoft.Web.WebView2.Core.CoreWebView2Environment.GetAvailableBrowserVersionString();
            repository.SetSetting("smoke", "persisted");
            if (new HistoryRepository(DesktopPaths.Database).GetSetting("smoke") != "persisted")
                throw new InvalidOperationException("Persistenza SQLite non verificata.");

            repository.AddEvent("INFO", "SMOKE", "AlfaRaceX Desktop smoke test completato.");
            Console.WriteLine("ALFARACEX_SMOKE_OK");
            return 0;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine(ex);
            return 2;
        }
    }

    public static async Task<int> RunUiAsync(MainWindow window)
    {
        try
        {
            if (Run() != 0) throw new InvalidOperationException("Prerequisiti UI smoke non verificati; consultare stderr.");
            var deadline = DateTime.UtcNow.AddSeconds(30);
            while (window.Browser.CoreWebView2 is null ||
                await window.Browser.ExecuteScriptAsync("document.getElementById('infoVersion')?.textContent?.startsWith('v') === true") != "true")
            {
                if (DateTime.UtcNow > deadline) throw new TimeoutException("Initialisation WebView2/bridge non completata.");
                await Task.Delay(250);
            }
            async Task AssertJs(string expression, string label)
            {
                if (await window.Browser.ExecuteScriptAsync(expression) != "true")
                    throw new InvalidOperationException("UI smoke: " + label);
            }
            await AssertJs("document.getElementById('disclaimerGate').hidden === false", "disclaimer primo avvio");
            await window.Browser.ExecuteScriptAsync("window.__smokeProbe = false; window.chrome.webview.addEventListener('message', e => { if (e.data.type === 'smokeProbe') window.__smokeProbe = e.data.data.value; });");
            await Task.Run(() => window.Post("smokeProbe", new { value = true }));
            deadline = DateTime.UtcNow.AddSeconds(5);
            while (await window.Browser.ExecuteScriptAsync("window.__smokeProbe") != "true")
            {
                if (DateTime.UtcNow > deadline) throw new TimeoutException("Messaggio dal worker non ricevuto dalla UI.");
                await Task.Delay(100);
            }
            await window.Browser.ExecuteScriptAsync("document.getElementById('disclaimerAcceptBtn').click()");
            deadline = DateTime.UtcNow.AddSeconds(5);
            while (await window.Browser.ExecuteScriptAsync("document.getElementById('disclaimerGate').hidden") != "true")
            {
                if (DateTime.UtcNow > deadline) throw new TimeoutException("Accettazione disclaimer non ricevuta dal backend.");
                await Task.Delay(100);
            }
            var repository = new HistoryRepository(DesktopPaths.Database);
            if (string.IsNullOrEmpty(repository.GetSetting("DisclaimerAcceptedUtc")))
                throw new InvalidOperationException("Accettazione disclaimer non persistita.");
            foreach (string page in new[] { "dashboard", "update", "backup", "restore", "logs", "info" })
            {
                await window.Browser.ExecuteScriptAsync($"document.querySelector('.nav-item[data-page={page}]').click()");
                await AssertJs($"document.getElementById('page-{page}').classList.contains('active')", "navigazione " + page);
                await Task.Delay(100);
                await using var screenshot = File.Create(Path.Combine(DesktopPaths.Root, $"ui-{page}.png"));
                await window.Browser.CoreWebView2.CapturePreviewAsync(
                    Microsoft.Web.WebView2.Core.CoreWebView2CapturePreviewImageFormat.Png, screenshot);
            }
            await AssertJs("document.styleSheets.length >= 2 && typeof bootstrap === 'object'", "risorse Bootstrap locali");
            File.WriteAllText(Path.Combine(DesktopPaths.Root, "ui-smoke-result.txt"), "PASS: WebView2, bridge, disclaimer persistito, sei viste, Bootstrap locale.");
            return 0;
        }
        catch (Exception ex)
        {
            File.WriteAllText(Path.Combine(DesktopPaths.Root, "ui-smoke-result.txt"), ex.ToString());
            return 2;
        }
    }
}
