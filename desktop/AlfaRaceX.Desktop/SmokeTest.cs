namespace AlfaRaceX.Desktop;

internal static class SmokeTest
{
    [System.Runtime.InteropServices.DllImport("user32.dll")]
    private static extern IntPtr SendMessage(IntPtr hwnd, int message, IntPtr wParam, IntPtr lParam);
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
            // Real UI -> backend -> release download -> HEX/hash validation. Never flash a device.
            await window.Browser.ExecuteScriptAsync("document.getElementById('prepareBtn').click()");
            deadline = DateTime.UtcNow.AddSeconds(60);
            while (await window.Browser.ExecuteScriptAsync("document.querySelectorAll('.target-card.prepared').length === 3 && !document.getElementById('prepareBtn').disabled") != "true")
            {
                if (DateTime.UtcNow > deadline) throw new TimeoutException("Preparazione firmware reale o riattivazione dei controlli non completata.");
                await Task.Delay(250);
            }
            foreach (string page in new[] { "dashboard", "update", "backup", "restore", "logs", "info" })
            {
                await window.Browser.ExecuteScriptAsync($"document.querySelector('.nav-item[data-page={page}]').click()");
                await AssertJs($"document.getElementById('page-{page}').classList.contains('active')", "navigazione " + page);
                await Task.Delay(100);
                await using var screenshot = File.Create(Path.Combine(DesktopPaths.Root, $"ui-{page}.png"));
                await window.Browser.CoreWebView2.CapturePreviewAsync(
                    Microsoft.Web.WebView2.Core.CoreWebView2CapturePreviewImageFormat.Png, screenshot);
            }
            // Inject a Windows notification burst; enumeration remains real and read-only.
            // This verifies the OS message hook/debounce, not physical hotplug or role detection.
            await window.Browser.ExecuteScriptAsync("window.__deviceRefreshCount=0; window.chrome.webview.addEventListener('message',e=>{if(e.data.type==='dashboard')window.__deviceRefreshCount++;});");
            var hwnd = new System.Windows.Interop.WindowInteropHelper(window).Handle;
            for (int i = 0; i < 10; i++) SendMessage(hwnd, 0x0219, new IntPtr(0x0007), IntPtr.Zero);
            deadline = DateTime.UtcNow.AddSeconds(5);
            while (await window.Browser.ExecuteScriptAsync("window.__deviceRefreshCount === 1") != "true")
            {
                if (DateTime.UtcNow > deadline) throw new TimeoutException("Aggiornamento USB dopo notifica Windows non ricevuto.");
                await Task.Delay(100);
            }
            await Task.Delay(750);
            await AssertJs("window.__deviceRefreshCount === 1", "notifiche USB duplicate aggregate");
            // Internal UI fixtures only; never issue a hardware command.
            foreach (int? count in new int?[] { null, 0, 2 })
            {
                window.Post("dashboard", new { dfuCount = count, deviceError = count is null ? "test enumeration failure" : null });
                await Task.Delay(100);
                await AssertJs("document.getElementById('backupBtn').disabled && document.querySelectorAll('.flash-btn:not(:disabled),.restore:not(:disabled)').length === 0", "operazioni bloccate senza un singolo DFU");
            }
            SendMessage(hwnd, 0x0219, new IntPtr(0x0007), IntPtr.Zero); // Restore real enumeration.
            await AssertJs("document.styleSheets.length >= 2 && typeof bootstrap === 'object'", "risorse Bootstrap locali");
            File.WriteAllText(Path.Combine(DesktopPaths.Root, "ui-smoke-result.txt"), "PASS: WebView2, worker bridge, disclaimer persistito, preparazione reale dei tre firmware, controlli riattivati, sei viste, Bootstrap locale, notifica Windows USB e debounce con enumerazione reale (senza hotplug fisico).");
            return 0;
        }
        catch (Exception ex)
        {
            File.WriteAllText(Path.Combine(DesktopPaths.Root, "ui-smoke-result.txt"), ex.ToString());
            return 2;
        }
    }
}
