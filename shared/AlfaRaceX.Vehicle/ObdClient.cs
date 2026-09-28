namespace AlfaRaceX.Vehicle;

public sealed record ObdReading(string SignalId, double Value, string Unit,
    DateTimeOffset ObservedAt, TimeSpan Latency, TimeSpan Freshness)
{
    public bool IsFresh(DateTimeOffset now) => now >= ObservedAt && now - ObservedAt <= Freshness;
    public string Provider => "External ELM / diagnostic read";
}

public sealed class ObdClient(IElmTransport transport) : IAsyncDisposable
{
    private readonly SemaphoreSlim gate = new(1, 1);
    private bool initialized;
    private bool telemetryProven;
    private bool dtcProven;
    public bool TelemetryAvailable => initialized && transport.IsConnected && telemetryProven;
    public VehicleCapabilities Capabilities => !initialized || !transport.IsConnected ? VehicleCapabilities.None :
        (telemetryProven ? VehicleCapabilities.TelemetryObd : VehicleCapabilities.None) |
        (dtcProven ? VehicleCapabilities.DtcRead : VehicleCapabilities.None);
    public string SgwAccess => "Unknown";

    public async Task InitializeAsync(CancellationToken token)
    {
        await gate.WaitAsync(token).ConfigureAwait(false);
        try
        {
            initialized = telemetryProven = dtcProven = false;
            await transport.ConnectAsync(token).ConfigureAwait(false);
            await transport.ExchangeAsync("ATZ", token).ConfigureAwait(false);
            foreach (string command in new[] { "ATE0", "ATL0", "ATS1", "ATH1", "ATD0", "ATCAF0", "ATCFC1", "ATSP7", "ATSH18DA10F1", "ATCRA18DAF110" })
            {
                string reply = await transport.ExchangeAsync(command, token).ConfigureAwait(false);
                if (!reply.Split(['\r', '\n'], StringSplitOptions.RemoveEmptyEntries).Any(line => line.Trim() == "OK"))
                    throw new InvalidDataException($"ELM initialization rejected {command}: {reply.Trim()}");
            }
            initialized = true;
        }
        finally { gate.Release(); }
    }

    public async Task<ObdReading> ReadAsync(string signalId, CancellationToken token)
    {
        var signal = ObdSignals.All.SingleOrDefault(s => s.Id == signalId)
            ?? throw new ArgumentException("Unknown diagnostic signal.", nameof(signalId));
        await gate.WaitAsync(token).ConfigureAwait(false);
        try
        {
            if (!initialized || !transport.IsConnected) throw new InvalidOperationException("Initialize the diagnostic connection first.");
            byte[] request = Convert.FromHexString(signal.Request);
            var started = System.Diagnostics.Stopwatch.StartNew();
            string text = await transport.ExchangeAsync(request.Length.ToString("X2") + signal.Request, token).ConfigureAwait(false);
            byte[] data = ElmCanReply.Parse(text, signal.ResponseId);
            byte[] prefix = [(byte)(request[0] + 0x40), .. request.Skip(1)];
            if (data.Length != prefix.Length + signal.DataBytes || !data.AsSpan(0, prefix.Length).SequenceEqual(prefix))
                throw new InvalidDataException($"Unexpected or negative ECU response for {signal.Id}: {Convert.ToHexString(data)}");
            uint raw = 0;
            foreach (byte value in data.Skip(prefix.Length)) raw = (raw << 8) | value;
            telemetryProven = true;
            return new(signal.Id, raw * signal.Scale + signal.Offset, signal.Unit,
                DateTimeOffset.UtcNow, started.Elapsed, signal.Freshness);
        }
        finally { gate.Release(); }
    }

    public async ValueTask DisposeAsync()
    {
        await gate.WaitAsync().ConfigureAwait(false);
        try { initialized = telemetryProven = dtcProven = false; await transport.DisposeAsync().ConfigureAwait(false); }
        finally { gate.Release(); }
    }

    public async Task<DiagnosticFaultSnapshot> ReadEngineFaultsAsync(CancellationToken token)
    {
        await gate.WaitAsync(token).ConfigureAwait(false);
        try
        {
            if (!initialized || !transport.IsConnected) throw new InvalidOperationException("Initialize the diagnostic connection first.");
            // Read only, in the current default diagnostic session. No attempt to
            // enter extended sessions or unlock an ECU that refuses this request.
            string reply = await transport.ExchangeAsync("031902FF", token).ConfigureAwait(false);
            var snapshot = DiagnosticFaultSnapshot.Parse(ElmCanReply.Parse(reply, 0x18DAF110));
            dtcProven = true;
            return snapshot;
        }
        finally { gate.Release(); }
    }
}
