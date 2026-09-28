namespace AlfaRaceX.Vehicle;

public sealed record ObdSignal(string Id, string Name, string Unit, string Request,
    int DataBytes, double Scale, double Offset, TimeSpan PollInterval, string Reference)
{
    public uint RequestId => 0x18DA10F1;
    public uint ResponseId => 0x18DAF110;
    public TimeSpan Freshness => PollInterval * 3;
    public string Profile => "Giulia/Stelvio 2.2 diesel; ECU support must be detected";
    public string Validation => "Public protocol reference; not validated on target vehicle";
}

public static class ObdSignals
{
    private const string Dpf = "https://github.com/dixtone/GiuliaAndStelvioDPFMonitor";
    // Only unambiguous read requests. No DNA-to-map inference, security access,
    // actuator commands, DTC clearing, PROXI or firmware operations.
    public static IReadOnlyList<ObdSignal> All { get; } = Array.AsReadOnly(new[]
    {
        new ObdSignal("rpm", "Regime motore", "rpm", "010C", 2, .25, 0, TimeSpan.FromMilliseconds(500), "SAE J1979 PID 0C"),
        new ObdSignal("speed", "Velocità", "km/h", "010D", 1, 1, 0, TimeSpan.FromMilliseconds(500), "SAE J1979 PID 0D"),
        new ObdSignal("coolant", "Temperatura refrigerante", "°C", "0105", 1, 1, -40, TimeSpan.FromSeconds(2), "SAE J1979 PID 05"),
        new ObdSignal("dpf_load", "Intasamento DPF", "%", "2218E4", 2, 1000.0 / 65535, 0, TimeSpan.FromSeconds(2), Dpf),
        new ObdSignal("dpf_temperature", "Temperatura DPF", "°C", "2218DE", 2, .02, -40, TimeSpan.FromSeconds(2), Dpf),
        new ObdSignal("regeneration", "Progresso rigenerazione", "%", "22380B", 2, 100.0 / 65535, 0, TimeSpan.FromSeconds(1), Dpf),
        new ObdSignal("regeneration_distance", "Distanza ultima rigenerazione", "km", "223807", 3, .1, 0, TimeSpan.FromSeconds(30), Dpf),
        new ObdSignal("regeneration_count", "Numero rigenerazioni", "", "2218A4", 2, 1, 0, TimeSpan.FromSeconds(60), Dpf),
        new ObdSignal("battery", "Tensione batteria", "V", "221955", 2, .0005, 0, TimeSpan.FromSeconds(5), Dpf),
        new ObdSignal("oil_quality", "Qualità olio", "%", "223813", 2, 100.0 / 65535, 0, TimeSpan.FromSeconds(30), Dpf)
    });
}
