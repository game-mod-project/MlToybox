using MLToybox.Re;
using Xunit;

namespace MLToybox.Tests;

public class ReSigMakerTests
{
    // 두 함수가 같은 프롤로그로 시작하고, 첫 번째만 뒤에 고유한 call 이 있다
    private static readonly byte[] Text =
    {
        0x48, 0x89, 0x5C, 0x24, 0x08,             // mov [rsp+8], rbx
        0x57,                                     // push rdi
        0x48, 0x83, 0xEC, 0x20,                   // sub rsp, 0x20
        0xE8, 0x10, 0x00, 0x00, 0x00,             // call rel32  (변위는 와일드카드 대상)
        0x48, 0x8B, 0x05, 0x44, 0x33, 0x22, 0x11, // mov rax, [rip+disp32] (와일드카드 대상)
        0xC3,                                     // ret
        0xCC, 0xCC,
        0x48, 0x89, 0x5C, 0x24, 0x08,
        0x57,
        0x48, 0x83, 0xEC, 0x20,
        0x33, 0xC0,                               // xor eax, eax
        0xC3,
    };

    [Fact]
    public void Make_ProducesUniquePatternWithWildcardedRelatives()
    {
        var pe = PeImage.FromText(Text, 0x140001000);
        var sig = SigMaker.Make(pe, 0x140001000)!;
        Assert.NotNull(sig);
        Assert.StartsWith("48 89 5C 24 08 57 48 83 EC 20 E8 ?? ?? ?? ??", sig);
        Assert.Equal(1, Pattern.Parse(sig).Count(Text, 5));
    }

    [Fact]
    public void Make_ReturnsNullWhenNeverUnique()
    {
        byte[] twice = { 0x90, 0xC3, 0x90, 0xC3 };
        var pe = PeImage.FromText(twice, 0x1000);
        Assert.Null(SigMaker.Make(pe, 0x1000, maxBytes: 2));
    }
}
