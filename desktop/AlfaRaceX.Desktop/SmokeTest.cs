namespace AlfaRaceX.Desktop;

internal static class SmokeTest
{
    public static int Run()
    {
        string root = Path.Combine(Path.GetTempPath(), "AlfaRaceX-Smoke", Guid.NewGuid().ToString("N"));
        try
        {
            Directory.CreateDirectory(root);
            foreach (string relative in new[] { "DISCLAIMER.md", "wwwroot/index.html", "wwwroot/js/app.js", "wwwroot/css/app.css", "wwwroot/img/brand.svg", "wwwroot/img/cars.svg", "wwwroot/img/alfa.svg", "wwwroot/vendor/bootstrap/css/bootstrap.min.css", "wwwroot/vendor/bootstrap-icons/font/bootstrap-icons.min.css" })
                if (!File.Exists(Path.Combine(AppContext.BaseDirectory, relative))) throw new FileNotFoundException("Missing packaged asset", relative);
            var repository = new HistoryRepository(Path.Combine(root, "test.db"));
            repository.Initialize();
            repository.SetSetting("DisclaimerVersion", "test-v1");
            repository.SetSetting("DisclaimerAcceptedUtc", DateTime.UtcNow.ToString("O"));
            if (repository.GetSetting("DisclaimerVersion") != "test-v1") throw new InvalidOperationException("Disclaimer persistence failed.");
            repository.SetSetting("DisclaimerVersion", "test-v2");
            if (repository.GetSetting("DisclaimerVersion") != "test-v2") throw new InvalidOperationException("Disclaimer version update failed.");
            repository.AddBackup("BH", Path.Combine(root,"test.bin"), "", new string('a',64), 131072, DateTime.UtcNow, "smoke");
            if (repository.CountBackups() != 1 || repository.GetBackups()[0].Role != "BH") throw new InvalidOperationException("Backup catalog failed.");
            repository.SaveNotes(repository.GetBackups()[0].Id, "Preserve this note");
            if (repository.GetBackups()[0].Notes != "Preserve this note") throw new InvalidOperationException("Backup notes failed.");
            long operation = repository.StartOperation("FLASH", "test-version");
            repository.FinishOperation(operation, "completed");
            repository.AddEvent("ERROR", "SMOKE", "Synthetic test error");
            if (repository.GetEvents()[0].Message != "Synthetic test error") throw new InvalidOperationException("Event persistence failed.");
            repository.ClearEvents();
            if (repository.GetEvents().Count != 0) throw new InvalidOperationException("Clear logs failed.");
            if (repository.GetOperations().Count != 1) throw new InvalidOperationException("Operation history lost after log cleanup.");
            Console.WriteLine("ALFARACEX_SMOKE_OK");
            return 0;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine(ex);
            return 2;
        }
        finally
        {
            Microsoft.Data.Sqlite.SqliteConnection.ClearAllPools();
            if (Directory.Exists(root)) Directory.Delete(root, true);
        }
    }
}
