using System.Globalization;

namespace AlfaRaceX.Updater;

internal sealed record AppLogEntry(DateTime Timestamp, string Level, string Message);

internal static class AppLog
{
    private static readonly object Sync = new();

    public static string LogDirectory =>
        Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "AlfaRaceX",
            "Updater",
            "Logs");

    public static string CurrentFile =>
        Path.Combine(LogDirectory, $"updater-{DateTime.Now:yyyyMMdd}.log");

    public static void Write(string level, string message)
    {
        level = string.IsNullOrWhiteSpace(level) ? "INFO" : level.Trim().ToUpperInvariant();
        message = (message ?? string.Empty).Replace("\r", " ").Replace("\n", " ");

        lock (Sync)
        {
            Directory.CreateDirectory(LogDirectory);
            File.AppendAllText(
                CurrentFile,
                $"{DateTime.Now:O}\t{level}\t{message}{Environment.NewLine}");
        }
    }

    public static IReadOnlyList<AppLogEntry> ReadRecent(int maxEntries = 400)
    {
        lock (Sync)
        {
            if (!File.Exists(CurrentFile))
                return Array.Empty<AppLogEntry>();

            string[] lines = File.ReadAllLines(CurrentFile);
            int start = Math.Max(0, lines.Length - maxEntries);
            var result = new List<AppLogEntry>(lines.Length - start);

            for (int i = start; i < lines.Length; i++)
            {
                string[] parts = lines[i].Split('\t', 3);
                if (parts.Length != 3) continue;

                if (!DateTime.TryParse(
                    parts[0],
                    CultureInfo.InvariantCulture,
                    DateTimeStyles.RoundtripKind,
                    out DateTime ts))
                    continue;

                result.Add(new AppLogEntry(ts, parts[1], parts[2]));
            }

            return result;
        }
    }

    public static void ClearCurrent()
    {
        lock (Sync)
        {
            Directory.CreateDirectory(LogDirectory);
            File.WriteAllText(CurrentFile, string.Empty);
        }
    }

    public static void ExportCurrent(string destination)
    {
        lock (Sync)
        {
            Directory.CreateDirectory(Path.GetDirectoryName(destination) ?? ".");
            if (File.Exists(CurrentFile))
                File.Copy(CurrentFile, destination, true);
            else
                File.WriteAllText(destination, string.Empty);
        }
    }
}
