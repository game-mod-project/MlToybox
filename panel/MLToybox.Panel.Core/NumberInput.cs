using System.Text;

namespace MLToybox.Panel.Core;

// 숫자 칸 입력 정규화: 한글 IME 전각 숫자(０-９), 공백, 천 단위 쉼표를 허용한다
public static class NumberInput
{
    public static int? Parse(string? text)
    {
        if (string.IsNullOrWhiteSpace(text)) return null;
        var sb = new StringBuilder();
        foreach (var ch in text)
        {
            if (ch is >= '０' and <= '９') sb.Append((char)('0' + (ch - '０')));
            else if (ch is >= '0' and <= '9') sb.Append(ch);
            else if (char.IsWhiteSpace(ch) || ch is ',' or '，') continue;
            else return null;
        }
        return int.TryParse(sb.ToString(), out var v) && v >= 0 ? v : null;
    }
}
