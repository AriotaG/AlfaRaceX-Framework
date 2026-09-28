using System.Globalization;

namespace AlfaRaceX.Vehicle;

public static class ElmCanReply
{
    // ATH1 / ATD0 / CAF0: raw CAN data includes ISO-TP PCI. CFC1 handles
    // flow-control at the adapter. Only the requested ECU can supply data.
    public static byte[] Parse(string text, uint responseId)
    {
        var result = new List<byte>();
        int expected = -1, sequence = 1;
        bool completed = false;
        foreach (string raw in text.Split(['\r', '\n'], StringSplitOptions.RemoveEmptyEntries))
        {
            string line = string.Concat(raw.Where(c => !char.IsWhiteSpace(c)));
            if (line is "" or "SEARCHING..." or "BUSINIT:OK") continue;
            int headerLength = responseId > 0x7FF ? 8 : 3;
            if (line.Length < headerLength || !uint.TryParse(line.AsSpan(0, headerLength),
                NumberStyles.HexNumber, CultureInfo.InvariantCulture, out uint id))
                throw new InvalidDataException($"ELM diagnostic response: {raw.Trim()}");
            if (id != responseId) continue;
            byte[] frame;
            try { frame = Convert.FromHexString(line[headerLength..]); }
            catch (FormatException e) { throw new InvalidDataException("Malformed CAN frame.", e); }
            if (frame.Length is < 2 or > 8 || completed)
                throw new InvalidDataException("Invalid or duplicate diagnostic CAN frame.");
            int kind = frame[0] >> 4;
            if (kind == 0)
            {
                int length = frame[0] & 15;
                if (expected != -1 || length < 1 || length > frame.Length - 1)
                    throw new InvalidDataException("Invalid ISO-TP single frame.");
                result.AddRange(frame.Skip(1).Take(length));
                completed = true;
            }
            else if (kind == 1)
            {
                expected = expected == -1 ? ((frame[0] & 15) << 8) | frame[1] :
                    throw new InvalidDataException("Repeated ISO-TP first frame.");
                if (expected <= 7 || frame.Length != 8)
                    throw new InvalidDataException("Invalid ISO-TP first-frame length.");
                result.AddRange(frame.Skip(2));
            }
            else if (kind == 2 && expected > 0)
            {
                if ((frame[0] & 15) != sequence || frame.Length - 1 < Math.Min(7, expected - result.Count))
                    throw new InvalidDataException("Missing or out-of-order ISO-TP data.");
                result.AddRange(frame.Skip(1).Take(expected - result.Count));
                sequence = (sequence + 1) & 15;
                completed = result.Count == expected;
            }
            else throw new InvalidDataException("Unexpected ISO-TP frame.");
        }
        if (!completed) throw new InvalidDataException("No complete response from the requested ECU.");
        return result.ToArray();
    }
}
