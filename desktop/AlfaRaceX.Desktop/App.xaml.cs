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
            DesktopPaths.TestRoot = Path.Combine(Path.GetTempPath(), "AlfaRaceX-Smoke-" + Guid.NewGuid().ToString("N"));
        }
        if (args.Any(a => string.Equals(a, "--smoke-test", StringComparison.OrdinalIgnoreCase)))
            return SmokeTest.Run();

        using var instance = new Mutex(true,
            uiSmoke ? "Local\\AlfaRaceX.Smoke." + Guid.NewGuid().ToString("N") : "Local\\AlfaRaceX.Desktop",
            out bool ownsInstance);
        if (!ownsInstance)
        {
            MessageBox.Show("AlfaRaceX Desktop è già aperto in questa sessione.", "AlfaRaceX");
            return 3;
        }
        try
        {
            DesktopPaths.Ensure();
            var app = new App();
            app.InitializeComponent();
            if (uiSmoke) SmokeTest.SeedInterruptedOperation();
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
        finally { instance.ReleaseMutex(); }
    }
}
