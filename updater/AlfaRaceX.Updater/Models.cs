using System.Text.Json.Serialization;

namespace AlfaRaceX.Updater;

internal sealed class UpdaterManifest
{
    [JsonPropertyName("product")] public string Product { get; set; } = "";
    [JsonPropertyName("version")] public string Version { get; set; } = "";
    [JsonPropertyName("releaseUrl")] public string ReleaseUrl { get; set; } = "";
}

internal sealed class UpdateManifest
{
    [JsonPropertyName("product")] public string Product { get; set; } = "";
    [JsonPropertyName("channel")] public string Channel { get; set; } = "";
    [JsonPropertyName("version")] public string Version { get; set; } = "";
    [JsonPropertyName("targets")] public List<FirmwareTarget> Targets { get; set; } = new();
}

internal sealed class FirmwareTarget
{
    [JsonPropertyName("id")] public string Id { get; set; } = "";
    [JsonPropertyName("label")] public string Label { get; set; } = "";
    [JsonPropertyName("portHint")] public string PortHint { get; set; } = "";
    [JsonPropertyName("url")] public string Url { get; set; } = "";
    [JsonPropertyName("sha256")] public string Sha256 { get; set; } = "";
    [JsonPropertyName("applicationStart")] public uint ApplicationStart { get; set; }
    [JsonPropertyName("applicationLimitExclusive")] public uint ApplicationLimitExclusive { get; set; }
}

internal sealed record PreparedFirmware(
    FirmwareTarget Target,
    string LocalPath,
    IntelHexImage Image
);
