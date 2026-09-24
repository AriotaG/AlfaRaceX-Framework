using System.Text;

namespace AlfaRaceX.Updater;

internal static class Program
{
    private static readonly string StartupLogPath =
        Path.Combine(
            Path.GetDirectoryName(Application.ExecutablePath) ?? AppContext.BaseDirectory,
            "AlfaRaceX-Updater-startup.log");

    [STAThread]
    private static void Main(string[] args)
    {
        try
        {
            SafeStartupLog("START", $"AlfaRaceX Updater {AppConstants.UpdaterVersion}");
            SafeStartupLog("INFO", $"OS: {Environment.OSVersion}");
            SafeStartupLog("INFO", $"64-bit OS: {Environment.Is64BitOperatingSystem}");
            SafeStartupLog("INFO", $"64-bit process: {Environment.Is64BitProcess}");

            ApplicationConfiguration.Initialize();

            if (args.Any(a => string.Equals(
                a,
                "--smoke-test",
                StringComparison.OrdinalIgnoreCase)))
            {
                SafeStartupLog("INFO", "Esecuzione smoke test Windows.");
                using var smokeForm = new MainForm();
                _ = smokeForm.Handle;
                SafeStartupLog("PASS", "Smoke test Windows completato.");
                return;
            }

            Application.SetUnhandledExceptionMode(
                UnhandledExceptionMode.CatchException);

            Application.ThreadException += (_, e) =>
                HandleFatal("Errore UI non gestito", e.Exception);

            AppDomain.CurrentDomain.UnhandledException += (_, e) =>
            {
                Exception ex = e.ExceptionObject as Exception
                    ?? new Exception(e.ExceptionObject?.ToString() ?? "Errore sconosciuto");
                HandleFatal("Errore applicazione non gestito", ex);
            };

            TryShowSplash();

            SafeStartupLog("INFO", "Creazione finestra principale.");
            using var form = new MainForm();
            SafeStartupLog("INFO", "Avvio message loop.");
            Application.Run(form);
            SafeStartupLog("END", "Chiusura normale.");
        }
        catch (Exception ex)
        {
            HandleFatal("Avvio AlfaRaceX Updater non riuscito", ex);
        }
    }

    private static void TryShowSplash()
    {
        try
        {
            SafeStartupLog("INFO", "Apertura splash.");
            using var splash = new SplashForm();
            splash.Show();
            splash.Refresh();

            var until = DateTime.UtcNow.AddMilliseconds(900);
            while (DateTime.UtcNow < until)
            {
                Application.DoEvents();
                Thread.Sleep(15);
            }

            splash.Close();
            SafeStartupLog("INFO", "Splash completato.");
        }
        catch (Exception ex)
        {
            // Lo splash è puramente grafico: non deve mai impedire l'avvio.
            SafeStartupLog(
                "WARN",
                $"Splash ignorato: {ex.GetType().Name}: {ex.Message}");
        }
    }

    private static void HandleFatal(string title, Exception ex)
    {
        SafeStartupLog(
            "FATAL",
            $"{ex.GetType().FullName}: {ex.Message}{Environment.NewLine}{ex.StackTrace}");

        try
        {
            MessageBox.Show(
                $"{title}.\n\n" +
                $"{ex.GetType().Name}: {ex.Message}\n\n" +
                $"È stato creato un log diagnostico:\n{StartupLogPath}",
                "AlfaRaceX Updater",
                MessageBoxButtons.OK,
                MessageBoxIcon.Error);
        }
        catch
        {
            // Last-resort path: logging above is deliberately best-effort.
        }
    }

    private static void SafeStartupLog(string level, string message)
    {
        string line =
            $"{DateTime.Now:yyyy-MM-dd HH:mm:ss.fff}\t{level}\t{message}{Environment.NewLine}";

        try
        {
            File.AppendAllText(
                StartupLogPath,
                line,
                Encoding.UTF8);
            return;
        }
        catch
        {
        }

        try
        {
            string fallbackDir = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "AlfaRaceX",
                "Updater");
            Directory.CreateDirectory(fallbackDir);
            File.AppendAllText(
                Path.Combine(fallbackDir, "startup.log"),
                line,
                Encoding.UTF8);
        }
        catch
        {
            // Nothing else can be done safely during fatal startup handling.
        }
    }
}
