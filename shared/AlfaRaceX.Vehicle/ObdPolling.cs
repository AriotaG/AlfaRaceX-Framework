using System.Runtime.CompilerServices;

namespace AlfaRaceX.Vehicle;

public sealed record ObdPollResult(string SignalId, ObdReading? Reading, string? Error,
    DateTimeOffset ObservedAt, bool Reconnecting = false);

// Scheduling uses monotonic elapsed time, independent of clock corrections.
public sealed class ObdPollSchedule
{
    private sealed class Entry(ObdSignal signal)
    {
        internal ObdSignal Signal = signal;
        internal TimeSpan Due;
        internal int Failures;
    }
    private readonly Dictionary<string, Entry> entries = new();
    private TimeSpan nextRequest;
    public TimeSpan MinimumGap { get; private set; } = TimeSpan.FromMilliseconds(200);

    public string? Select(IReadOnlyCollection<string> requested, TimeSpan now)
    {
        foreach (string id in requested)
            if (!entries.ContainsKey(id)) entries.Add(id, new(ObdSignals.All.Single(s => s.Id == id)));
        if (now < nextRequest) return null;
        return entries.Values.Where(e => requested.Contains(e.Signal.Id) && e.Due <= now)
            .OrderBy(e => e.Due).ThenBy(e => e.Signal.PollInterval).Select(e => e.Signal.Id).FirstOrDefault();
    }

    public void Complete(string id, TimeSpan now, TimeSpan latency, bool success)
    {
        var entry = entries[id];
        entry.Failures = success ? 0 : Math.Min(6, entry.Failures + 1);
        double baseMs = entry.Signal.PollInterval.TotalMilliseconds;
        entry.Due = now + TimeSpan.FromMilliseconds(success ? baseMs :
            Math.Min(60000, Math.Max(2000, baseMs) * (1 << entry.Failures)));
        double measuredGap = Math.Clamp(latency.TotalMilliseconds / 2, 200, 1500);
        MinimumGap = TimeSpan.FromMilliseconds(MinimumGap.TotalMilliseconds * .75 + measuredGap * .25);
        nextRequest = now + MinimumGap;
    }
}

public static class ObdPolling
{
    public static async IAsyncEnumerable<ObdPollResult> RunAsync(ObdClient client,
        Func<IReadOnlyCollection<string>> visibleSignals,
        [EnumeratorCancellation] CancellationToken token)
    {
        var elapsed = System.Diagnostics.Stopwatch.StartNew();
        var schedule = new ObdPollSchedule();
        int connectionFailures = 0;
        while (true)
        {
            token.ThrowIfCancellationRequested();
            string? id = schedule.Select(visibleSignals(), elapsed.Elapsed);
            if (id is null) { await Task.Delay(100, token).ConfigureAwait(false); continue; }
            var requestTime = System.Diagnostics.Stopwatch.StartNew();
            ObdReading? reading = null;
            string? error = null;
            bool reconnect = false;
            try
            {
                reading = await client.ReadAsync(id, token).ConfigureAwait(false);
                connectionFailures = 0;
            }
            catch (InvalidDataException failure) { error = failure.Message; }
            catch (Exception failure) when (failure is IOException or TimeoutException or System.Net.Sockets.SocketException)
            {
                error = failure.Message;
                reconnect = true;
            }
            schedule.Complete(id, elapsed.Elapsed, requestTime.Elapsed, reading is not null);
            yield return new(id, reading, error, DateTimeOffset.UtcNow, reconnect);
            if (!reconnect) continue;
            // A timed-out stream has already been discarded. Never resume with
            // old ELM state, cached headers or capabilities from that connection.
            while (true)
            {
                if (++connectionFailures > 3) throw new IOException("OBD connection lost after three reconnect attempts: " + error);
                await Task.Delay(TimeSpan.FromSeconds(1 << connectionFailures), token).ConfigureAwait(false);
                bool recovered = false;
                try { await client.InitializeAsync(token).ConfigureAwait(false); recovered = true; }
                catch (Exception failure) when (failure is IOException or TimeoutException or System.Net.Sockets.SocketException) { error = failure.Message; }
                if (recovered) break;
                yield return new(id, null, error, DateTimeOffset.UtcNow, true);
            }
        }
    }
}
