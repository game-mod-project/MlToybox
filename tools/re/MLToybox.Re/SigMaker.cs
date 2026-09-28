using Iced.Intel;

namespace MLToybox.Re;

public static class SigMaker
{
    public static string? Make(PeImage pe, ulong va, int maxBytes = 64)
    {
        var text = pe.SectionBytes(pe.Text).ToArray();
        var acc = new List<short>();
        foreach (var (instr, bytes, offs) in Disasm.Decode(pe, va, 64))
        {
            var masked = bytes.Select(b => (short)b).ToArray();
            bool branch = instr.FlowControl is FlowControl.Call or FlowControl.UnconditionalBranch or FlowControl.ConditionalBranch;
            if (instr.IsIPRelativeMemoryOperand && offs.HasDisplacement)
                for (int k = 0; k < offs.DisplacementSize; k++) masked[offs.DisplacementOffset + k] = -1;
            if (branch && offs.HasImmediate)
                for (int k = 0; k < offs.ImmediateSize; k++) masked[offs.ImmediateOffset + k] = -1;
            foreach (var m in masked)
            {
                if (acc.Count >= maxBytes) return null;
                acc.Add(m);
            }
            // 명령어 경계에서만 유일성 검사. 끝의 와일드카드는 남겨 둔다(스캐너가 허용, 명령어 길이 보존)
            if (acc.Count >= 8 && Pattern.FromBytes(acc).Count(text, 2) == 1)
                return Pattern.FromBytes(acc).ToString();
        }
        return null;
    }
}
