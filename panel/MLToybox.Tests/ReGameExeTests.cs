using MLToybox.Re;
using Xunit;

namespace MLToybox.Tests;

public class ReGameExeTests
{
    private const string Exe = @"E:\SteamLibrary\steamapps\common\Manor Lords\ManorLords\Binaries\Win64\ManorLords-Win64-Shipping.exe";

    [Fact]
    public void ExecResolver_FindsThunkForKnownUFunction()
    {
        var pe = PeImage.Load(Exe);
        var thunks = ExecResolver.Resolve(pe, "getConstructionProgress");
        Assert.NotEmpty(thunks);
        var text = pe.Text;
        Assert.All(thunks, va => Assert.InRange(va, pe.ImageBase + text.VirtualAddress, pe.ImageBase + text.VirtualAddress + text.VirtualSize));
    }
}
