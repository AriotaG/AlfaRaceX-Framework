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
                if (Marshal.GetLastWin32Error() != 122 || required < 6 || required > 65536)
                    throw new IOException("Dimensione percorso USB non valida durante enumerazione.");

                IntPtr detail = Marshal.AllocHGlobal((int)required);
                try
                {
                    Marshal.WriteInt32(detail, IntPtr.Size == 8 ? 8 : 6);
                    if (!WinUsbNative.SetupDiGetDeviceInterfaceDetail(
                        set, ref data, detail, required, out _, IntPtr.Zero))
                        WinUsbNative.ThrowLast("Lettura percorso USB fallita.");

                    // DevicePath follows a DWORD on both architectures. cbSize is
                    // 8 on x64 because of tail padding, not because of field offset.
                    IntPtr pathPtr = IntPtr.Add(detail, sizeof(uint));
                    string path = Marshal.PtrToStringUni(pathPtr) ?? "";
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
}
