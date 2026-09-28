using MLToybox.Re;
using Xunit;

namespace MLToybox.Tests;

public class ReXrefsTests
{
    [Fact]
    public void Find_LocatesRipRelativeReferences()
    {
        const ulong textVa = 0x140001000;
        // 0x00: lea rdx,[rip+0x20]  (48 8D 15 20 00 00 00) → 명령 끝 0x07 + 0x20 = 0x27
        // 0x07: nop x3
        // 0x0A: mov rax,[rip+0x1D]  (48 8B 05 1D 00 00 00) → 명령 끝 0x11 + 0x1D = 0x2E (다른 대상)
        var text = new byte[0x40];
        new byte[] { 0x48, 0x8D, 0x15, 0x20, 0x00, 0x00, 0x00, 0x90, 0x90, 0x90, 0x48, 0x8B, 0x05, 0x1D, 0x00, 0x00, 0x00 }.CopyTo(text, 0);
        var pe = PeImage.FromText(text, textVa);
        var hits = Xrefs.Find(pe, textVa + 0x27);
        Assert.Equal(new ulong[] { textVa }, hits);           // 명령 시작 주소(= disp 위치 - 3)
        Assert.Empty(Xrefs.Find(pe, textVa + 0x30));
    }
}
