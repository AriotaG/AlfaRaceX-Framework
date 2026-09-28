using System.Diagnostics;
using System.IO.Ports;
using System.Text;

namespace AlfaRaceX.Desktop;

internal sealed class PedalRaceXSerialTransport : IPedalRaceXTransport
{
    private readonly SerialPort _port;
    internal static string[] AvailablePorts() => SerialPort.GetPortNames().Order(StringComparer.OrdinalIgnoreCase).ToArray();

    internal PedalRaceXSerialTransport(string port)
    {
        if (!AvailablePorts().Contains(port, StringComparer.OrdinalIgnoreCase))
            throw new IOException("Porta selezionata non disponibile. Aggiorna l'elenco e collega C1 in modalità diagnostica USB.");
        // USB CDC host line coding; external C1 -> pedal remains 9600 baud in firmware.
        _port = new SerialPort(port, 115200, Parity.None, 8, StopBits.One)
        { Handshake = Handshake.None, ReadTimeout = 100, WriteTimeout = 1000, DtrEnable = true, RtsEnable = false };
        try { _port.Open(); }
        catch { _port.Dispose(); throw; }
    }

    public string Exchange(string command, CancellationToken ct)
    {
        ct.ThrowIfCancellationRequested();
        _port.DiscardInBuffer();
        _port.Write(command + "\r");
        var response = new StringBuilder();
        var elapsed = Stopwatch.StartNew();
        while (elapsed.Elapsed < TimeSpan.FromSeconds(3))
        {
            ct.ThrowIfCancellationRequested();
            int value;
            try { value = _port.ReadByte(); }
            catch (TimeoutException) { continue; } // Bounded cancellation/deadline polling, not a swallowed device error.
            if (value < 0) throw new EndOfStreamException("C1 ha interrotto la risposta PedalRaceX.");
            if (value == '>') return response.ToString();
            if (response.Length >= 512) throw new InvalidDataException("Risposta C1 oltre il limite PedalRaceX.");
            response.Append((char)value);
        }
        throw new TimeoutException("Nessuna risposta PedalRaceX valida entro 3 secondi. Verifica C1, modalità diagnostica e versione firmware.");
    }
    public void Dispose() => _port.Dispose();
}
