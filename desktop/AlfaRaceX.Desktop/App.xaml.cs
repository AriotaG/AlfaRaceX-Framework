using System.Windows;
using Velopack;

namespace AlfaRaceX.Desktop;

public partial class App : Application
{
    [STAThread]
    private static int Main(string[] args)
    {
        VelopackApp.Build().Run();

        if (args.Any(a => string.Equals(a, "--smoke-test", StringComparison.OrdinalIgnoreCase)))
            return SmokeTest.Run();

        DesktopPaths.Ensure();
        var app = new App();
        app.InitializeComponent();
        app.Run(new MainWindow());
        return 0;
    }
}