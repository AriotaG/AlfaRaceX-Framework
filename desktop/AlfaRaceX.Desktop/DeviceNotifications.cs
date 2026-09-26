using System.Windows.Interop;
using System.Windows.Threading;

namespace AlfaRaceX.Desktop;

public partial class MainWindow
{
    // Windows broadcasts DBT_DEVNODES_CHANGED when devices are added or removed.
    // https://learn.microsoft.com/windows/win32/devio/dbt-devnodes-changed
    private const int WmDeviceChange = 0x0219;
    private const int DbtDevnodesChanged = 0x0007;
    private readonly DispatcherTimer _deviceRefreshTimer = new() { Interval = TimeSpan.FromMilliseconds(500) };
    private HwndSource? _windowSource;
    private bool _windowClosed;
    private int _dashboardGeneration;

    private void InitializeDeviceNotifications()
    {
        SourceInitialized += (_, _) =>
        {
            _windowSource = HwndSource.FromHwnd(new WindowInteropHelper(this).Handle);
            _windowSource?.AddHook(DeviceWindowProc);
        };
        _deviceRefreshTimer.Tick += async (_, _) =>
        {
            _deviceRefreshTimer.Stop();
            if (!_windowClosed) await SendDashboardAsync();
        };
        Closed += (_, _) =>
        {
            _windowClosed = true;
            ++_dashboardGeneration;
            _deviceRefreshTimer.Stop();
            _windowSource?.RemoveHook(DeviceWindowProc);
            _windowSource = null;
        };
    }

    private IntPtr DeviceWindowProc(IntPtr hwnd, int message, IntPtr wParam, IntPtr lParam, ref bool handled)
    {
        if (message == WmDeviceChange && wParam.ToInt64() == DbtDevnodesChanged && !_windowClosed)
        {
            ++_dashboardGeneration; // Invalidate an enumeration started before this notification.
            _deviceRefreshTimer.Stop();
            _deviceRefreshTimer.Start();
            handled = true;
            return new IntPtr(1);
        }
        return IntPtr.Zero;
    }
}
