namespace AlfaRaceX.Desktop;

internal static class DesktopPaths
{
    public static string Root => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "AlfaRaceX",
        "Desktop");

    public static string Database => Path.Combine(Root, "alfaracex.db");
    public static string Backups => Path.Combine(Root, "Backups");
    public static string Logs => Path.Combine(Root, "Logs");
    public static string WebView2 => Path.Combine(Root, "WebView2");
    public static string Temp => Path.Combine(Root, "Temp");

    public static void Ensure()
    {
        Directory.CreateDirectory(Root);
        Directory.CreateDirectory(Backups);
        Directory.CreateDirectory(Logs);
        Directory.CreateDirectory(WebView2);
        Directory.CreateDirectory(Temp);
    }
}