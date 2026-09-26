using System.Runtime.InteropServices;

namespace AlfaRaceX.Updater;

internal static class UsbDeviceEnumerator
{
    public static IReadOnlyList<string> FindDfuPaths()
    {
        Guid guid = WinUsbNative.GuidDevInterfaceUsbDevice;
        IntPtr set = WinUsbNative.SetupDiGetClassDevs(
            ref guid,
            IntPtr.Zero,
            IntPtr.Zero,
            WinUsbNative.DigcfPresent | WinUsbNative.DigcfDeviceInterface);

        if (set == WinUsbNative.InvalidHandleValue)
            WinUsbNative.ThrowLast("Impossibile enumerare i dispositivi USB.");

        try
        {
            var result = new List<string>();
            uint index = 0;

            while (true)
            {
                var data = new WinUsbNative.SpDeviceInterfaceData
                {
                    cbSize = (uint)Marshal.SizeOf<WinUsbNative.SpDeviceInterfaceData>()
                };

                if (!WinUsbNative.SetupDiEnumDeviceInterfaces(
                    set, IntPtr.Zero, ref guid, index, ref data))
                {
                    int error = Marshal.GetLastWin32Error();
                    if (error == 259) break;
                    WinUsbNative.ThrowLast("Enumerazione USB fallita.");
                }

                _ = WinUsbNative.SetupDiGetDeviceInterfaceDetail(
                    set, ref data, IntPtr.Zero, 0, out uint required, IntPtr.Zero);
                if (required < 6 || required > int.MaxValue)
                    throw new IOException("Dimensione del percorso USB non valida.");

                IntPtr detail = Marshal.AllocHGlobal((int)required);
                try
                {
                    Marshal.WriteInt32(detail, IntPtr.Size == 8 ? 8 : 6);
                    if (!WinUsbNative.SetupDiGetDeviceInterfaceDetail(
                        set, ref data, detail, required, out _, IntPtr.Zero))
                        WinUsbNative.ThrowLast("Lettura percorso USB fallita.");

                    string path = ReadDevicePath(detail, required);
                    string lower = path.ToLowerInvariant();

                    string vid = $"vid_{AppConstants.DfuVendorId:x4}";
                    string pid = $"pid_{AppConstants.DfuProductId:x4}";
                    if (lower.Contains(vid) && lower.Contains(pid))
                        result.Add(path);
                }
                finally
                {
                    Marshal.FreeHGlobal(detail);
                }

                index++;
            }

            return result;
        }
        finally
        {
            WinUsbNative.SetupDiDestroyDeviceInfoList(set);
        }
    }

    internal static string ReadDevicePath(IntPtr detail, uint size)
    {
        if (detail == IntPtr.Zero || size < 6 || size > int.MaxValue)
            throw new InvalidDataException("Buffer percorso USB non valido.");
        // DWORD cbSize precedes WCHAR DevicePath[] on both architectures.
        // cbSize is 8 on x64; it is not the offset of DevicePath (4).
        string buffer = Marshal.PtrToStringUni(IntPtr.Add(detail, 4), checked((int)(size - 4) / 2))!;
        int end = buffer.IndexOf('\0');
        if (end < 0) throw new InvalidDataException("Percorso USB non terminato.");
        return buffer[..end];
    }
}
