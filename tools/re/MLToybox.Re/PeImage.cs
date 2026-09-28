using System.Text;

namespace MLToybox.Re;

public sealed record Section(string Name, uint VirtualAddress, uint VirtualSize, uint RawOffset, uint RawSize);

public sealed class PeImage
{
    public byte[] Bytes { get; }
    public ulong ImageBase { get; }
    public IReadOnlyList<Section> Sections { get; }

    private PeImage(byte[] bytes, ulong imageBase, IReadOnlyList<Section> sections)
    {
        Bytes = bytes; ImageBase = imageBase; Sections = sections;
    }

    public Section Text => Sections.First(s => s.Name == ".text");

    public static PeImage Load(string path) => Parse(File.ReadAllBytes(path));

    public static PeImage Parse(byte[] b)
    {
        int pe = BitConverter.ToInt32(b, 0x3C);
        if (b[pe] != 'P' || b[pe + 1] != 'E') throw new InvalidDataException("not a PE file");
        ushort count = BitConverter.ToUInt16(b, pe + 6);
        ushort optSize = BitConverter.ToUInt16(b, pe + 20);
        int opt = pe + 24;
        if (BitConverter.ToUInt16(b, opt) != 0x20B) throw new InvalidDataException("not PE32+");
        ulong imageBase = BitConverter.ToUInt64(b, opt + 24);
        var sections = new List<Section>();
        int sec = opt + optSize;
        for (int i = 0; i < count; i++, sec += 40)
        {
            string name = Encoding.ASCII.GetString(b, sec, 8).TrimEnd('\0');
            sections.Add(new Section(name,
                BitConverter.ToUInt32(b, sec + 12), BitConverter.ToUInt32(b, sec + 8),
                BitConverter.ToUInt32(b, sec + 20), BitConverter.ToUInt32(b, sec + 16)));
        }
        return new PeImage(b, imageBase, sections);
    }

    // 테스트용: .text 하나만 있는 가상 이미지 (파일 오프셋 0 = textVa)
    public static PeImage FromText(byte[] text, ulong textVa) =>
        new(text, textVa, new[] { new Section(".text", 0, (uint)text.Length, 0, (uint)text.Length) });

    public ReadOnlySpan<byte> SectionBytes(Section s) =>
        Bytes.AsSpan((int)s.RawOffset, (int)Math.Min(s.RawSize, s.VirtualSize));

    public int? RvaToOffset(uint rva)
    {
        foreach (var s in Sections)
            if (rva >= s.VirtualAddress && rva < s.VirtualAddress + Math.Max(s.VirtualSize, s.RawSize))
            {
                uint off = rva - s.VirtualAddress;
                return off < s.RawSize ? (int)(s.RawOffset + off) : null;
            }
        return null;
    }

    public uint? OffsetToRva(int offset)
    {
        foreach (var s in Sections)
            if (offset >= s.RawOffset && offset < s.RawOffset + s.RawSize)
                return (uint)(s.VirtualAddress + (offset - s.RawOffset));
        return null;
    }

    public ulong ReadU64(uint rva) => BitConverter.ToUInt64(Bytes, RvaToOffset(rva) ?? throw new ArgumentOutOfRangeException(nameof(rva)));
}
