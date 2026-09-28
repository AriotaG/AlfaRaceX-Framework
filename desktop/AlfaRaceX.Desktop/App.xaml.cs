using System.Windows;
using Velopack;

namespace AlfaRaceX.Desktop;

public partial class App : Application
{
    [STAThread]
    private static int Main(string[] args)
    {
        try { return RunApplication(args); }
        catch (Exception ex)
        {
            StartupLog.Write("FATAL_STARTUP", ex);
            if (!args.Any(a => a.StartsWith("--smoke", StringComparison.OrdinalIgnoreCase) || a == "--ui-smoke-test"))
                MessageBox.Show($"Avvio AlfaRaceX non riuscito.\n\n{ex.Message}\n\nLog: {StartupLog.FilePath}",
                    "AlfaRaceX", MessageBoxButton.OK, MessageBoxImage.Error);
            return 2;
        }
    }

    internal static string InstanceKey { get; private set; } = "Local\\AlfaRaceX.Desktop";

    private static int RunApplication(string[] args)
    {

        bool smoke = args.Contains("--smoke-test", StringComparer.OrdinalIgnoreCase);
        bool uiSmoke = args.Contains("--ui-smoke-test", StringComparer.OrdinalIgnoreCase);
        if (smoke || uiSmoke)
        {
            string? root = args.FirstOrDefault(a => a.StartsWith("--smoke-root=", StringComparison.Ordinal));
            DesktopPaths.TestRoot = root is null
                ? Path.Combine(Path.GetTempPath(), "AlfaRaceX-Smoke-" + Guid.NewGuid().ToString("N"))
                : Path.GetFullPath(root["--smoke-root=".Length..]);
            if (Directory.Exists(DesktopPaths.TestRoot) || File.Exists(DesktopPaths.TestRoot)) return 3;
        }
        StartupLog.Write($"START version={typeof(App).Assembly.GetName().Version} executable={Environment.ProcessPath}");
        StartupLog.Write("CONTEXT " + System.Text.Json.JsonSerializer.Serialize(new
        {
            processId = Environment.ProcessId,
            workingDirectory = Environment.CurrentDirectory,
            commandLine = Environment.CommandLine,
            baseDirectory = AppContext.BaseDirectory,
            dataRoot = DesktopPaths.Root,
            database = DesktopPaths.Database,
            backups = DesktopPaths.Backups,
            webView2 = DesktopPaths.WebView2
        }));
        AppDomain.CurrentDomain.UnhandledException += (_, e) =>
            StartupLog.Write("UNHANDLED", e.ExceptionObject as Exception);
        VelopackApp.Build().Run();
        if (smoke) return SmokeTest.Run();

        if (uiSmoke)
        {
            string? testKey = args.FirstOrDefault(a => a.StartsWith("--instance-key=", StringComparison.Ordinal));
            string id = testKey is null ? Guid.NewGuid().ToString("N") : Guid.Parse(testKey["--instance-key=".Length..]).ToString("N");
            InstanceKey = "Local\\AlfaRaceX.Smoke." + id;
        }
        using var activation = new EventWaitHandle(false, EventResetMode.AutoReset, InstanceKey + ".Activate");
        using var instance = new Mutex(true, InstanceKey, out bool ownsInstance);
        if (!ownsInstance)
        {
            activation.Set();
            StartupLog.Write("ACTIVATION_FOR_EXISTING_INSTANCE");
            return 0;
        }
        StartupLog.Write("ENSURE_DATA_DIRECTORIES");
        DesktopPaths.Ensure();
        StartupLog.Write("DATA_DIRECTORIES_READY");
        var app = new App();
        app.DispatcherUnhandledException += (_, e) => StartupLog.Write("UI_UNHANDLED", e.Exception);
        app.InitializeComponent();
        StartupLog.Write("CREATE_MAIN_WINDOW");
        var window = new MainWindow();
        var activationWait = ThreadPool.RegisterWaitForSingleObject(activation, (_, _) =>
        {
            if (app.Dispatcher.HasShutdownStarted) return;
            app.Dispatcher.BeginInvoke(() =>
            {
                if (app.Dispatcher.HasShutdownStarted) return;
                StartupLog.Write("RESTORE_EXISTING_WINDOW");
                window.Show();
                if (window.WindowState == WindowState.Minimized) window.WindowState = WindowState.Normal;
                bool activated = window.Activate();
                StartupLog.Write($"WINDOW_ACTIVATED={activated}");
            });
        }, null, Timeout.Infinite, executeOnlyOnce: false);
        if (uiSmoke)
        {
            window.ShowInTaskbar = false;
            window.WindowStartupLocation = WindowStartupLocation.Manual;
            window.Left = -20000;
            window.Loaded += async (_, _) => app.Shutdown(await SmokeTest.RunUiAsync(window));
        }
        try
        {
            StartupLog.Write("RUN_UI");
            return app.Run(window);
        }
        finally
        {
            activationWait.Unregister(null);
            StartupLog.Write("EXIT");
        }
    }
}
