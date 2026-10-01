#include "test.h"
#include "overlay/core/status_doc.h"
#include "runtime.h"
#include <filesystem>

using namespace mlt::ov;

// 탭이 읽는 상태 항목(자원, 영지, 병력, 수행원, 인구, 용병)
TEST(overlay_status_reads_the_tab_data_from_a_real_file) {
    auto text = mlt::readFileUtf8(std::filesystem::path(MLT_FIXTURES_DIR) / "status_from_mod.json");
    CHECK(text.has_value());
    auto s = parseStatus(*text);
    CHECK(s.has_value());
    CHECK(s->resourceIds.has_value() && s->resourceIds->size() == 56 && (*s->resourceIds)[1] == "Timber");
    CHECK(s->resources.has_value() && s->resources->at("Timber") == 1849.0);
    CHECK(s->regions.has_value() && s->regions->size() == 3);
    CHECK((*s->regions)[0].key == "gold" && (*s->regions)[0].name == "Mandlach" && (*s->regions)[0].values.at("Timber") == 722.0);
    CHECK(s->playerRegions.has_value() && s->playerRegions->size() == 3 && (*s->playerRegions)[2].key == "nus" && (*s->playerRegions)[2].name == "Haderwand");
    CHECK(s->spawn.has_value() && s->spawn->disbanded == 0 && s->spawn->pending == 0 && s->spawn->byUnit.empty());   // byUnit 은 [] 로 적혀 있다
    CHECK(s->retinue.has_value() && s->retinue->squads.empty() && !s->retinue->editing);
    const PopulationStatus& p = *s->population;
    CHECK(p.families == 157 && p.population == 486 && p.homeless == 0 && p.freeSlots == 58 && p.unassigned == 67 && p.natural == 2 && p.multiplied == 0);
    CHECK(p.regions.size() == 3 && p.regions[1].key == "Lei" && p.regions[1].families == 5 && p.regions[1].freeSlots == 15);
    const MercenaryStatus& m = *s->mercenaries;
    CHECK(m.slots.size() == 3 && m.slots[0].name == "검사대" && m.slots[0].cost == 10000000 && m.slots[0].custom && !m.slots[1].custom);
    CHECK(m.hiredMine == 1 && m.hiredAi == 1 && m.refunded == 0 && m.skipped.empty() && !m.note);
}

TEST(overlay_status_reads_spawn_retinue_and_mercenary_details) {
    auto s = parseStatus(R"({"version":1,"heartbeat":10,"inGame":true,
        "spawn":{"disbanded":3,"pending":1,"byUnit":{"retinue_tier1":2,"militia":1}},
        "retinue":{"squads":[{"id":63,"unit":"retinue_tier1","count":36,"kind":"spawned"},{"id":64,"unit":"retinue_tier3","count":12,"kind":"mercenary"}],"editing":63},
        "mercenaries":{"slots":[{"name":"토이박스 용병단","cost":10000000,"custom":true}],"hiredMine":2,"hiredAi":3,"refunded":6000,
                       "skipped":[{"name":"궁수대","reason":"unknown unit: foo"}],"note":"rebuild produced 1 of 3 slots"}})");
    CHECK(s.has_value());
    CHECK(s->spawn->disbanded == 3 && s->spawn->pending == 1 && s->spawn->byUnit.size() == 2);
    CHECK(s->spawn->byUnit[0].first == "retinue_tier1" && s->spawn->byUnit[0].second == 2);   // 파일에 적힌 순서
    CHECK(s->retinue->squads.size() == 2 && s->retinue->squads[1].id == 64 && s->retinue->squads[1].kind == "mercenary" && s->retinue->editing == 63);
    CHECK(s->mercenaries->refunded == 6000 && s->mercenaries->skipped.size() == 1 && s->mercenaries->skipped[0].reason == "unknown unit: foo");
    CHECK(s->mercenaries->note == "rebuild produced 1 of 3 slots");
}

TEST(overlay_status_tab_data_is_absent_or_empty_when_the_mod_sends_nothing) {
    auto s = parseStatus(R"({"version":1,"heartbeat":10,"inGame":false})");
    CHECK(s.has_value());
    CHECK(!s->resourceIds && !s->resources && !s->regions && !s->playerRegions && !s->spawn && !s->retinue && !s->population && !s->mercenaries);
    // 모드의 JSON 인코더는 빈 표를 [] 로 쓴다
    s = parseStatus(R"({"version":1,"heartbeat":10,"inGame":true,"resources":[],"resourceIds":[],"regions":[],"playerRegions":[],
        "spawn":{"disbanded":0,"byUnit":[],"pending":0},"retinue":{"squads":[]},"population":{"families":1,"regions":[]},
        "mercenaries":{"slots":[],"hiredMine":0,"hiredAi":0,"refunded":0,"skipped":[]}})");
    CHECK(s.has_value());
    CHECK(s->resources.has_value() && s->resources->empty() && s->resourceIds->empty() && s->regions->empty() && s->playerRegions->empty());
    CHECK(s->spawn->byUnit.empty() && s->retinue->squads.empty() && s->population->families == 1 && s->population->regions.empty());
    CHECK(s->mercenaries->slots.empty() && s->mercenaries->skipped.empty());
    // 형식이 다른 항목은 건너뛴다
    s = parseStatus(R"({"heartbeat":10,"regions":[5,{"key":"a","values":[]}],"playerRegions":"x","retinue":{"squads":[1,{"id":"x"}]}})");
    CHECK(s.has_value() && s->regions->size() == 1 && (*s->regions)[0].values.empty() && !s->playerRegions);
    CHECK(s->retinue->squads.size() == 1 && s->retinue->squads[0].id == 0);
}
