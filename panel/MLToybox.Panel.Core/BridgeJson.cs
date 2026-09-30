using System.Text.Json;
using System.Text.Json.Serialization;

namespace MLToybox.Panel.Core;

public static class BridgeJson
{
    public static readonly JsonSerializerOptions Options = new(JsonSerializerDefaults.Web)
    {
        WriteIndented = true,
        Converters = { new LuaEmptyTableDictionaryConverterFactory() },
    };
}

// 모드(Lua)의 JSON 인코더는 빈 테이블을 {} 가 아니라 [] 로 쓴다. 문자열 키 사전 자리에 온 빈 배열은 빈 사전으로 읽는다
public sealed class LuaEmptyTableDictionaryConverterFactory : JsonConverterFactory
{
    public override bool CanConvert(Type t) =>
        t.IsGenericType && t.GetGenericTypeDefinition() == typeof(Dictionary<,>) && t.GetGenericArguments()[0] == typeof(string);

    public override JsonConverter CreateConverter(Type t, JsonSerializerOptions options) =>
        (JsonConverter)Activator.CreateInstance(typeof(Converter<>).MakeGenericType(t.GetGenericArguments()[1]))!;

    private sealed class Converter<TValue> : JsonConverter<Dictionary<string, TValue>>
    {
        public override Dictionary<string, TValue>? Read(ref Utf8JsonReader reader, Type typeToConvert, JsonSerializerOptions options)
        {
            if (reader.TokenType == JsonTokenType.StartArray)
            {
                reader.Skip();   // 빈 배열만 온다고 가정하지 않고 내용은 버린다(사전으로 해석할 수 없음)
                return new Dictionary<string, TValue>();
            }
            if (reader.TokenType != JsonTokenType.StartObject) throw new JsonException("expected object");
            var dict = new Dictionary<string, TValue>();
            while (reader.Read() && reader.TokenType != JsonTokenType.EndObject)
            {
                var key = reader.GetString()!;
                reader.Read();
                dict[key] = JsonSerializer.Deserialize<TValue>(ref reader, options)!;
            }
            return dict;
        }

        public override void Write(Utf8JsonWriter writer, Dictionary<string, TValue> value, JsonSerializerOptions options)
        {
            writer.WriteStartObject();
            foreach (var (k, v) in value)
            {
                writer.WritePropertyName(k);   // 키는 그대로(자원 id 등 대소문자 유지)
                JsonSerializer.Serialize(writer, v, options);
            }
            writer.WriteEndObject();
        }
    }
}
