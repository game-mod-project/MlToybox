#include "test.h"
#include "json.h"

using namespace mlt;

TEST(json_parses_nested_objects_and_types) {
    auto j = parseJson(R"({"a":{"b":true,"n":-2.5e1,"s":"x\"yé","arr":[1,null,false]},"z":null})");
    CHECK(j.has_value());
    CHECK(j->get("a")->get("b")->asBool(false) == true);
    CHECK(j->get("a")->get("n")->asNumber(0) == -25.0);
    CHECK(j->get("a")->get("s")->s == "x\"y\xC3\xA9");
    CHECK(j->get("a")->get("arr")->a->size() == 3);
    CHECK(j->get("z")->type == Json::Type::Null);
    CHECK(j->get("missing") == nullptr);
}

TEST(json_accepts_bom_and_whitespace) {
    auto j = parseJson("\xEF\xBB\xBF \r\n{ \"k\" : 1 }\n");
    CHECK(j.has_value() && j->get("k")->asNumber(0) == 1.0);
}

TEST(json_rejects_malformed) {
    CHECK(!parseJson("{").has_value());
    CHECK(!parseJson("{\"a\":}").has_value());
    CHECK(!parseJson("{\"a\":1} trailing").has_value());
    CHECK(!parseJson("").has_value());
}

TEST(json_escape_roundtrip) {
    CHECK(escapeJson("a\"b\\c\n") == "a\\\"b\\\\c\\n");
}
