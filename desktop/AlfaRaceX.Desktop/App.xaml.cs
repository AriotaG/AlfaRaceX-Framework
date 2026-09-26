using System.Windows;
using Velopack;

namespace AlfaRaceX.Desktop;

public partial class App : Application
{
    [STAThread]
    private static int Main(string[] args)
    {
        VelopackApp.Build().Run();

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
        if (smoke) return SmokeTest.Run();

        using var instance = new Mutex(true,
            uiSmoke ? "Local\\AlfaRaceX.Smoke." + Guid.NewGuid().ToString("N") : "Local\\AlfaRaceX.Desktop",
            out bool ownsInstance);
        if (!ownsInstance) return 3;
        DesktopPaths.Ensure();
        var app = new App();
        app.InitializeComponent();
        var window = new MainWindow();
        if (uiSmoke)
        {
            window.ShowInTaskbar = false;
            window.WindowStartupLocation = WindowStartupLocation.Manual;
            window.Left = -20000;
            window.Loaded += async (_, _) => app.Shutdown(await SmokeTest.RunUiAsync(window));
        }
        return app.Run(window);
    }
}
