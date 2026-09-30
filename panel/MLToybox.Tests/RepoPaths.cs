namespace MLToybox.Tests;

public static class RepoPaths
{
    public static string Root { get; } = FindRoot();
    public static string Scripts => Path.Combine(Root, "mod", "MLToybox", "Scripts");
    public static string LuaTests => Path.Combine(Root, "mod", "MLToybox", "tests");

    private static string FindRoot()
    {
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir is not null && !File.Exists(Path.Combine(dir.FullName, "CLAUDE.md")))
            dir = dir.Parent;
        return dir?.FullName ?? throw new InvalidOperationException("repo root (CLAUDE.md) not found");
    }
}
