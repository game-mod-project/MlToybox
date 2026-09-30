using MLToybox.Panel.Core;
using Xunit;

namespace MLToybox.Tests;

public class NumberInputTests
{
    [Theory]
    [InlineData("90000", 90000)]
    [InlineData("９００００", 90000)]        // 한글 IME 전각 숫자
    [InlineData("9 0 0 0 0", 90000)]
    [InlineData("1,500,000", 1500000)]
    [InlineData(" 42 ", 42)]
    public void Parse_AcceptsImeAndGroupedDigits(string text, int expected) =>
        Assert.Equal(expected, NumberInput.Parse(text));

    [Theory]
    [InlineData("")]
    [InlineData("abc")]
    [InlineData("-5")]
    [InlineData("99999999999")]
    public void Parse_RejectsInvalid(string text) => Assert.Null(NumberInput.Parse(text));
}
