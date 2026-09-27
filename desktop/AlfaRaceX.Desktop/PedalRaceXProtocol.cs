using System.Globalization;

namespace AlfaRaceX.Desktop;

internal sealed record PedalRaceXStatus(int Mode, int Power, int RequestedMap, int AppliedMap,
    int Communication, bool CanConfigure, bool EngineRunning, bool EngineKnown, bool DisablePending,
    uint Transmissions, uint Replies, uint Errors, uint ReplyAgeMs);

internal interface IPedalRaceXTransport : IDisposable
{
    string Exchange(string command, CancellationToken cancellationToken);
}

internal static class PedalRaceXProtocol
{
    internal static PedalRaceXStatus Parse(string response)
    {
        string[] lines = response.Replace(">", "\r").Split(['\r', '\n'], StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
        string[] records = lines.Where(x => x.StartsWith("ARXPRX1:", StringComparison.Ordinal)).ToArray();
        if (records.Length != 1) throw new InvalidDataException("Risposta PedalRaceX assente o ambigua. Richiesto firmware AlfaRaceX C1 con protocollo PedalRaceX v1.");
        string record = records[0];
        if (record.StartsWith("ARXPRX1:ERR:", StringComparison.Ordinal))
            throw new InvalidDataException("C1 ha rifiutato la richiesta PedalRaceX: " + record[12..]);
        const string prefix = "ARXPRX1:C1:";
        if (!record.StartsWith(prefix, StringComparison.Ordinal) || record.Length != prefix.Length + 44)
            throw new InvalidDataException("Identità C1 o formato PedalRaceX non valido.");
        string hex = record[prefix.Length..];
        if (hex.Any(x => !Uri.IsHexDigit(x))) throw new InvalidDataException("Stato PedalRaceX non esadecimale.");
        int Byte(int offset) => int.Parse(hex.AsSpan(offset, 2), NumberStyles.HexNumber, CultureInfo.InvariantCulture);
        uint Word(int offset) => uint.Parse(hex.AsSpan(offset, 8), NumberStyles.HexNumber, CultureInfo.InvariantCulture);
        int mode = Byte(0), power = Byte(2) - 10, requested = Byte(4), applied = Byte(6), comm = Byte(8), flags = Byte(10);
        if (mode > 8 || power is < -10 or > 10 || requested is < 1 or > 5 || applied > 5 || comm > 6 || flags > 15)
            throw new InvalidDataException("Valori PedalRaceX fuori intervallo.");
        return new(mode, power, requested, applied, comm, (flags & 1) != 0, (flags & 2) != 0, (flags & 8) != 0,
            (flags & 4) != 0, Word(12), Word(20), Word(28), Word(36));
    }

    internal static PedalRaceXStatus Read(IPedalRaceXTransport transport, CancellationToken ct) =>
        Parse(transport.Exchange("AT@PRX?", ct));

    internal static PedalRaceXStatus Apply(IPedalRaceXTransport transport, int mode, int power, CancellationToken ct)
    {
        if (mode is < 0 or > 8 || power is < -10 or > 10) throw new ArgumentOutOfRangeException(nameof(mode));
        PedalRaceXStatus before = Read(transport, ct); // Fresh identity and vehicle-state gate on this same connection.
        if (!before.CanConfigure) throw new InvalidOperationException("Modifica bloccata: C1 richiede dati recenti di vettura ferma o motore spento.");
        ct.ThrowIfCancellationRequested();
        PedalRaceXStatus after = Parse(transport.Exchange($"AT@PRX={mode:X2},{power + 10:X2}", ct));
        if (after.Mode != mode || after.Power != power) throw new InvalidDataException("C1 non ha accettato la configurazione richiesta.");
        return after; // Configuration accepted is not a claim of external hardware confirmation.
    }
}
