using System.Text.Json;

namespace MLToybox.Panel.Core;

public static class BridgeJson
{
    public static readonly JsonSerializerOptions Options = new(JsonSerializerDefaults.Web) { WriteIndented = true };
}
