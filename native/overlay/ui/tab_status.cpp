#include "tabs.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <imgui.h>
#include <initializer_list>
#include <string>

namespace mlt::ov {

namespace {
std::string localTime(long long epochSeconds) {
    const std::time_t t = static_cast<std::time_t>(epochSeconds);
    std::tm tm{};
    if (localtime_s(&tm, &t) != 0) return "-";
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    return buf;
}

const char* orDash(const std::optional<std::string>& s) {
    return s ? s->c_str() : "-";
}

// 이름 / 값 … 표의 한 줄. 열 수는 BeginTable 에 준 수와 같아야 한다
void row(std::initializer_list<const char*> cells) {
    ImGui::TableNextRow();
    for (const char* cell : cells) {
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(cell);
    }
}

void drawModStatus(TabContext& ctx) {
    const StatusDoc* s = ctx.status;
    if (!s) {
        ImGui::TextUnformatted("status.json 없음");
        return;
    }
    ImGui::Text("heartbeat: %s", localTime(s->heartbeat).c_str());
    ImGui::Text("inGame: %s", s->inGame ? "true" : "false");
    if (s->appliedSeq) ImGui::Text("appliedSeq: %lld / sent: %lld", *s->appliedSeq, ctx.app.lastSentSeq);
    else ImGui::Text("appliedSeq: - / sent: %lld", ctx.app.lastSentSeq);
    ImGui::Text("bridgeError: %s", orDash(s->bridgeError));

    ImGui::SeparatorText("기능");
    if (!s->features) ImGui::TextUnformatted("(모드에 등록된 기능 없음)");
    else if (ImGui::BeginTable("##features", 3, ImGuiTableFlags_SizingFixedFit)) {
        for (const auto& [name, f] : *s->features) {
            row({ name.c_str(), f.active ? "active=true" : "active=false", (std::string("error=") + orDash(f.lastError)).c_str() });
        }
        ImGui::EndTable();
    }

    if (s->commands) {
        ImGui::SeparatorText("명령 결과");
        for (const auto& [id, r] : *s->commands) {
            std::string line = id.substr(0, 8) + (r.ok ? " 성공" : " 실패");
            if (r.squads) {
                line += " 분대";
                for (size_t i = 0; i < r.squads->size(); ++i) line += (i ? "," : " ") + std::to_string((*r.squads)[i]);
            }
            if (r.reformed) line += " 재구성 " + std::to_string(*r.reformed) + "개";
            if (r.added) line += " 가족 " + std::to_string(*r.added) + "/" + std::to_string(r.requested.value_or(0));
            if (r.error) line += " " + *r.error;
            ImGui::TextUnformatted(line.c_str());
        }
    }

    ImGui::SeparatorText("네이티브");
    if (!s->native) ImGui::TextUnformatted("(정보 없음)");
    else {
        const NativeStatus& n = *s->native;
        if (!n.loaded) ImGui::Text("미로드: %s", orDash(n.error));
        else ImGui::TextUnformatted(n.stale ? "응답 없음 (heartbeat 끊김)" : "동작 중");
        if (!n.features.empty() && ImGui::BeginTable("##native", 4, ImGuiTableFlags_SizingFixedFit)) {
            for (const auto& [name, f] : n.features) {
                row({ name.c_str(), f.installed ? "installed=true" : "installed=false", f.active ? "active=true" : "active=false",
                      (std::string("error=") + orDash(f.lastError)).c_str() });
            }
            ImGui::EndTable();
        }
    }
}

void drawOverlaySettings(App& a) {
    ImGui::SeparatorText("오버레이");
    ImGui::Text("빌드: %s", __DATE__);
    ImGui::Text("글꼴: %s", a.koreanFont.load() ? "맑은 고딕" : "기본 글꼴(한글이 표시되지 않습니다)");
    if (const long long errors = a.workerErrors.load()) ImGui::Text("작업 스레드 오류: %lld회 (저장이 늦어질 수 있습니다)", errors);

    const float width = 160.0f * ImGui::GetStyle().FontScaleMain;
    ImGui::SetNextItemWidth(width);
    if (ImGui::BeginCombo("여닫는 키", a.settings.toggleKey.c_str())) {
        for (const std::string& name : toggleKeyNames()) {
            const bool selected = name == a.settings.toggleKey;
            if (ImGui::Selectable(name.c_str(), selected) && !selected) {
                a.settings.toggleKey = name;
                syncSettingsAtoms(a);
                a.settingsDirty = true;
            }
        }
        ImGui::EndCombo();
    }

    int percent = static_cast<int>(std::lround(a.settings.scale * 100.0f));
    ImGui::SetNextItemWidth(width);
    if (ImGui::SliderInt("글자 배율", &percent, 80, 150, "%d%%")) {
        percent = std::clamp((percent + 5) / 10 * 10, 80, 150);   // 10% 단위
        const float scale = static_cast<float>(percent) / 100.0f;
        if (std::fabs(scale - a.settings.scale) > 0.001f) {
            a.settings.scale = scale;
            syncSettingsAtoms(a);
            a.settingsDirty = true;
        }
    }
}
}

void drawStatusTab(TabContext& ctx) {
    drawModStatus(ctx);
    drawOverlaySettings(ctx.app);
}

}
