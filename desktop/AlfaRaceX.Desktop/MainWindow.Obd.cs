using AlfaRaceX.Vehicle;

namespace AlfaRaceX.Desktop;

public partial class MainWindow
{
    internal Func<string, int, IElmTransport> ObdTransportFactory { get; set; } =
        (host, port) => StreamElmTransport.Tcp(host, port, TimeSpan.FromSeconds(5));

    private Task ReadObdAsync(string host, int port) => RunExclusiveAsync("OBD", async token =>
    {
        Post("obdReset", new { message = "Connessione diagnostica in corso…" });
        try
        {
            await using var client = new ObdClient(ObdTransportFactory(host, port));
            await client.InitializeAsync(token);
            Log("INFO", "OBD", "Adattatore inizializzato; letture STANDARD, SGW sconosciuto.");
            foreach (var signal in ObdSignals.All)
            {
                token.ThrowIfCancellationRequested();
                try
                {
                    var reading = await client.ReadAsync(signal.Id, token);
                    Post("obdReading", new
                    {
                        id = signal.Id, name = signal.Name, value = reading.Value, unit = reading.Unit,
                        observedUtc = reading.ObservedAt, latencyMs = reading.Latency.TotalMilliseconds,
                        freshnessMs = reading.Freshness.TotalMilliseconds, provider = reading.Provider
                    });
                }
                catch (InvalidDataException error)
                {
                    Log("WARN", "OBD", $"{signal.Id}: {error.Message}");
                    Post("obdSignalError", new { id = signal.Id, name = signal.Name, message = error.Message });
                }
                // Sequential requests with a minimum gap; never flood a slow ELM adapter.
                await Task.Delay(200, token);
            }
            try
            {
                var faults = await client.ReadEngineFaultsAsync(token);
                Post("obdFaults", new { message = faults.Faults.Count == 0 ?
                    "ECU motore: nessun DTC restituito per la maschera richiesta." :
                    "ECU motore · DTC grezzi: " + string.Join(", ", faults.Faults.Select(f => $"{f.RawCode} (stato {f.Status:X2})")) });
            }
            catch (InvalidDataException error)
            {
                Log("WARN", "OBD", "DTC motore: " + error.Message);
                Post("obdFaults", new { message = "DTC motore non disponibili: " + error.Message });
            }
            Log("INFO", "OBD", $"Lettura terminata: capacità verificate={client.Capabilities}; connessione chiusa.");
        }
        finally
        {
            Post("obdClosed", new { message = "Sessione chiusa. I valori sono campioni storici, non telemetria in tempo reale." });
        }
    });
}
