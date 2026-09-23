namespace AlfaRaceX.Updater;

internal static class AppConstants
{
    public const string ProductName = "AlfaRaceX";
    public const string UpdaterVersion = "0.2.0";
    public const string DefaultManifestUrl =
        "https://raw.githubusercontent.com/AriotaG/AlfaRaceX-Framework/main/distribution/release-candidate.json";

    public const ushort DfuVendorId = 0x0483;
    public const ushort DfuProductId = 0xDF11;
    public const uint FlashPageSize = 2048;
}
