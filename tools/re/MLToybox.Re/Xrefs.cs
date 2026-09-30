using Iced.Intel;

namespace MLToybox.Re;

// .text 에서 RIP 상대 메모리 피연산자(lea/mov 등)로 va 를 참조하는 명령의 시작 주소를 찾는다.
public static class Xrefs
{
    public static List<ulong> Find(PeImage pe, ulong va)
    {
        var text = pe.Text;
        var bytes = pe.SectionBytes(text).ToArray();
        ulong textVa = pe.ImageBase + text.VirtualAddress;
        var hits = new List<ulong>();
        for (int i = 2; i + 4 <= bytes.Length; i++)
        {
            int disp = BitConverter.ToInt32(bytes, i);
            if (textVa + (ulong)i + 4 + (ulong)(long)disp != va) continue;
            // disp32 가 명령 끝에 있는 형태만: 시작 후보를 긴 것(i-4)부터 디코드해 검증한다.
            // 짧은 것부터 보면 REX 접두사를 떼어 낸 해석(예: 8D 15 = lea edx)도 유효해서 시작 주소가 어긋난다.
            for (int back = 4; back >= 2; back--)
            {
                int start = i - back;
                if (start < 0) continue;
                var decoder = Decoder.Create(64, new ByteArrayCodeReader(bytes, start, bytes.Length - start), textVa + (ulong)start);
                decoder.Decode(out var instr);
                if (!instr.IsInvalid && instr.IsIPRelativeMemoryOperand && instr.IPRelativeMemoryAddress == va && start + instr.Length == i + 4)
                {
                    hits.Add(textVa + (ulong)start);
                    break;
                }
            }
        }
        return hits;
    }
}
