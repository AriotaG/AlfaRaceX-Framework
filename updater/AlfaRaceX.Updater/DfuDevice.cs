using Microsoft.Win32.SafeHandles;
using System.Runtime.InteropServices;

namespace AlfaRaceX.Updater;

internal sealed class DfuDevice : IDisposable
{
    private const byte RequestDnload = 1;
    private const byte RequestUpload = 2;
    private const byte RequestGetStatus = 3;
    private const byte RequestClearStatus = 4;
    private const byte RequestAbort = 6;

    private const byte DfuIdle = 2;
    private const byte DfuDownloadBusy = 4;
    private const byte DfuDownloadIdle = 5;
    private const byte DfuError = 10;

    private readonly SafeFileHandle _file;
    private IntPtr _usb;
    public int TransferSize { get; }

    private DfuDevice(SafeFileHandle file, IntPtr usb)
    {
        _file = file;
        _usb = usb;
        if (!WinUsbNative.WinUsb_SetCurrentAlternateSetting(_usb, 0))
            WinUsbNative.ThrowLast("Impossibile selezionare l'interfaccia DFU.");
        TransferSize = ReadTransferSize();
    }

    public static DfuDevice OpenSingle()
    {
        var paths = UsbDeviceEnumerator.FindDfuPaths();
        if (paths.Count == 0)
            throw new InvalidOperationException(
                "Nessun dispositivo in modalità DFU rilevato. Verifica collegamento e driver WinUSB.");
        if (paths.Count > 1)
            throw new InvalidOperationException(
                "Sono presenti più dispositivi DFU. Lascia collegato un solo modulo alla volta.");

        var file = WinUsbNative.CreateFile(
            paths[0],
            WinUsbNative.GenericRead | WinUsbNative.GenericWrite,
            WinUsbNative.FileShareRead | WinUsbNative.FileShareWrite,
            IntPtr.Zero,
            WinUsbNative.OpenExisting,
            WinUsbNative.FileAttributeNormal,
            IntPtr.Zero);

        if (file.IsInvalid)
            WinUsbNative.ThrowLast(
                "Il dispositivo è presente ma non è accessibile tramite WinUSB.");

        if (!WinUsbNative.WinUsb_Initialize(file, out IntPtr usb))
        {
            file.Dispose();
            WinUsbNative.ThrowLast("Driver USB non compatibile con WinUSB.");
        }

        var device = new DfuDevice(file, usb);
        device.NormalizeState();
        return device;
    }

    public void ProgramAndVerify(
        IntelHexImage image,
        uint pageSize,
        IProgress<int>? progress,
        Action<string>? log,
        CancellationToken ct)
    {
        var pages = image.TouchedPages(pageSize).ToArray();
        int payloadBytes = image.Bytes.Count;
        long totalUnits = Math.Max(1L, (long)pages.Length * pageSize + (long)payloadBytes * 2L);
        long completed = 0;

        log?.Invoke($"DFU transfer size: {TransferSize} byte.");

        foreach (uint page in pages)
        {
            ct.ThrowIfCancellationRequested();
            ErasePage(page);
            completed += pageSize;
            progress?.Report((int)Math.Clamp(completed * 100L / totalUnits, 0, 100));
        }

        foreach (var segment in image.Segments())
        {
            ct.ThrowIfCancellationRequested();
            WriteSegment(segment.Address, segment.Data, bytes =>
            {
                completed += bytes;
                progress?.Report((int)Math.Clamp(completed * 100L / totalUnits, 0, 100));
            });
        }

        foreach (var segment in image.Segments())
        {
            ct.ThrowIfCancellationRequested();
            VerifySegment(segment.Address, segment.Data, bytes =>
            {
                completed += bytes;
                progress?.Report((int)Math.Clamp(completed * 100L / totalUnits, 0, 100));
            });
        }

        progress?.Report(100);
    }

    public void Leave(uint applicationAddress)
    {
        EnsureIdle();
        SetAddressPointer(applicationAddress);
        var setup = Setup(0x21, RequestDnload, 0, 0);
        _ = Control(setup, Array.Empty<byte>());
    }

    private void ErasePage(uint address)
    {
        EnsureIdle();
        byte[] command =
        {
            0x41,
            (byte)(address & 0xFF),
            (byte)((address >> 8) & 0xFF),
            (byte)((address >> 16) & 0xFF),
            (byte)((address >> 24) & 0xFF)
        };
        _ = Control(Setup(0x21, RequestDnload, 0, (ushort)command.Length), command);
        WaitDownloadIdle();
    }

    private void SetAddressPointer(uint address)
    {
        byte[] command =
        {
            0x21,
            (byte)(address & 0xFF),
            (byte)((address >> 8) & 0xFF),
            (byte)((address >> 16) & 0xFF),
            (byte)((address >> 24) & 0xFF)
        };
        _ = Control(Setup(0x21, RequestDnload, 0, (ushort)command.Length), command);
        WaitDownloadIdle();
    }

    private void WriteSegment(uint address, byte[] data, Action<int> onBytes)
    {
        EnsureIdle();
        SetAddressPointer(address);

        int offset = 0;
        ushort block = 2;
        while (offset < data.Length)
        {
            int count = Math.Min(TransferSize, data.Length - offset);
            byte[] chunk = data.AsSpan(offset, count).ToArray();
            _ = Control(Setup(0x21, RequestDnload, block, (ushort)count), chunk);
            WaitDownloadIdle();
            offset += count;
            block++;
            onBytes(count);
        }

        EnsureIdle();
    }

    private void VerifySegment(uint address, byte[] expected, Action<int> onBytes)
    {
        EnsureIdle();
        SetAddressPointer(address);
        EnsureIdle();

        int offset = 0;
        ushort block = 2;
        while (offset < expected.Length)
        {
            int count = Math.Min(TransferSize, expected.Length - offset);
            byte[] actual = new byte[count];
            uint transferred = Control(Setup(0xA1, RequestUpload, block, (ushort)count), actual);

            if (transferred != count)
                throw new IOException($"Verifica incompleta a 0x{address + (uint)offset:X8}.");

            if (!actual.AsSpan(0, count).SequenceEqual(expected.AsSpan(offset, count)))
                throw new IOException($"Verifica fallita a 0x{address + (uint)offset:X8}.");

            offset += count;
            block++;
            onBytes(count);
        }

        EnsureIdle();
    }

    private void NormalizeState()
    {
        DfuStatus s = GetStatus();
        if (s.State == DfuError)
        {
            _ = Control(Setup(0x21, RequestClearStatus, 0, 0), Array.Empty<byte>());
            s = GetStatus();
        }

        if (s.State != DfuIdle)
            EnsureIdle();
    }

    private void EnsureIdle()
    {
        DfuStatus s = GetStatus();
        if (s.State == DfuIdle) return;

        if (s.State == DfuError)
        {
            _ = Control(Setup(0x21, RequestClearStatus, 0, 0), Array.Empty<byte>());
            s = GetStatus();
            if (s.State == DfuIdle) return;
        }

        _ = Control(Setup(0x21, RequestAbort, 0, 0), Array.Empty<byte>());
        s = GetStatus();
        if (s.State != DfuIdle)
            throw new IOException($"Impossibile riportare DFU in IDLE (stato {s.State}).");
    }

    private void WaitDownloadIdle()
    {
        for (int i = 0; i < 200; i++)
        {
            DfuStatus s = GetStatus();
            if (s.State == DfuDownloadIdle || s.State == DfuIdle)
                return;
            if (s.State == DfuError)
                throw new IOException($"DFU errore 0x{s.Status:X2}.");

            Thread.Sleep(Math.Clamp(s.PollTimeoutMs, 1, 5000));
        }

        throw new TimeoutException("Timeout durante l'operazione DFU.");
    }

    private DfuStatus GetStatus()
    {
        byte[] data = new byte[6];
        uint transferred = Control(Setup(0xA1, RequestGetStatus, 0, 6), data);
        if (transferred != 6)
            throw new IOException("Risposta DFU GETSTATUS incompleta.");

        int poll = data[1] | (data[2] << 8) | (data[3] << 16);
        return new DfuStatus(data[0], poll, data[4]);
    }

    private int ReadTransferSize()
    {
        byte[] head = new byte[9];
        if (!WinUsbNative.WinUsb_GetDescriptor(_usb, 2, 0, 0, head, (uint)head.Length, out uint n) || n < 9)
            return 1024;

        int total = head[2] | (head[3] << 8);
        if (total < 9 || total > 4096) return 1024;

        byte[] config = new byte[total];
        if (!WinUsbNative.WinUsb_GetDescriptor(_usb, 2, 0, 0, config, (uint)config.Length, out n))
            return 1024;

        for (int i = 0; (uint)(i + 8) < n;)
        {
            int len = config[i];
            int type = config[i + 1];
            if (len < 2) break;
            if (type == 0x21 && len >= 9)
            {
                int size = config[i + 5] | (config[i + 6] << 8);
                if (size >= 64 && size <= 4096) return size;
            }
            i += len;
        }

        return 1024;
    }

    private static WinUsbNative.WinUsbSetupPacket Setup(
        byte requestType, byte request, ushort value, ushort length) =>
        new()
        {
            RequestType = requestType,
            Request = request,
            Value = value,
            Index = 0,
            Length = length
        };

    private uint Control(WinUsbNative.WinUsbSetupPacket setup, byte[] buffer)
    {
        if (!WinUsbNative.WinUsb_ControlTransfer(
            _usb,
            setup,
            buffer,
            (uint)buffer.Length,
            out uint transferred,
            IntPtr.Zero))
        {
            int error = Marshal.GetLastWin32Error();
            throw new IOException($"Trasferimento DFU fallito. Errore Windows {error}.");
        }
        return transferred;
    }

    public void Dispose()
    {
        if (_usb != IntPtr.Zero)
        {
            WinUsbNative.WinUsb_Free(_usb);
            _usb = IntPtr.Zero;
        }
        _file.Dispose();
    }

    private readonly record struct DfuStatus(byte Status, int PollTimeoutMs, byte State);
}
