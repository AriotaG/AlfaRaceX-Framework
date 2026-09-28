using AlfaRaceX.Vehicle;
using System.Net;
using System.Net.Sockets;
using System.Text;

internal static class ObdTests
{
    private static void Check(bool condition) { if (!condition) throw new Exception("OBD assertion failed"); }
    private static void Reject(string text)
    {
        try { ElmCanReply.Parse(text, 0x18DAF110); }
        catch (InvalidDataException) { return; }
        throw new Exception("Invalid ISO-TP response accepted");
    }

    public static void Run(Action<string, Action> test)
    {
        test("OBD rejects unbounded transport timeout and command injection", () =>
        {
            try { StreamElmTransport.Tcp("localhost", 1234, Timeout.InfiniteTimeSpan); throw new Exception("Infinite timeout accepted"); }
            catch (ArgumentOutOfRangeException) { }
            using var transport = new AsyncDisposer(StreamElmTransport.Tcp("localhost", 1234, TimeSpan.FromSeconds(1)));
            try { transport.Value.ExchangeAsync("ATI\r04", default).GetAwaiter().GetResult(); throw new Exception("Multiple commands accepted"); }
            catch (ArgumentException) { }
        });
        test("OBD DTC preserves manufacturer code and status", () =>
        {
            var snapshot = DiagnosticFaultSnapshot.Parse([0x59, 2, 0xFF, 0x12, 0x34, 0x56, 0x09]);
            Check(snapshot.Faults.Single() == new DiagnosticFault("123456", 9));
            Check(DiagnosticFaultSnapshot.Parse([0x59, 2, 0xFF]).Faults.Count == 0);
        });
        test("OBD malformed DTC response is never an empty successful scan", () =>
        {
            foreach (byte[] bytes in new byte[][] { [0x7F, 0x19, 0x11], [0x59, 2], [0x59, 2, 0xFF, 1] })
            {
                try { DiagnosticFaultSnapshot.Parse(bytes); }
                catch (InvalidDataException) { continue; }
                throw new Exception("Malformed DTC accepted");
            }
        });
        test("OBD raw single-frame and foreign ECU filtering", () =>
            Check(ElmCanReply.Parse("18DAF118 04 41 0C FF FF\r18DAF110 04 41 0C 1F 40 00 00 00", 0x18DAF110)
                .SequenceEqual(new byte[] { 0x41, 0x0C, 0x1F, 0x40 })));
        test("OBD ISO-TP multi-frame reassembly", () =>
            Check(ElmCanReply.Parse("18DAF110 10 09 62 12 34 01 02 03\r18DAF110 21 04 05 06 00 00 00 00", 0x18DAF110)
                .SequenceEqual(new byte[] { 0x62, 0x12, 0x34, 1, 2, 3, 4, 5, 6 })));
        test("OBD rejects missing or reordered frames", () =>
        {
            Reject("18DAF110 10 09 62 12 34 01 02 03");
            Reject("18DAF110 10 09 62 12 34 01 02 03\r18DAF110 22 04 05 06");
            Reject("18DAF110 21 04 05 06");
        });
        test("OBD rejects malformed, duplicate and missing data", () =>
        {
            Reject("NO DATA"); Reject("18DAF110 07 41 0C"); Reject("18DAF110 GG");
            Reject("18DAF110 01 41\r18DAF110 01 41");
            Reject("18DAF118 04 41 0C 00 00");
        });
        test("OBD catalog only permits well formed reads", () =>
        {
            foreach (var signal in ObdSignals.All)
            {
                var request = Convert.FromHexString(signal.Request);
                Check(request[0] == 1 && request.Length == 2 || request[0] == 0x22 && request.Length == 3);
                Check(signal.DataBytes is >= 1 and <= 4 && signal.PollInterval > TimeSpan.Zero);
            }
        });
        test("OBD reading freshness rejects stale and future timestamps", () =>
        {
            var now = DateTimeOffset.UtcNow;
            var reading = new ObdReading("rpm", 800, "rpm", now, TimeSpan.Zero, TimeSpan.FromSeconds(1));
            Check(reading.IsFresh(now) && !reading.IsFresh(now.AddSeconds(2)) && !reading.IsFresh(now.AddSeconds(-1)));
        });
        test("OBD TCP loopback initialization and fragmented real socket response", () => TcpSession().GetAwaiter().GetResult());
        test("OBD TCP timeout discards connection before reconnect", () => TimeoutSession().GetAwaiter().GetResult());
    }

    private sealed class AsyncDisposer(IElmTransport value) : IDisposable
    {
        public IElmTransport Value => value;
        public void Dispose() => value.DisposeAsync().AsTask().GetAwaiter().GetResult();
    }

    private static async Task<string> ReadCommand(NetworkStream stream, CancellationToken token)
    {
        var bytes = new List<byte>(); var one = new byte[1];
        while (await stream.ReadAsync(one, token) != 0)
        {
            if (one[0] == '\r') return Encoding.ASCII.GetString(bytes.ToArray());
            bytes.Add(one[0]);
        }
        throw new IOException("Test client disconnected");
    }

    private static async Task TcpSession()
    {
        using var deadline = new CancellationTokenSource(TimeSpan.FromSeconds(10));
        using var listener = new TcpListener(IPAddress.Loopback, 0);
        listener.Start();
        var server = Task.Run(async () =>
        {
            using var peer = await listener.AcceptTcpClientAsync(deadline.Token);
            using var stream = peer.GetStream();
            var commands = new[] { "ATZ", "ATE0", "ATL0", "ATS1", "ATH1", "ATD0", "ATCAF0", "ATCFC1", "ATSP7", "ATSH18DA10F1", "ATCRA18DAF110", "02010C", "032218E4", "02010D", "031902FF" };
            foreach (var expected in commands)
            {
                Check(await ReadCommand(stream, deadline.Token) == expected);
                string response = expected switch
                {
                    "ATZ" => "ELM327 test fixture\r>",
                    "02010C" => "18DAF110 04 41 0C 1F 40 00 00 00\r>",
                    "032218E4" => "18DAF110 05 62 18 E4 FF FF 00 00\r>",
                    "02010D" => "18DAF110 03 7F 01 11 00 00 00 00\r>",
                    "031902FF" => "18DAF110 07 59 02 FF 12 34 56 09\r>",
                    _ => "OK\r>"
                };
                foreach (byte b in Encoding.ASCII.GetBytes(response))
                    await stream.WriteAsync(new byte[] { b }, deadline.Token);
            }
        }, deadline.Token);
        await using var client = new ObdClient(StreamElmTransport.Tcp("127.0.0.1", ((IPEndPoint)listener.LocalEndpoint).Port, TimeSpan.FromSeconds(2)));
        Check(!client.TelemetryAvailable);
        await client.InitializeAsync(deadline.Token);
        Check(!client.TelemetryAvailable && client.SgwAccess == "Unknown");
        Check((await client.ReadAsync("rpm", deadline.Token)).Value == 2000);
        Check(client.TelemetryAvailable);
        Check((await client.ReadAsync("dpf_load", deadline.Token)).Value == 1000);
        try { await client.ReadAsync("speed", deadline.Token); throw new Exception("Negative reply accepted"); }
        catch (InvalidDataException) { }
        Check((await client.ReadEngineFaultsAsync(deadline.Token)).Faults.Single().RawCode == "123456");
        Check(client.Capabilities == (VehicleCapabilities.TelemetryObd | VehicleCapabilities.DtcRead));
        await server;
    }

    private static async Task TimeoutSession()
    {
        using var deadline = new CancellationTokenSource(TimeSpan.FromSeconds(10));
        using var listener = new TcpListener(IPAddress.Loopback, 0);
        listener.Start();
        var server = Task.Run(async () =>
        {
            using var first = await listener.AcceptTcpClientAsync(deadline.Token);
            Check(await ReadCommand(first.GetStream(), deadline.Token) == "ATI");
            using var second = await listener.AcceptTcpClientAsync(deadline.Token);
            Check(await ReadCommand(second.GetStream(), deadline.Token) == "ATI");
            await second.GetStream().WriteAsync(Encoding.ASCII.GetBytes("new connection\r>"), deadline.Token);
        }, deadline.Token);
        await using var transport = StreamElmTransport.Tcp("127.0.0.1", ((IPEndPoint)listener.LocalEndpoint).Port, TimeSpan.FromMilliseconds(150));
        await transport.ConnectAsync(deadline.Token);
        try { await transport.ExchangeAsync("ATI", deadline.Token); throw new Exception("Timeout was not enforced"); }
        catch (TimeoutException) { }
        Check(!transport.IsConnected);
        await transport.ConnectAsync(deadline.Token);
        Check((await transport.ExchangeAsync("ATI", deadline.Token)).Contains("new connection"));
        await server;
    }
}
