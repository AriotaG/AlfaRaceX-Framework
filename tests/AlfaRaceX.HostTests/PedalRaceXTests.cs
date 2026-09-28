using AlfaRaceX.Desktop;

internal static class PedalRaceXTests
{
    // Test transport only; never substituted into the production device path.
    private sealed class Transport(params string[] replies) : IPedalRaceXTransport
    {
        private readonly Queue<string> _replies = new(replies);
        public List<string> Commands { get; } = [];
        public string Exchange(string command, CancellationToken ct) { ct.ThrowIfCancellationRequested(); Commands.Add(command); return _replies.Dequeue(); }
        public void Dispose() { }
    }
    private static string Record(string fields = "05140404020B") => "ARXPRX1:C1:" + fields + "00000001000000010000000000000008";
    private static void Reject(Action work)
    {
        try { work(); } catch (InvalidDataException) { return; }
        throw new Exception("Invalid response accepted");
    }
    internal static void Run(Action<string, Action> test)
    {
        test("PedalRaceX bridge preserves the JavaScript status contract", () => {
            var status = PedalRaceXProtocol.Parse(Record());
            using var json = System.Text.Json.JsonDocument.Parse(System.Text.Json.JsonSerializer.Serialize(new { status }));
            var s = json.RootElement.GetProperty("status");
            string[] names = ["mode", "power", "requestedMap", "appliedMap", "communication", "canConfigure",
                "engineRunning", "engineKnown", "disablePending", "transmissions", "replies", "errors", "replyAgeMs"];
            if (!s.EnumerateObject().Select(p => p.Name).Order().SequenceEqual(names.Order()) ||
                !s.GetProperty("canConfigure").GetBoolean() || s.GetProperty("appliedMap").GetInt32() != 4)
                throw new Exception("Bridge status cannot be consumed by the WebView2 UI");
        });
        test("PedalRaceX parses real protocol fields without treating power as readback", () => {
            var s = PedalRaceXProtocol.Parse("AT@PRX?\r" + Record() + "\r>");
            if (s.Mode != 5 || s.Power != 10 || s.AppliedMap != 4 || s.Replies != 1 || !s.EngineKnown || !s.CanConfigure) throw new Exception("Fields changed");
        });
        test("PedalRaceX rejects foreign role and ambiguous identity", () => {
            Reject(() => PedalRaceXProtocol.Parse(Record().Replace(":C1:", ":BH:")));
            Reject(() => PedalRaceXProtocol.Parse(Record()+"\r"+Record()));
            Reject(() => PedalRaceXProtocol.Parse("ELM327 v1.4\rOK\r>"));
        });
        test("PedalRaceX rejects truncated out-of-range and malformed state", () => {
            Reject(() => PedalRaceXProtocol.Parse(Record()[..^1]));
            Reject(() => PedalRaceXProtocol.Parse(Record("09140404020B")));
            Reject(() => PedalRaceXProtocol.Parse(Record("051F0404020B")));
            Reject(() => PedalRaceXProtocol.Parse(Record("0514040402ZZ")));
            Reject(() => PedalRaceXProtocol.Parse("ARXPRX1:ERR:VEHICLE_STATE"));
        });
        test("PedalRaceX writes only after fresh C1 identity and state", () => {
            using var t = new Transport(Record(), Record("040A0303010B"));
            var s = PedalRaceXProtocol.Apply(t,4,0,CancellationToken.None);
            if (!t.Commands.SequenceEqual(new[]{"AT@PRX?","AT@PRX=04,0A"}) || s.Communication != 1) throw new Exception("Ordering or pending state lost");
        });
        test("PedalRaceX unknown vehicle state never sends a configuration", () => {
            using var t = new Transport(Record("051404040200"));
            try { PedalRaceXProtocol.Apply(t,4,0,CancellationToken.None); throw new Exception("Unsafe write"); }
            catch (InvalidOperationException) { if(t.Commands.Count != 1) throw new Exception("Write issued"); }
        });
        test("PedalRaceX rejected configuration never reports acceptance", () => {
            using var t = new Transport(Record(),Record());
            Reject(() => PedalRaceXProtocol.Apply(t,4,0,CancellationToken.None));
        });
        test("PedalRaceX cancellation prevents device writes", () => {
            using var t = new Transport(Record());
            try { PedalRaceXProtocol.Apply(t,4,0,new CancellationToken(true)); throw new Exception("Cancelled operation ran"); }
            catch(OperationCanceledException) { if(t.Commands.Count != 0) throw new Exception("Command issued"); }
        });
    }
}
