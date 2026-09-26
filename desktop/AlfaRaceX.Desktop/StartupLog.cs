using System.Diagnostics;

namespace AlfaRaceX.Desktop;

internal static class StartupLog
{
    private static readonly object Sync = new();
    internal static string FilePath => Path.Combine(DesktopPaths.Logs, $"startup-{DateTime.UtcNow:yyyyMMdd}-{Environment.ProcessId}.log");

    internal static void Write(string phase, Exception? error = null)
    {
        string entry = $"{DateTime.UtcNow:O} [{phase}] {error}\n";
        try
        {
            lock (Sync)
            {
                Directory.CreateDirectory(DesktopPaths.Logs);
                File.AppendAllText(FilePath, entry);
            }
        }
        catch (Exception logError) when (logError is IOException or UnauthorizedAccessException)
        {
            // A logging failure must still be visible through stderr/debug diagnostics.
            Console.Error.WriteLine($"Startup log unavailable: {FilePath}: {logError.Message}\n{entry}");
            Trace.TraceError($"Startup log unavailable: {logError}\n{entry}");
        }
    }
}
