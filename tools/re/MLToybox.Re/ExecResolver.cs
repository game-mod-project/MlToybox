using System.Text;

namespace MLToybox.Re;

// UE5 는 UFunction 네이티브 등록 테이블에 { const char* Name; FNativeFuncPtr Pointer } (FNameNativePtrPair) 을 둔다.
// 이름 문자열 VA 를 가리키는 포인터를 데이터 섹션에서 찾고, 바로 다음 qword 를 exec 썽크로 본다.
public static class ExecResolver
{
    public static IReadOnlyList<ulong> Resolve(PeImage pe, string name)
    {
        var needle = Encoding.ASCII.GetBytes("\0" + name + "\0");
        var text = pe.Text;
        ulong textStart = pe.ImageBase + text.VirtualAddress;
        var results = new List<ulong>();
        foreach (var s in pe.Sections.Where(s => s.Name != ".text"))
        {
            foreach (int i in FindAll(pe.SectionBytes(s), needle))
            {
                ulong strVa = pe.ImageBase + s.VirtualAddress + (uint)i + 1;
                foreach (var refVa in FindPointers(pe, strVa))
                {
                    ulong thunk = pe.ReadU64((uint)(refVa - pe.ImageBase + 8));
                    if (thunk >= textStart && thunk < textStart + text.VirtualSize) results.Add(thunk);
                }
            }
        }
        return results.Distinct().ToList();
    }

    private static List<int> FindAll(ReadOnlySpan<byte> hay, byte[] needle)
    {
        var hits = new List<int>();
        int from = 0;
        while (from < hay.Length)
        {
            int j = hay[from..].IndexOf(needle);
            if (j < 0) break;
            hits.Add(from + j);
            from += j + 1;
        }
        return hits;
    }

    // Span 지역 변수는 yield 이터레이터에서 쓸 수 없으므로 List 로 모아 반환한다
    public static List<ulong> FindPointers(PeImage pe, ulong va)
    {
        var found = new List<ulong>();
        foreach (var s in pe.Sections.Where(s => s.Name is ".rdata" or ".data"))
        {
            var bytes = pe.SectionBytes(s);
            for (int i = 0; i + 8 <= bytes.Length; i += 8)
                if (BitConverter.ToUInt64(bytes.Slice(i, 8)) == va)
                    found.Add(pe.ImageBase + s.VirtualAddress + (uint)i);
        }
        return found;
    }
}
