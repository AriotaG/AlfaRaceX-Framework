namespace AlfaRaceX.Updater;

internal static class Program
{
    [STAThread]
    private static void Main()
    {
        ApplicationConfiguration.Initialize();

        using (var splash = new SplashForm())
        {
            splash.Show();
            splash.Refresh();

            var until = DateTime.UtcNow.AddMilliseconds(1450);
            while (DateTime.UtcNow < until)
            {
                Application.DoEvents();
                Thread.Sleep(15);
            }

            splash.Close();
        }

        Application.Run(new MainForm());
    }
}
