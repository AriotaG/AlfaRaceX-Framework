using System.Windows;


namespace AlfaRaceX.Desktop;

public partial class App : Application
{
    [STAThread]
    private static int Main(string[] args)
    {


        if (args.Any(a => string.Equals(a, "--smoke-test", StringComparison.OrdinalIgnoreCase)))
            return SmokeTest.Run();

        if (args.Contains("--ui-validation"))
        {
            UiValidation.Enabled = true;
            UiValidation.Stage = args.Contains("--subsequent") ? "subsequent" : "first";
            DesktopPaths.ValidationRoot = Environment.GetEnvironmentVariable("ALFARACEX_UI_TEST_ROOT")
                ?? Path.Combine(Path.GetTempPath(), "AlfaRaceX-UI-Validation");
        }
        DesktopPaths.Ensure();
        var app = new App();
        app.InitializeComponent();
        app.Run(new MainWindow());
        return 0;
    }
}