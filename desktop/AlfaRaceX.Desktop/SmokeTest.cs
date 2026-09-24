namespace AlfaRaceX.Desktop;

internal static class SmokeTest
{
    public static int Run()
    {
        try
        {
            DesktopPaths.Ensure();
            var repository = new HistoryRepository(DesktopPaths.Database);
            repository.Initialize();
            repository.AddEvent("INFO", "SMOKE", "AlfaRaceX Desktop smoke test completato.");

            if (!File.Exists(DesktopPaths.Database))
                throw new InvalidOperationException("Database SQLite non creato.");

            Console.WriteLine("ALFARACEX_SMOKE_OK");
            return 0;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine(ex);
            return 2;
        }
    }
}