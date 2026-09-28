using Iced.Intel;

namespace MLToybox.Re;

public static class Disasm
{
    public static IEnumerable<(Instruction Instr, byte[] Bytes, ConstantOffsets Offsets)> Decode(PeImage pe, ulong va, int maxInstructions)
    {
        int off = pe.RvaToOffset((uint)(va - pe.ImageBase)) ?? throw new ArgumentOutOfRangeException(nameof(va));
        var reader = new ByteArrayCodeReader(pe.Bytes, off, pe.Bytes.Length - off);
        var decoder = Decoder.Create(64, reader, va);
        for (int n = 0; n < maxInstructions; n++)
        {
            decoder.Decode(out var instr);
            if (instr.IsInvalid) yield break;
            var bytes = pe.Bytes.AsSpan(off + (int)(instr.IP - va), instr.Length).ToArray();
            yield return (instr, bytes, decoder.GetConstantOffsets(instr));
        }
    }

    public static IReadOnlyList<string> Lines(PeImage pe, ulong va, int maxInstructions)
    {
        var formatter = new NasmFormatter();
        var output = new StringOutput();
        var lines = new List<string>();
        foreach (var (instr, bytes, _) in Decode(pe, va, maxInstructions))
        {
            formatter.Format(instr, output);
            lines.Add($"{instr.IP:X16}  {Convert.ToHexString(bytes),-24}  {output.ToStringAndReset()}");
        }
        return lines;
    }
}
