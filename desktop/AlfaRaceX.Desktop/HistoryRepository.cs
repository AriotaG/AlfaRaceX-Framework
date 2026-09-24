using Microsoft.Data.Sqlite;

namespace AlfaRaceX.Desktop;

internal sealed record BackupRecord(
    long Id,
    string Role,
    string BinPath,
    string MetadataPath,
    string Sha256,
    long Size,
    DateTime CreatedUtc,
    string Source);

internal sealed record HistoryEvent(
    long Id,
    DateTime CreatedUtc,
    string Level,
    string Category,
    string Message);

internal sealed class HistoryRepository
{
    private readonly string _connectionString;

    public HistoryRepository(string databasePath)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(databasePath)!);
        var cs = new SqliteConnectionStringBuilder
        {
            DataSource = databasePath,
            Mode = SqliteOpenMode.ReadWriteCreate,
            Cache = SqliteCacheMode.Shared
        };
        _connectionString = cs.ToString();
    }

    public void Initialize()
    {
        using var connection = Open();
        using var command = connection.CreateCommand();
        command.CommandText = """
            PRAGMA journal_mode=WAL;
            PRAGMA foreign_keys=ON;

            CREATE TABLE IF NOT EXISTS Backups (
                Id INTEGER PRIMARY KEY AUTOINCREMENT,
                Role TEXT NOT NULL,
                BinPath TEXT NOT NULL,
                MetadataPath TEXT NOT NULL,
                Sha256 TEXT NOT NULL,
                Size INTEGER NOT NULL,
                CreatedUtc TEXT NOT NULL,
                Source TEXT NOT NULL
            );

            CREATE UNIQUE INDEX IF NOT EXISTS IX_Backups_Path
                ON Backups(BinPath);

            CREATE TABLE IF NOT EXISTS Events (
                Id INTEGER PRIMARY KEY AUTOINCREMENT,
                CreatedUtc TEXT NOT NULL,
                Level TEXT NOT NULL,
                Category TEXT NOT NULL,
                Message TEXT NOT NULL
            );

            CREATE INDEX IF NOT EXISTS IX_Events_CreatedUtc
                ON Events(CreatedUtc DESC);

            CREATE TABLE IF NOT EXISTS Settings (
                Key TEXT PRIMARY KEY,
                Value TEXT NOT NULL,
                UpdatedUtc TEXT NOT NULL
            );
            """;
        command.ExecuteNonQuery();
    }

    public void AddEvent(string level, string category, string message)
    {
        level = Normalize(level, "INFO");
        category = Normalize(category, "APP");
        message = (message ?? string.Empty).Trim();

        using var connection = Open();
        using var command = connection.CreateCommand();
        command.CommandText = """
            INSERT INTO Events(CreatedUtc, Level, Category, Message)
            VALUES ($created, $level, $category, $message);
            """;
        command.Parameters.AddWithValue("$created", DateTime.UtcNow.ToString("O"));
        command.Parameters.AddWithValue("$level", level);
        command.Parameters.AddWithValue("$category", category);
        command.Parameters.AddWithValue("$message", message);
        command.ExecuteNonQuery();

        WriteTextLog(level, category, message);
    }

    public void AddBackup(
        string role,
        string binPath,
        string metadataPath,
        string sha256,
        long size,
        DateTime createdUtc,
        string source)
    {
        using var connection = Open();
        using var command = connection.CreateCommand();
        command.CommandText = """
            INSERT INTO Backups(Role, BinPath, MetadataPath, Sha256, Size, CreatedUtc, Source)
            VALUES ($role, $bin, $meta, $sha, $size, $created, $source)
            ON CONFLICT(BinPath) DO UPDATE SET
                Role=excluded.Role,
                MetadataPath=excluded.MetadataPath,
                Sha256=excluded.Sha256,
                Size=excluded.Size,
                CreatedUtc=excluded.CreatedUtc,
                Source=excluded.Source;
            """;
        command.Parameters.AddWithValue("$role", role);
        command.Parameters.AddWithValue("$bin", Path.GetFullPath(binPath));
        command.Parameters.AddWithValue("$meta", string.IsNullOrWhiteSpace(metadataPath) ? string.Empty : Path.GetFullPath(metadataPath));
        command.Parameters.AddWithValue("$sha", sha256);
        command.Parameters.AddWithValue("$size", size);
        command.Parameters.AddWithValue("$created", createdUtc.ToUniversalTime().ToString("O"));
        command.Parameters.AddWithValue("$source", source);
        command.ExecuteNonQuery();
    }

    public IReadOnlyList<BackupRecord> GetBackups()
    {
        using var connection = Open();
        using var command = connection.CreateCommand();
        command.CommandText = """
            SELECT Id, Role, BinPath, MetadataPath, Sha256, Size, CreatedUtc, Source
            FROM Backups
            ORDER BY CreatedUtc DESC;
            """;

        using var reader = command.ExecuteReader();
        var result = new List<BackupRecord>();
        while (reader.Read())
        {
            result.Add(new BackupRecord(
                reader.GetInt64(0),
                reader.GetString(1),
                reader.GetString(2),
                reader.GetString(3),
                reader.GetString(4),
                reader.GetInt64(5),
                DateTime.Parse(reader.GetString(6), null, System.Globalization.DateTimeStyles.RoundtripKind),
                reader.GetString(7)));
        }
        return result;
    }

    public BackupRecord? GetBackup(long id) => GetBackups().FirstOrDefault(x => x.Id == id);

    public int CountBackups()
    {
        using var connection = Open();
        using var command = connection.CreateCommand();
        command.CommandText = "SELECT COUNT(*) FROM Backups;";
        return Convert.ToInt32(command.ExecuteScalar());
    }

    public IReadOnlyList<HistoryEvent> GetEvents(int limit = 500)
    {
        limit = Math.Clamp(limit, 1, 5000);
        using var connection = Open();
        using var command = connection.CreateCommand();
        command.CommandText = """
            SELECT Id, CreatedUtc, Level, Category, Message
            FROM Events
            ORDER BY Id DESC
            LIMIT $limit;
            """;
        command.Parameters.AddWithValue("$limit", limit);

        using var reader = command.ExecuteReader();
        var result = new List<HistoryEvent>();
        while (reader.Read())
        {
            result.Add(new HistoryEvent(
                reader.GetInt64(0),
                DateTime.Parse(reader.GetString(1), null, System.Globalization.DateTimeStyles.RoundtripKind),
                reader.GetString(2),
                reader.GetString(3),
                reader.GetString(4)));
        }
        return result;
    }

    public void ClearEvents()
    {
        using var connection = Open();
        using var command = connection.CreateCommand();
        command.CommandText = "DELETE FROM Events;";
        command.ExecuteNonQuery();
    }

    public string? GetSetting(string key)
    {
        using var connection = Open();
        using var command = connection.CreateCommand();
        command.CommandText = "SELECT Value FROM Settings WHERE Key = $key LIMIT 1;";
        command.Parameters.AddWithValue("$key", key);
        return command.ExecuteScalar() as string;
    }

    public void SetSetting(string key, string value)
    {
        using var connection = Open();
        using var command = connection.CreateCommand();
        command.CommandText = """
            INSERT INTO Settings(Key, Value, UpdatedUtc)
            VALUES ($key, $value, $updated)
            ON CONFLICT(Key) DO UPDATE SET
                Value=excluded.Value,
                UpdatedUtc=excluded.UpdatedUtc;
            """;
        command.Parameters.AddWithValue("$key", key);
        command.Parameters.AddWithValue("$value", value);
        command.Parameters.AddWithValue("$updated", DateTime.UtcNow.ToString("O"));
        command.ExecuteNonQuery();
    }

    private SqliteConnection Open()
    {
        var connection = new SqliteConnection(_connectionString);
        connection.Open();
        return connection;
    }

    private static string Normalize(string value, string fallback) =>
        string.IsNullOrWhiteSpace(value) ? fallback : value.Trim().ToUpperInvariant();

    private static void WriteTextLog(string level, string category, string message)
    {
        try
        {
            Directory.CreateDirectory(DesktopPaths.Logs);
            string path = Path.Combine(DesktopPaths.Logs, $"desktop-{DateTime.Now:yyyyMMdd}.log");
            File.AppendAllText(path,
                $"{DateTime.Now:O}\t{level}\t{category}\t{message.Replace('\r', ' ').Replace('\n', ' ')}{Environment.NewLine}");
        }
        catch
        {
            // Logging must never take down the application.
        }
    }
}