using System.Text.RegularExpressions;

namespace MLToybox.Panel.Core;

public static class GameLocator
{
    public const string AppId = "1363080";
    public const string InstallDirName = "Manor Lords";

    private static readonly Regex LibraryEntry = new(
        "\"path\"\\s*\"(?<path>[^\"]+)\"(?<body>.*?)(?=\"path\"|\\z)", RegexOptions.Singleline);
    private static readonly Regex AppKey = new($"\"{AppId}\"\\s*\"");

    public static string? FindLibraryWithApp(string vdfText)
    {
        foreach (Match m in LibraryEntry.Matches(vdfText))
        {
            if (AppKey.IsMatch(m.Groups["body"].Value))
                return m.Groups["path"].Value.Replace(@"\\", @"\");
        }
        return null;
    }

    public static string GameDirFromLibrary(string library) =>
        Path.Combine(library, "steamapps", "common", InstallDirName);

    public static string BridgeDir(string gameDir) =>
        Path.Combine(gameDir, "ManorLords", "Binaries", "Win64", "ue4ss", "Mods", "MLToybox", "bridge");

    public static bool LooksLikeGameDir(string dir) =>
        File.Exists(Path.Combine(dir, "ManorLords", "Binaries", "Win64", "ManorLords-Win64-Shipping.exe"));

    public static string? Detect(string? steamPath)
    {
        if (string.IsNullOrEmpty(steamPath)) return null;
        var vdf = Path.Combine(steamPath, "steamapps", "libraryfolders.vdf");
        if (!File.Exists(vdf)) return null;
        var library = FindLibraryWithApp(File.ReadAllText(vdf));
        if (library is null) return null;
        var dir = GameDirFromLibrary(library);
        return LooksLikeGameDir(dir) ? dir : null;
    }

    public static string? SteamPathFromRegistry() =>
        Microsoft.Win32.Registry.GetValue(@"HKEY_CURRENT_USER\Software\Valve\Steam", "SteamPath", null) as string;
}
