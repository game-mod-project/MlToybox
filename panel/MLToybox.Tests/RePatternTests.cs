using MLToybox.Re;
using Xunit;

namespace MLToybox.Tests;

public class RePatternTests
{
    [Fact]
    public void Parse_SameFormatAsNative()
    {
        var p = Pattern.Parse("48 8B ?? 05 ? ff");
        Assert.Equal("48 8B ?? 05 ?? FF", p.ToString());
        Assert.Throws<FormatException>(() => Pattern.Parse("?? 48"));
        Assert.Throws<FormatException>(() => Pattern.Parse("488B"));
    }

    [Fact]
    public void Count_StopsAtMax()
    {
        byte[] hay = { 0x11, 0x22, 0x90, 0x11, 0x22, 0x90, 0x11, 0x22 };
        Assert.Equal(2, Pattern.Parse("11 22").Count(hay, 2));
        Assert.Equal(3, Pattern.Parse("11 22").Count(hay, 10));
        Assert.Equal(0, Pattern.Parse("33").Count(hay, 10));
    }
}
