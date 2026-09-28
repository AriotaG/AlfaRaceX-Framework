namespace AlfaRaceX.Vehicle;

[Flags]
public enum VehicleCapabilities
{
    None = 0,
    TelemetryObd = 1 << 0,
    DtcRead = 1 << 1,
    DtcClear = 1 << 2,
    DirectCanC1 = 1 << 3,
    DirectCanC2 = 1 << 4,
    DirectCanBH = 1 << 5,
    CanSniffer = 1 << 6,
    RaceControl = 1 << 7,
    EscTcControl = 1 << 8,
    AwdControl = 1 << 9,
    PedalRaceXControl = 1 << 10,
    AutomaticDna = 1 << 11,
    FirmwareUpdate = 1 << 12,
    BackupRestore = 1 << 13
}

public sealed record DiagnosticFault(string RawCode, byte Status);
public sealed record DiagnosticFaultSnapshot(byte SupportedStatusMask,
    IReadOnlyList<DiagnosticFault> Faults, DateTimeOffset ObservedAt)
{
    // UDS 0x19 / reportDTCByStatusMask. Preserve 24-bit manufacturer code and
    // status; do not invent a P-code description or claim a complete vehicle scan.
    public static DiagnosticFaultSnapshot Parse(byte[] data)
    {
        if (data.Length < 3 || data[0] != 0x59 || data[1] != 0x02 || (data.Length - 3) % 4 != 0)
            throw new InvalidDataException("Invalid or negative UDS DTC response: " + Convert.ToHexString(data));
        var faults = new List<DiagnosticFault>();
        for (int offset = 3; offset < data.Length; offset += 4)
            faults.Add(new(Convert.ToHexString(data.AsSpan(offset, 3)), data[offset + 3]));
        return new(data[2], faults.AsReadOnly(), DateTimeOffset.UtcNow);
    }
}
