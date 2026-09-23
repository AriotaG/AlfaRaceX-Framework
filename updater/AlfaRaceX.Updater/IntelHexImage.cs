namespace AlfaRaceX.Updater;

internal sealed class IntelHexImage
{
    private readonly SortedDictionary<uint, byte> _bytes = new();

    public IReadOnlyDictionary<uint, byte> Bytes => _bytes;
    public uint MinAddress => _bytes.Count == 0 ? 0 : _bytes.First().Key;
    public uint MaxAddress => _bytes.Count == 0 ? 0 : _bytes.Last().Key;

    public static IntelHexImage Load(string path)
    {
        var image = new IntelHexImage();
        uint upper = 0;
        int lineNo = 0;

        foreach (string raw in File.ReadLines(path))
        {
            lineNo++;
            string line = raw.Trim();
            if (line.Length == 0) continue;
            if (!line.StartsWith(':'))
                throw new InvalidDataException($"HEX riga {lineNo}: prefisso mancante.");

            byte[] record = Convert.FromHexString(line[1..]);
            if (record.Length < 5)
                throw new InvalidDataException($"HEX riga {lineNo}: record troppo corto.");

            int count = record[0];
            if (record.Length != count + 5)
                throw new InvalidDataException($"HEX riga {lineNo}: lunghezza errata.");

            int sum = 0;
            foreach (byte b in record) sum = (sum + b) & 0xFF;
            if (sum != 0)
                throw new InvalidDataException($"HEX riga {lineNo}: checksum errato.");

            ushort offset = (ushort)((record[1] << 8) | record[2]);
            byte type = record[3];

            switch (type)
            {
                case 0x00:
                    for (int i = 0; i < count; i++)
                        image._bytes[upper + offset + (uint)i] = record[4 + i];
                    break;
                case 0x01:
                    return image;
                case 0x02:
                    if (count != 2) throw new InvalidDataException("HEX: segmento esteso non valido.");
                    upper = (uint)(((record[4] << 8) | record[5]) << 4);
                    break;
                case 0x04:
                    if (count != 2) throw new InvalidDataException("HEX: indirizzo lineare non valido.");
                    upper = (uint)(((record[4] << 8) | record[5]) << 16);
                    break;
                case 0x03:
                case 0x05:
                    break;
                default:
                    throw new InvalidDataException($"HEX: tipo record 0x{type:X2} non supportato.");
            }
        }

        if (image._bytes.Count == 0)
            throw new InvalidDataException("Il firmware HEX non contiene dati.");

        return image;
    }

    public void ValidateApplicationRange(uint start, uint limitExclusive)
    {
        if (start >= limitExclusive)
            throw new InvalidDataException("Intervallo applicativo non valido.");

        foreach (uint address in _bytes.Keys)
        {
            if (address < start || address >= limitExclusive)
                throw new InvalidDataException(
                    $"Il firmware contiene dati fuori area applicativa: 0x{address:X8}.");
        }
    }

    public IEnumerable<uint> TouchedPages(uint pageSize)
    {
        var pages = new SortedSet<uint>();
        foreach (uint address in _bytes.Keys)
            pages.Add(address - (address % pageSize));
        return pages;
    }

    public IEnumerable<(uint Address, byte[] Data)> Segments()
    {
        if (_bytes.Count == 0) yield break;

        uint start = 0;
        uint previous = 0;
        var buffer = new List<byte>();
        bool first = true;

        foreach (var pair in _bytes)
        {
            if (first)
            {
                start = previous = pair.Key;
                buffer.Add(pair.Value);
                first = false;
                continue;
            }

            if (pair.Key != previous + 1)
            {
                yield return (start, buffer.ToArray());
                buffer.Clear();
                start = pair.Key;
            }

            buffer.Add(pair.Value);
            previous = pair.Key;
        }

        if (buffer.Count > 0)
            yield return (start, buffer.ToArray());
    }
}
