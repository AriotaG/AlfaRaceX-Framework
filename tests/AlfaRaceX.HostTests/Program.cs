using AlfaRaceX.Updater;

int failed = 0;
string root = Path.Combine(Path.GetTempPath(), "arx-tests-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(root);
string Record(byte type, ushort address, params byte[] data)
{
    byte[] bytes = [(byte)data.Length, (byte)(address >> 8), (byte)address, type, .. data];
    return ":" + Convert.ToHexString(bytes) + ((byte)(-bytes.Sum(b => b))).ToString("X2");
}
string upper = Record(4, 0, 8, 0);
string payload = Record(0, 0, 0, 64, 0, 32, 9, 0, 0, 8, 0, 191);
string eof = Record(1, 0);
IntelHexImage Load(params string[] lines)
{
    string path = Path.Combine(root, Guid.NewGuid() + ".hex");
    File.WriteAllLines(path, lines);
    return IntelHexImage.Load(path);
}
void Test(string name, Action test)
{
    try { test(); Console.WriteLine("PASS " + name); }
    catch (Exception e) { failed++; Console.WriteLine("FAIL " + name + ": " + e.Message); }
}
void Reject(Action action)
{
    try { action(); }
    catch (InvalidDataException) { return; }
    throw new Exception("Invalid input was accepted");
}
try
{
    Test("valid HEX range and segments", () => {
        var image = Load(upper, payload, eof);
        image.ValidateApplicationRange(0x08000000, 0x0800F000);
        if (image.Bytes.Count != 10 || image.Segments().Single().Data.Length != 10) throw new Exception("Data changed");
    });
    Test("missing EOF", () => Reject(() => Load(upper, payload)));
    Test("empty EOF-only image", () => Reject(() => Load(eof)));
    Test("trailing data after EOF", () => Reject(() => Load(upper, payload, eof, payload)));
    Test("duplicate address", () => Reject(() => Load(upper, payload, payload, eof)));
    Test("malformed EOF", () => Reject(() => Load(upper, payload, Record(1, 1, 1))));
    Test("invalid start record", () => Reject(() => Load(upper, payload, Record(5, 0, 1), eof)));
    Test("address overflow", () => Reject(() => Load(Record(4, 0, 255, 255), Record(0, 65535, 1, 2), eof)));
    Test("bad checksum", () => Reject(() => Load(upper, payload[..^2] + "FF", eof)));
    Test("outside application", () => Reject(() => Load(upper, payload, eof).ValidateApplicationRange(0x08000800, 0x0800F000)));
    Test("invalid hex characters", () => Reject(() => Load(upper, ":GG", eof)));
    Test("odd hex length", () => Reject(() => Load(upper, ":0", eof)));
    Test("unknown record type", () => Reject(() => Load(upper, payload, Record(6, 0), eof)));
    Test("invalid extended address offset", () => Reject(() => Load(Record(4, 1, 8, 0), payload, eof)));
    Test("exclusive upper limit", () => Reject(() => Load(upper, Record(0, 0xF000, 1), eof).ValidateApplicationRange(0x08000000, 0x0800F000)));
    Test("segmented address and sparse image preserved", () => {
        var image = Load(Record(2, 0, 0x12, 0x34), Record(0, 5, 1, 2), Record(0, 9, 3), eof);
        var segments = image.Segments().ToArray();
        if (segments.Length != 2 || segments[0].Address != 0x12345 ||
            !segments[0].Data.SequenceEqual(new byte[] {1, 2}) || segments[1].Address != 0x12349 ||
            segments[1].Data.Single() != 3) throw new Exception("Sparse data changed");
    });
    Test("highest address without overflow", () => {
        var image = Load(Record(4, 0, 255, 255), Record(0, 65535, 7), eof);
        if (image.MaxAddress != uint.MaxValue || image.Bytes[uint.MaxValue] != 7) throw new Exception("Boundary changed");
    });
    Test("page coverage and zero page size", () => {
        var image = Load(upper, Record(0, 0x7FF, 1, 2), eof);
        if (!image.TouchedPages(2048).SequenceEqual(new uint[] {0x08000000, 0x08000800})) throw new Exception("Page coverage changed");
        try { image.TouchedPages(0); }
        catch (ArgumentOutOfRangeException) { return; }
        throw new Exception("Zero page size accepted");
    });
    Test("start records and blank lines accepted", () => {
        var image = Load("", upper, payload, Record(3, 0, 0, 0, 0, 0), Record(5, 0, 8, 0, 0, 1), eof, " ");
        if (image.Bytes.Count != 10) throw new Exception("Start record changed data");
    });
}
finally { Directory.Delete(root, true); }
Console.WriteLine($"Failed: {failed}");
return failed == 0 ? 0 : 1;
