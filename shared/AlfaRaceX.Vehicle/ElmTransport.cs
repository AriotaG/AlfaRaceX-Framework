using System.Net.Sockets;
using System.Text;

namespace AlfaRaceX.Vehicle;

// Platform BLE implementations must use the adapter's documented services, not
// an assumed serial UUID. This boundary has no WPF or Android dependency.
public interface IElmTransport : IAsyncDisposable
{
    bool IsConnected { get; }
    Task ConnectAsync(CancellationToken cancellationToken);
    Task<string> ExchangeAsync(string command, CancellationToken cancellationToken);
}

public sealed class StreamElmTransport(
    Func<CancellationToken, Task<Stream>> openStream, TimeSpan timeout) : IElmTransport
{
    private readonly SemaphoreSlim gate = new(1, 1);
    private readonly TimeSpan timeout = timeout > TimeSpan.Zero && timeout <= TimeSpan.FromMinutes(2)
        ? timeout : throw new ArgumentOutOfRangeException(nameof(timeout));
    private Stream? stream;
    private bool disposed;
    public bool IsConnected => stream is not null;

    public static StreamElmTransport Tcp(string host, int port, TimeSpan timeout)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(host);
        if (port is < 1 or > 65535) throw new ArgumentOutOfRangeException(nameof(port));
        return new(async token =>
        {
            var client = new TcpClient { NoDelay = true };
            try
            {
                await client.ConnectAsync(host, port, token).ConfigureAwait(false);
                // NetworkStream owns the socket; disposing it closes the connection.
                return new NetworkStream(client.Client, ownsSocket: true);
            }
            catch { client.Dispose(); throw; }
        }, timeout);
    }

    public async Task ConnectAsync(CancellationToken cancellationToken)
    {
        await gate.WaitAsync(cancellationToken).ConfigureAwait(false);
        try
        {
            ObjectDisposedException.ThrowIf(disposed, this);
            if (stream is not null) return;
            using var deadline = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
            deadline.CancelAfter(timeout);
            stream = await openStream(deadline.Token).ConfigureAwait(false);
        }
        finally { gate.Release(); }
    }

    public async Task<string> ExchangeAsync(string command, CancellationToken cancellationToken)
    {
        if (string.IsNullOrWhiteSpace(command) || command.Length > 64 ||
            command.Any(c => c is < ' ' or > '~'))
            throw new ArgumentException("ELM requires one bounded ASCII command without line breaks.", nameof(command));
        await gate.WaitAsync(cancellationToken).ConfigureAwait(false);
        try
        {
            ObjectDisposedException.ThrowIf(disposed, this);
            var current = stream ?? throw new InvalidOperationException("ELM transport is disconnected.");
            using var deadline = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
            deadline.CancelAfter(timeout);
            // Also interrupts platform streams whose ReadAsync ignores cancellation.
            using var abort = deadline.Token.Register(current.Dispose);
            try
            {
                await current.WriteAsync(Encoding.ASCII.GetBytes(command + "\r"), deadline.Token).ConfigureAwait(false);
                await current.FlushAsync(deadline.Token).ConfigureAwait(false);
                var reply = new StringBuilder();
                var one = new byte[1];
                while (reply.Length < 16384)
                {
                    if (await current.ReadAsync(one, deadline.Token).ConfigureAwait(false) == 0)
                        throw new IOException("ELM closed the connection before its prompt.");
                    if (one[0] == '>') return reply.ToString();
                    if (one[0] > 127) throw new InvalidDataException("Non-ASCII ELM response.");
                    reply.Append((char)one[0]);
                }
                throw new InvalidDataException("ELM response exceeds 16 KiB.");
            }
            catch (Exception error)
            {
                // Never let a late reply satisfy the next diagnostic request.
                stream = null;
                current.Dispose();
                if (deadline.IsCancellationRequested)
                {
                    cancellationToken.ThrowIfCancellationRequested();
                    throw new TimeoutException($"ELM prompt timeout after {timeout.TotalMilliseconds:0} ms.", error);
                }
                throw;
            }
        }
        finally { gate.Release(); }
    }

    public async ValueTask DisposeAsync()
    {
        await gate.WaitAsync().ConfigureAwait(false);
        try { disposed = true; stream?.Dispose(); stream = null; }
        finally { gate.Release(); }
    }
}
