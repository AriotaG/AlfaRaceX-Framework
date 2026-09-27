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
            var workArea = System.Windows.SystemParameters.WorkArea;
            if (window.Width > workArea.Width || window.Height > workArea.Height ||
                window.MinWidth > workArea.Width || window.MinHeight > workArea.Height)
                throw new InvalidOperationException("Dimensioni iniziali finestra fuori dall'area di lavoro.");
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
            // Exercise the real rejection path before acceptance, without invoking hardware.
            await window.Browser.ExecuteScriptAsync("window.__bridgeRejections=0; window.chrome.webview.addEventListener('message',e=>{if(e.data.type==='log' && e.data.data.category==='BRIDGE')window.__bridgeRejections++;}); document.querySelector('.nav-item[data-page=logs]').click();");
            deadline = DateTime.UtcNow.AddSeconds(5);
            while (await window.Browser.ExecuteScriptAsync("window.__bridgeRejections >= 1") != "true")
            {
                if (DateTime.UtcNow > deadline) throw new TimeoutException("Rifiuto richiesta prima del disclaimer non ricevuto.");
                await Task.Delay(100);
            }
            await Task.Delay(750);
            await AssertJs("window.__bridgeRejections === 1 && document.getElementById('logList').textContent.includes('disclaimer')", "nessun ciclo di richieste causato dal log di rifiuto");
            // Test-only hostile text and burst: verify escaping, newest-first order and bounded DOM.
            for (int i = 0; i < 505; i++)
                window.Post("log", new { createdUtc = DateTime.UtcNow, level = "INFO", category = "SMOKE", message = $"<img src=x onerror=alert(1)> fixture {i}" });
            deadline = DateTime.UtcNow.AddSeconds(5);
            while (await window.Browser.ExecuteScriptAsync("document.getElementById('logList').firstElementChild?.textContent.includes('fixture 504') === true") != "true")
            {
                if (DateTime.UtcNow > deadline) throw new TimeoutException("Flusso log UI non completato.");
                await Task.Delay(100);
            }
            await AssertJs("document.getElementById('logList').children.length === 500 && !document.querySelector('#logList img') && document.getElementById('logList').lastElementChild.textContent.includes('fixture 5')", "log live limitati e testo HTML innocuo");
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
            // Explicit test-only device transport: exercises UI -> service -> protocol -> reply.
            // The real firmware half of this contract is covered by test_pedal_host.
            var pedalFixture = new PedalFixture();
            var originalFactory = window.PedalTransportFactory;
            try
            {
                window.PedalTransportFactory = _ => pedalFixture;
                await window.Browser.ExecuteScriptAsync("window.__pedalPortsReceived=false; window.chrome.webview.addEventListener('message',e=>{if(e.data.type==='pedalPorts')window.__pedalPortsReceived=true;}); document.querySelector('.nav-item[data-page=pedal]').click();");
                deadline = DateTime.UtcNow.AddSeconds(5);
                while (await window.Browser.ExecuteScriptAsync("window.__pedalPortsReceived") != "true")
                {
                    if (DateTime.UtcNow > deadline) throw new TimeoutException("Enumerazione porte PedalRaceX non completata.");
                    await Task.Delay(100);
                }
                await window.Browser.ExecuteScriptAsync("document.getElementById('pedalPort').add(new Option('TEST ONLY','TEST-ONLY')); document.getElementById('pedalPort').value='TEST-ONLY'; document.getElementById('pedalPort').dispatchEvent(new Event('change')); document.getElementById('pedalReadBtn').click();");
                deadline = DateTime.UtcNow.AddSeconds(5);
                while (await window.Browser.ExecuteScriptAsync("!document.getElementById('pedalApplyBtn').disabled") != "true")
                {
                    if (DateTime.UtcNow > deadline) throw new TimeoutException("Lettura fixture PedalRaceX non completata.");
                    await Task.Delay(100);
                }
                await AssertJs("document.getElementById('pedalMode').options.length===9 && document.getElementById('pedalApplied').textContent==='Non confermata'", "modalità PedalRaceX e nessuna conferma inventata");
                await window.Browser.ExecuteScriptAsync("document.getElementById('pedalMode').value='5'; document.getElementById('pedalPower').value='2'; window.__savedConfirm=window.confirm; window.confirm=()=>true; document.getElementById('pedalApplyBtn').click(); window.confirm=window.__savedConfirm; delete window.__savedConfirm;");
                deadline = DateTime.UtcNow.AddSeconds(5);
                while (await window.Browser.ExecuteScriptAsync("document.getElementById('pedalComm').textContent==='In attesa' && !document.getElementById('pedalReadBtn').disabled") != "true")
                {
                    if (DateTime.UtcNow > deadline) throw new TimeoutException("Applicazione PedalRaceX non riporta attesa hardware.");
                    await Task.Delay(100);
                }
                if (pedalFixture.LastWrite != "AT@PRX=05,0C") throw new InvalidOperationException("UI PedalRaceX ha alterato modo/potenza.");
                await window.Browser.ExecuteScriptAsync("document.getElementById('pedalReadBtn').click()");
                deadline = DateTime.UtcNow.AddSeconds(5);
                while (await window.Browser.ExecuteScriptAsync("document.getElementById('pedalComm').textContent==='Mappa confermata' && !document.getElementById('pedalReadBtn').disabled") != "true")
                {
                    if (DateTime.UtcNow > deadline) throw new TimeoutException("Risposta PedalRaceX non raggiunge la UI.");
                    await Task.Delay(100);
                }
                await window.Browser.ExecuteScriptAsync("document.getElementById('pedalMode').scrollIntoView({block:'center'})");
                await AssertJs("document.documentElement.scrollWidth<=window.innerWidth && ['pedalMode','pedalPower','pedalApplyBtn'].every(id=>{const r=document.getElementById(id).getBoundingClientRect();return r.width>0&&r.left>=0&&r.right<=window.innerWidth&&r.top>=0&&r.bottom<=window.innerHeight;})", "controlli PedalRaceX visibili senza overflow orizzontale");
                await using (var screenshot = File.Create(Path.Combine(DesktopPaths.Root, "ui-pedal-confirmed-test-only.png")))
                    await window.Browser.CoreWebView2.CapturePreviewAsync(Microsoft.Web.WebView2.Core.CoreWebView2CapturePreviewImageFormat.Png, screenshot);
                await window.Browser.ExecuteScriptAsync("window.scrollTo(0,0)");
                pedalFixture.Fail = true;
                await window.Browser.ExecuteScriptAsync("document.getElementById('pedalReadBtn').click()");
                deadline = DateTime.UtcNow.AddSeconds(5);
                while (await window.Browser.ExecuteScriptAsync("document.getElementById('pedalComm').textContent==='Non verificata' && !document.getElementById('pedalReadBtn').disabled") != "true")
                {
                    if (DateTime.UtcNow > deadline) throw new TimeoutException("Errore PedalRaceX non invalida stato e controlli.");
                    await Task.Delay(100);
                }
                await AssertJs("document.getElementById('pedalApplyBtn').disabled", "scrittura PedalRaceX bloccata dopo disconnessione");
            }
            finally { window.PedalTransportFactory = originalFactory; }
            // Real UI -> backend -> release download -> HEX/hash validation. Never flash a device.
            await window.Browser.ExecuteScriptAsync("document.getElementById('prepareBtn').click()");
            deadline = DateTime.UtcNow.AddSeconds(60);
            while (await window.Browser.ExecuteScriptAsync("document.querySelectorAll('.target-card.prepared').length === 3 && !document.getElementById('prepareBtn').disabled") != "true")
            {
                if (DateTime.UtcNow > deadline) throw new TimeoutException("Preparazione firmware reale o riattivazione dei controlli non completata.");
                await Task.Delay(250);
            }
            foreach (string page in new[] { "dashboard", "update", "backup", "restore", "logs", "pedal", "info" })
            {
                await window.Browser.ExecuteScriptAsync($"document.querySelector('.nav-item[data-page={page}]').click()");
                await AssertJs($"document.getElementById('page-{page}').classList.contains('active')", "navigazione " + page);
                await Task.Delay(100);
                await using var screenshot = File.Create(Path.Combine(DesktopPaths.Root, $"ui-{page}.png"));
                await window.Browser.CoreWebView2.CapturePreviewAsync(
                    Microsoft.Web.WebView2.Core.CoreWebView2CapturePreviewImageFormat.Png, screenshot);
            }
            await AssertJs("document.querySelectorAll('.operation-card').length === 3 && [...document.querySelectorAll('#operationPercent,[data-operation-percent]')].every(x=>x.textContent==='100%')", "progresso reale visibile in aggiornamento, backup e ripristino");
            await AssertJs("[...document.querySelectorAll('[id]')].map(x=>x.id).length === new Set([...document.querySelectorAll('[id]')].map(x=>x.id)).size", "nessun ID duplicato nei controlli operazione");
            // Internal presentation fixture only; no backup or device operation is simulated as real.
            string detailFixture = "TEST UI - percorso lungo\nFile: C:\\test-only\\" + new string('x', 180) + ".bin\nSHA-256: " + new string('a', 64);
            window.Post("operationProgress", new { message = detailFixture, progress = 100 });
            await window.Browser.ExecuteScriptAsync("document.querySelector('.nav-item[data-page=backup]').click()");
            await Task.Delay(150);
            await AssertJs("document.querySelector('#page-backup [data-operation-text]').textContent.includes('SHA-256:') && document.querySelector('#page-backup [data-operation-text]').scrollWidth <= document.querySelector('#page-backup [data-operation-text]').clientWidth + 1", "esito backup e hash leggibili senza overflow");
            window.Post("operationProgress", new { message = "Firmware scaricati e verificati.", progress = 100 });
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
            window.WindowState = System.Windows.WindowState.Minimized;
            var start = new System.Diagnostics.ProcessStartInfo(Environment.ProcessPath
                ?? throw new InvalidOperationException("Percorso eseguibile non disponibile."))
            {
                UseShellExecute = false,
                CreateNoWindow = true,
                WindowStyle = System.Diagnostics.ProcessWindowStyle.Hidden
            };
            start.ArgumentList.Add("--ui-smoke-test");
            start.ArgumentList.Add("--instance-key=" + App.InstanceKey["Local\\AlfaRaceX.Smoke.".Length..]);
            start.ArgumentList.Add("--smoke-root=" + DesktopPaths.Root + "-second-" + Guid.NewGuid().ToString("N"));
            using (var second = System.Diagnostics.Process.Start(start)
                ?? throw new InvalidOperationException("Seconda istanza non avviata."))
            {
                try { await second.WaitForExitAsync().WaitAsync(TimeSpan.FromSeconds(10)); }
                finally { if (!second.HasExited) second.Kill(entireProcessTree: true); }
                if (second.ExitCode != 0) throw new InvalidOperationException("Seconda istanza non ha notificato la prima.");
            }
            deadline = DateTime.UtcNow.AddSeconds(5);
            while (window.WindowState == System.Windows.WindowState.Minimized)
            {
                if (DateTime.UtcNow > deadline) throw new TimeoutException("Secondo avvio non ha ripristinato la finestra.");
                await Task.Delay(100);
            }
            string startup = File.ReadAllText(StartupLog.FilePath);
            if (!startup.Contains("CREATE_MAIN_WINDOW") || !startup.Contains("WEBVIEW2_READY") || !startup.Contains("RESTORE_EXISTING_WINDOW") ||
                !startup.Contains("CONTEXT ") || !startup.Contains("DATA_DIRECTORIES_READY") ||
                !startup.Contains("DATABASE_READY") || !startup.Contains("RECOVERY_CHECK_COMPLETE"))
                throw new InvalidOperationException("Diagnostica avvio/seconda istanza incompleta.");
            string contextLine = File.ReadLines(StartupLog.FilePath).Single(x => x.Contains("[CONTEXT "));
            int contextStart = contextLine.IndexOf("[CONTEXT ", StringComparison.Ordinal) + "[CONTEXT ".Length;
            using (var context = System.Text.Json.JsonDocument.Parse(contextLine[contextStart..contextLine.LastIndexOf(']')]))
            {
                var root = context.RootElement;
                if (root.GetProperty("processId").GetInt32() != Environment.ProcessId ||
                    root.GetProperty("workingDirectory").GetString() != Environment.CurrentDirectory ||
                    root.GetProperty("commandLine").GetString() != Environment.CommandLine ||
                    root.GetProperty("dataRoot").GetString() != DesktopPaths.Root ||
                    root.GetProperty("database").GetString() != DesktopPaths.Database)
                    throw new InvalidOperationException("Contesto diagnostico diverso dal processo effettivo.");
            }
            await AssertJs("document.styleSheets.length >= 2 && typeof bootstrap === 'object'", "risorse Bootstrap locali");
            File.WriteAllText(Path.Combine(DesktopPaths.Root, "ui-smoke-result.txt"), "PASS: WebView2, worker bridge, disclaimer persistito, PedalRaceX lettura/applicazione/attesa/conferma/disconnessione con trasporto TEST ONLY e controlli visibili, preparazione reale dei tre firmware, controlli DFU, sette viste, Bootstrap locale, notifica Windows USB/debounce con enumerazione reale (senza hotplug fisico), secondo processo ripristina la prima finestra, log di avvio.");
            return 0;
        }
        catch (Exception ex)
        {
            File.WriteAllText(Path.Combine(DesktopPaths.Root, "ui-smoke-result.txt"), ex.ToString());
            return 2;
        }
    }

    private sealed class PedalFixture : IPedalRaceXTransport
    {
        internal string? LastWrite;
        internal bool Fail;
        public string Exchange(string command, CancellationToken ct)
        {
            ct.ThrowIfCancellationRequested();
            if (Fail) throw new IOException("TEST ONLY: dispositivo scollegato.");
            string fields;
            if (command.StartsWith("AT@PRX=", StringComparison.Ordinal)) { LastWrite = command; fields = "050C04000109"; }
            else fields = LastWrite is null ? "000A01000009" : "050C04040209";
            return "ARXPRX1:C1:" + fields + "00000001000000010000000000000008\r>";
        }
        public void Dispose() { }
    }
}
