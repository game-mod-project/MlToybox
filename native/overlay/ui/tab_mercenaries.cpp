#include "overlay/core/merc_rules.h"
#include "overlay/core/view.h"
#include "tabs.h"
#include "widgets.h"
#include <algorithm>
#include <imgui.h>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace mlt::ov {

namespace {
const ImVec4 kErrorColor(0.95f, 0.40f, 0.35f, 1.0f);

int infantryIndex() {
    for (size_t i = 0; i < units().size(); ++i) {
        if (units()[i].id == "mercenary_infantry") return static_cast<int>(i);
    }
    return 0;
}

// 편집 영역. "등록"을 누르기 전에는 문서에 반영하지 않는다
struct Editor {
    int editing = -1;                   // 고치고 있는 용병단의 자리. -1 = 새 용병단
    MercCompany loaded;                 // 그 자리에 있던 내용. 밖에서 목록이 바뀌었는지 볼 때 쓴다(core/merc_rules 의 locateCompany)
    std::string name;
    std::vector<std::string> units;     // 분대마다 병종 id 하나
    int cost = 1000;
    std::optional<std::string> region;
    std::optional<std::string> banner;
    int unit = infantryIndex();         // "분대 추가"에서 고른 병종
    int count = 1;
    std::string group;                  // 구성 목록에서 고른 병종 id
    std::string message;                // 등록·사용 체크의 결과나 이유
    bool messageIsError = false;

    void say(std::string text, bool error) {
        message = std::move(text);
        messageIsError = error;
    }
    void load(const MercCompany& c, int index) {
        editing = index;
        loaded = c;
        name = c.name;
        units = c.units;
        cost = std::clamp(c.cost, 0, 10000000);
        region = c.region;
        banner = c.banner;
        group.clear();
        message.clear();
    }
    void clear() {
        editing = -1;
        name.clear();
        units.clear();
        cost = 1000;
        region.reset();
        banner.reset();
        group.clear();
        message.clear();
    }
};
Editor g_editor;

std::vector<ScopeOption> unitChoices() {
    std::vector<ScopeOption> options;
    for (const UnitOption& u : units()) options.push_back({ u.id, u.label });
    return options;
}

const std::string& labelOf(const std::vector<ScopeOption>& options, const std::optional<std::string>& key) {
    return options[static_cast<size_t>(indexOfKey(options, key))].label;
}

// 표: 사용 / 이름 / 구성 / 고용비 / 도착 영지 / 깃발, 그리고 "새 용병단"·"삭제". 문서를 바꿨으면 true
bool drawList(MercSettings& m, const std::vector<ScopeOption>& regions) {
    Editor& e = g_editor;
    bool changed = false;
    if (m.companies.empty()) {
        ImGui::TextDisabled("등록한 용병단이 없습니다");
    } else if (ImGui::BeginTable("##companies", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("사용", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("이름");
        ImGui::TableSetupColumn("구성");
        ImGui::TableSetupColumn("고용비");
        ImGui::TableSetupColumn("도착 영지");
        ImGui::TableSetupColumn("깃발");
        ImGui::TableHeadersRow();
        for (int i = 0; i < static_cast<int>(m.companies.size()); ++i) {
            const MercCompany& c = m.companies[static_cast<size_t>(i)];
            ImGui::PushID(i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            bool use = c.enabled;
            if (ImGui::Checkbox("##use", &use)) {
                // 등록 수에는 제한이 없고 "사용"은 최대 3개다. 사용 중인 것만 고용 창에 올라간다(규칙은 core/merc_rules)
                if (auto reason = setCompanyEnabled(m.companies, i, use)) {
                    e.say(*reason, true);
                } else {
                    e.message.clear();
                    changed = true;
                }
            }
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            if (ImGui::Selectable((c.name + "##name").c_str(), e.editing == i)) e.load(c, i);   // 줄을 누르면 편집 영역에 싣는다
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(unitSummary(c.units).c_str());
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(formatThousands(c.cost).c_str());
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(labelOf(regions, c.region).c_str());
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(c.banner ? c.banner->c_str() : "-");
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (ImGui::Button("새 용병단")) e.clear();
    ImGui::SameLine();
    ImGui::BeginDisabled(e.editing < 0);
    if (ImGui::Button("삭제")) {
        m.companies.erase(m.companies.begin() + e.editing);
        e.clear();
        changed = true;
    }
    ImGui::EndDisabled();
    return changed;
}

void field(const char* label) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(scaled(90.0f));
}

// 구성 목록: 병종마다 한 줄("용병 - 보병 × 2"), 처음 넣은 순서
void drawComposition(Editor& e) {
    std::vector<std::string> order;
    for (const std::string& unit : e.units) {
        if (std::find(order.begin(), order.end(), unit) == order.end()) order.push_back(unit);
    }
    if (ImGui::BeginListBox("##composition", ImVec2(scaled(260.0f), scaled(76.0f)))) {
        for (const std::string& unit : order) {
            const auto count = std::count(e.units.begin(), e.units.end(), unit);
            const std::string label = unitLabel(unit) + " × " + std::to_string(count) + "##" + unit;
            if (ImGui::Selectable(label.c_str(), e.group == unit)) e.group = unit;
        }
        ImGui::EndListBox();
    }
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::BeginDisabled(e.group.empty());
    if (ImGui::Button("선택 병종 빼기")) {
        e.units.erase(std::remove(e.units.begin(), e.units.end(), e.group), e.units.end());
        e.group.clear();
    }
    ImGui::EndDisabled();
    ImGui::Text("합계 %d / %d", static_cast<int>(e.units.size()), kMercMaxSquads);
    ImGui::EndGroup();
}

// 문서를 바꿨으면 true("등록"이 성공했을 때만)
bool drawEditor(MercSettings& m, const std::vector<ScopeOption>& regions) {
    static const std::vector<ScopeOption> unitOptions = unitChoices();
    static const std::vector<ScopeOption> banners = bannerOptions();
    Editor& e = g_editor;

    field("이름");
    textField("##name", e.name, static_cast<size_t>(kMercNameMax) * 4, scaled(260.0f));   // 40자. 한 글자는 4바이트까지

    field("분대 추가");
    comboOptions("##unit", unitOptions, e.unit, scaled(180.0f));
    ImGui::SameLine();
    numberField("##count", e.count, 1, kMercMaxSquads, scaled(50.0f));
    ImGui::SameLine();
    ImGui::TextUnformatted("개 분대");
    ImGui::SameLine();
    if (ImGui::Button("추가")) {
        if (static_cast<int>(e.units.size()) + e.count > kMercMaxSquads) {
            e.say("분대는 " + std::to_string(kMercMaxSquads) + "개까지입니다.", true);
        } else {
            e.units.insert(e.units.end(), static_cast<size_t>(e.count), units()[static_cast<size_t>(e.unit)].id);
            e.message.clear();
        }
    }

    field("구성");
    drawComposition(e);

    field("고용비");
    numberField("##cost", e.cost, 0, 10000000, scaled(110.0f));

    field("도착 영지");
    int region = indexOfKey(regions, e.region);
    comboOptions("##region", regions, region, scaled(220.0f));
    e.region = regions[static_cast<size_t>(region)].key;

    field("깃발");
    int banner = indexOfKey(banners, e.banner);
    comboOptions("##banner", banners, banner, scaled(220.0f));
    e.banner = banners[static_cast<size_t>(banner)].key;

    bool changed = false;
    if (ImGui::Button("등록")) {
        // 검증하고 넣거나 고친다(규칙은 core/merc_rules). 실패하면 문서를 바꾸지 않고 이유를 보여 준다
        MercCompany draft;
        draft.name = e.name;
        draft.units = e.units;
        draft.cost = e.cost;
        draft.region = e.region;
        draft.banner = e.banner;
        const MercRegistration result = registerCompany(m.companies, e.editing, std::move(draft));
        e.say(result.message, !result.ok);
        if (result.ok) {
            e.editing = result.index;
            e.name = m.companies[static_cast<size_t>(result.index)].name;
            changed = true;
        }
    }
    if (!e.message.empty()) {
        ImGui::SameLine();
        if (e.messageIsError) ImGui::TextColored(kErrorColor, "%s", e.message.c_str());
        else ImGui::TextUnformatted(e.message.c_str());
    }
    return changed;
}
}

void drawMercenariesTab(TabContext& ctx) {
    App& a = ctx.app;
    Editor& e = g_editor;
    MercSettings m = a.control.mercenaries();
    // 패널이나 손 편집으로 목록이 바뀌면 자리 번호가 다른 용병단을 가리킨다. 실었던 내용으로 다시 찾고,
    // 없어졌거나 내용이 바뀌었으면 편집 대상에서 푼다(그대로 "삭제"·"등록"하면 엉뚱한 용병단이 지워지거나 덮인다)
    if (e.editing >= 0) {
        e.editing = locateCompany(m.companies, e.editing, e.loaded);
        if (e.editing < 0) e.say("편집하던 용병단이 밖에서 바뀌었습니다. 표에서 다시 고르세요.", true);
    }
    const StatusDoc* live = ctx.inGame ? ctx.status : nullptr;                // 게임 안일 때만 게임 상태를 쓴다

    bool changed = false;
    changed |= ImGui::Checkbox("용병 기능 사용 (고용 창 자동 보충)", &m.enabled);
    changed |= ImGui::Checkbox("내 용병단 고용비 환급, 유지비 0", &m.refund);
    changed |= ImGui::Checkbox("커스텀 용병단 AI 잠금 (고용 창을 열 때만 설정한 고용비)", &m.lockFromAi);

    // 도착 영지 선택지. 등록된 용병단의 도착 영지와 지금 고른 값은 영지 목록에 없어도 남긴다
    // (게임 밖에서 용병단을 고쳐 등록해도 저장된 도착 영지가 "내 첫 영지"로 바뀌지 않게)
    static const std::vector<RegionInfo> noRegions;
    std::vector<std::optional<std::string>> keep;
    for (const MercCompany& c : m.companies) keep.push_back(c.region);
    keep.push_back(e.region);
    const std::vector<ScopeOption> regions = mercRegionOptions(live && live->playerRegions ? *live->playerRegions : noRegions, keep);

    ImGui::SeparatorText(("등록한 용병단 (사용 최대 " + std::to_string(kMercMaxEnabled) + "개)").c_str());
    changed |= drawList(m, regions);
    ImGui::SeparatorText("용병단 편집");
    changed |= drawEditor(m, regions);
    if (changed) {
        a.control.setMercenaries(m);
        markDirty(a);
        // 방금 내가 바꾼 것은 밖에서 바뀐 것이 아니다. 문서에 들어간 모습으로 다시 기억한다
        const std::vector<MercCompany> saved = a.control.mercenaries().companies;
        if (e.editing >= 0 && e.editing < static_cast<int>(saved.size())) e.loaded = saved[static_cast<size_t>(e.editing)];
    }

    ImGui::Spacing();
    ImGui::PushTextWrapPos(0.0f);
    for (const std::string& line : mercStatusLines(live && live->mercenaries ? &*live->mercenaries : nullptr)) ImGui::TextUnformatted(line.c_str());
    ImGui::PopTextWrapPos();
}

}
