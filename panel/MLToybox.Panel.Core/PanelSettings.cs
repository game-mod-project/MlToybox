using System.Text.Json;

namespace MLToybox.Panel.Core;

public sealed class PanelSettings
{
    public string? GameDir { get; set; }

    public static string DefaultPath =>
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "MLToybox", "panel.json");

    public static PanelSettings Load(string path)
    {
        try
        {
            if (File.Exists(path))
                return JsonSerializer.Deserialize<PanelSettings>(File.ReadAllText(path), BridgeJson.Options) ?? new();
        }
        catch (JsonException) { }
        catch (IOException) { }
        return new PanelSettings();
    }

    public void Save(string path)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        File.WriteAllText(path, JsonSerializer.Serialize(this, BridgeJson.Options));
    }
}
