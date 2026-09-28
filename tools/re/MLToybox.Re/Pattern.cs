using System.Globalization;

namespace MLToybox.Re;

public sealed class Pattern
{
    private readonly short[] _bytes;
    private Pattern(short[] bytes) => _bytes = bytes;

    public static Pattern Parse(string text)
    {
        var parts = text.Split(' ', StringSplitOptions.RemoveEmptyEntries);
        var bytes = new List<short>();
        foreach (var p in parts)
        {
            if (p is "?" or "??") { bytes.Add(-1); continue; }
            if (p.Length != 2 || !byte.TryParse(p, NumberStyles.HexNumber, null, out var v)) throw new FormatException($"bad token '{p}'");
            bytes.Add(v);
        }
        if (bytes.Count == 0 || bytes[0] < 0) throw new FormatException("empty or leading wildcard");
        return new Pattern(bytes.ToArray());
    }

    public static Pattern FromBytes(IEnumerable<short> bytes) => new(bytes.ToArray());

    public int Count(ReadOnlySpan<byte> hay, int max)
    {
        int n = _bytes.Length, hits = 0;
        byte first = (byte)_bytes[0];
        for (int i = 0; i + n <= hay.Length; i++)
        {
            if (hay[i] != first) continue;
            bool ok = true;
            for (int j = 1; j < n; j++)
                if (_bytes[j] >= 0 && hay[i + j] != (byte)_bytes[j]) { ok = false; break; }
            if (ok && ++hits >= max) break;
        }
        return hits;
    }

    public override string ToString() => string.Join(' ', _bytes.Select(b => b < 0 ? "??" : ((byte)b).ToString("X2")));
}
