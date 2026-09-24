namespace AlfaRaceX.Updater;

internal static class AppConstants
{
    public const string ProductName = "AlfaRaceX";
    public static string UpdaterVersion =>
        typeof(AppConstants).Assembly.GetName().Version?.ToString(3) ?? "0.0.0";

    public const string DefaultManifestUrl =
        "https://raw.githubusercontent.com/AriotaG/AlfaRaceX-Framework/main/distribution/release-candidate.json";

    public const string UpdaterManifestUrl =
        "https://raw.githubusercontent.com/AriotaG/AlfaRaceX-Framework/main/distribution/updater.json";

    public const ushort DfuVendorId = 0x0483;
    public const ushort DfuProductId = 0xDF11;
    public const uint FlashPageSize = 2048;
}
