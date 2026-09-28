using System.Globalization;
using System.Text;
using MLToybox.Re;

var argList = args.ToList();
string exe = @"E:\SteamLibrary\steamapps\common\Manor Lords\ManorLords\Binaries\Win64\ManorLords-Win64-Shipping.exe";
int exeIdx = argList.IndexOf("--exe");
if (exeIdx >= 0) { exe = argList[exeIdx + 1]; argList.RemoveRange(exeIdx, 2); }
if (argList.Count == 0) { Console.WriteLine("usage: exec <UFunction> | disasm <va> [n] | sig <va> | count \"<pattern>\" | strings <text>"); return 1; }

var pe = PeImage.Load(exe);
ulong Hex(string s) => ulong.Parse(s.Replace("0x", ""), NumberStyles.HexNumber);

switch (argList[0])
{
    case "exec":
        foreach (var t in ExecResolver.Resolve(pe, argList[1])) Console.WriteLine($"{t:X16}");
        break;
    case "disasm":
        foreach (var l in Disasm.Lines(pe, Hex(argList[1]), argList.Count > 2 ? int.Parse(argList[2]) : 40)) Console.WriteLine(l);
        break;
    case "sig":
        Console.WriteLine(SigMaker.Make(pe, Hex(argList[1])) ?? "NOT UNIQUE within 64 bytes");
        break;
    case "count":
        Console.WriteLine(Pattern.Parse(argList[1]).Count(pe.SectionBytes(pe.Text), 10));
        break;
    case "strings":
        foreach (var enc in new[] { Encoding.ASCII, Encoding.Unicode })
        {
            var needle = enc.GetBytes(argList[1]);
            var all = pe.Bytes;
            for (int i = all.AsSpan().IndexOf(needle); i >= 0;)
            {
                uint? rva = pe.OffsetToRva(i);
                if (rva is not null) Console.WriteLine($"{(enc == Encoding.ASCII ? "ascii" : "utf16")} {pe.ImageBase + rva.Value:X16}");
                int next = all.AsSpan(i + 1).IndexOf(needle);
                i = next < 0 ? -1 : i + 1 + next;
            }
        }
        break;
    default:
        Console.WriteLine($"unknown command {argList[0]}"); return 1;
}
return 0;
