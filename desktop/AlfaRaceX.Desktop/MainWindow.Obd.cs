using AlfaRaceX.Vehicle;

namespace AlfaRaceX.Desktop;

public partial class MainWindow
{
    private volatile bool _obdVisible = true;
    private volatile string _obdGroup = "overview";
    internal Func<string, int, IElmTransport> ObdTransportFactory { get; set; } =
        (host, port) => StreamElmTransport.Tcp(host, port, TimeSpan.FromSeconds(5));

    private void SetObdView(bool visible, string group)
    {
        if (group is not ("overview" or "dpf" or "all" or "dna")) throw new ArgumentException("Gruppo OBD non valido.");
        _obdVisible = visible;
        _obdGroup = group;
    }

    private IReadOnlyCollection<string> VisibleObdSignals()
    {
        if (!_obdVisible) return Array.Empty<string>();
        return ObdSignals.All.Where(s => _obdGroup switch
        {
            "dna" => s.Experimental,
            "dpf" => s.Id.StartsWith("dpf_", StringComparison.Ordinal) || s.Id.StartsWith("regeneration", StringComparison.Ordinal),
            "overview" => s.Id is "rpm" or "speed" or "coolant" or "battery" or "dpf_load" or "regeneration",
            _ => !s.Experimental
        }).Select(s => s.Id).ToArray();
    }

    private void PostObdReading(ObdSignal signal, ObdReading reading) => Post("obdReading", new
    {
        id = signal.Id, name = signal.Name, value = reading.Value, unit = reading.Unit,
        observedUtc = reading.ObservedAt, latencyMs = reading.Latency.TotalMilliseconds,
        freshnessMs = reading.Freshness.TotalMilliseconds, provider = reading.Provider
    });

    private Task ReadObdAsync(string host, int port, bool continuous) => RunExclusiveAsync("OBD", async token =>
    {
        Post("obdReset", new { message = "Connessione diagnostica in corso…" });
        try
        {
            await using var client = new ObdClient(ObdTransportFactory(host, port));
            await client.InitializeAsync(token);
            Log("INFO", "OBD", "Adattatore inizializzato; letture STANDARD, SGW sconosciuto.");
            if (continuous)
            {
                Post("obdMonitoring", new { message = "Monitoraggio attivo; polling sospeso quando la dashboard non è visibile." });
                await foreach (var result in ObdPolling.RunAsync(client, VisibleObdSignals, token))
                {
                    var signal = ObdSignals.All.Single(s => s.Id == result.SignalId);
                    if (result.Reading is not null) PostObdReading(signal, result.Reading);
                    else
                    {
                        Log("WARN", "OBD", $"{result.SignalId}: {result.Error}");
                        if (result.Reconnecting) Post("obdReset", new { message = "Connessione persa; riconnessione limitata in corso…" });
                        Post("obdSignalError", new { id = signal.Id, name = signal.Name, message = result.Error });
                    }
                }
                return;
            }
            foreach (var signal in ObdSignals.All.Where(s => !s.Experimental))
            {
                token.ThrowIfCancellationRequested();
                try
                {
                    var reading = await client.ReadAsync(signal.Id, token);
                    PostObdReading(signal, reading);
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
