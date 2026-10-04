#include "features/deposits.h"
#include "features/memory.h"
#include <algorithm>
#include <cstdio>
#include <memory>
#include <mutex>

namespace mlt::deposits {

bool isMineral(int32_t type) {
    return type == kSalt || type == kIron || type == kClay;
}

bool canBeUnderground(int32_t type) {
    return isMineral(type) || type == kStone;
}

namespace {
int pickKind(const MineralTargets& t, int32_t type) {
    switch (type) {
        case kSalt: return t.salt;
        case kIron: return t.iron;
        case kClay: return t.clay;
        default: return 0;
    }
}
}

int targetOf(const MineralTargets& common, const MineralTargets* region, int32_t type) {
    if (!isMineral(type)) return 0;
    if (region) {
        const int own = pickKind(*region, type);
        if (own >= 0) return own;
    }
    return std::max(0, pickKind(common, type));
}

int32_t topUp(int32_t amount, int target) {
    if (target <= 0) return amount;
    return std::max<int32_t>(amount, std::min(target, kMaxTarget));
}

bool process(const uint8_t* engine, const RegionControl& control, const IsRegionFn& isRegion, bool richKnown, std::vector<NodeInfo>& out) {
    out.clear();
    if (!engine) return false;
    const uint8_t* list = mem::readPtr(engine, kEngineNodesOffset);
    const int32_t count = mem::read<int32_t>(engine, kEngineNodesOffset + 8);
    if (count < 0 || count > kMaxNodes) return false;
    if (count == 0) return true;
    if (!list) return false;
    for (int32_t i = 0; i < count; ++i) {
        // 매장지는 게임의 객체다. 목록에서 읽은 포인터로 그 객체의 값을 고친다
        auto* node = const_cast<uint8_t*>(mem::readPtr(list, static_cast<std::ptrdiff_t>(i) * 8));
        if (!node) continue;
        const int32_t type = mem::read<int32_t>(node, kNodeTypeOffset);
        if (!canBeUnderground(type)) continue;
        // 광물은 게임이 남은 양을 두는 매장지만: 덩어리가 없고 동물 무리가 아니다. 돌은 양이 덩어리에 있다
        const bool mineral = isMineral(type);
        if (mem::read<int32_t>(node, kNodeSquadOffset) > 0) continue;
        if (mineral && mem::read<int32_t>(node, kNodeClumpCountOffset) != 0) continue;
        const uint8_t* region = mem::readPtr(node, kNodeRegionOffset);
        if (!region_scope::ownedByMainPlayer(region)) continue;
        NodeInfo info{ reinterpret_cast<std::uintptr_t>(node), reinterpret_cast<std::uintptr_t>(region), type, 0, std::nullopt };
        if (richKnown) {
            bool rich = mem::read<uint8_t>(node, kNodeRichOffset) != 0;
            if (!rich && control.rich) {
                mem::write<uint8_t>(node, kNodeRichOffset, 1);
                rich = true;
            }
            info.rich = rich;
        }
        if (!mineral) {
            out.push_back(info);
            continue;
        }
        const MineralTargets* own = nullptr;
        if (isRegion) {
            for (const auto& [key, targets] : control.regions) {
                if (isRegion(region, key)) { own = &targets; break; }
            }
        }
        const int32_t amount = mem::read<int32_t>(node, kNodeAmountOffset);
        const int32_t next = topUp(amount, targetOf(control.common, own, type));
        if (next != amount) mem::write<int32_t>(node, kNodeAmountOffset, next);
        info.amount = next;
        out.push_back(info);
    }
    return true;
}

std::string renderNodes(const std::vector<NodeInfo>& nodes, long long day) {
    if (nodes.empty()) return "";
    std::string out = "\"nodesDay\":" + std::to_string(day) + ",\"nodes\":[";
    for (size_t i = 0; i < nodes.size(); ++i) {
        char node[32], region[32];
        std::snprintf(node, sizeof node, "%llX", static_cast<unsigned long long>(nodes[i].node));
        std::snprintf(region, sizeof region, "%llX", static_cast<unsigned long long>(nodes[i].region));
        if (i) out += ",";
        out += std::string("{\"node\":\"") + node + "\",\"region\":\"" + region + "\",\"type\":" + std::to_string(nodes[i].type) +
               ",\"amount\":" + std::to_string(nodes[i].amount);
        if (nodes[i].rich) out += *nodes[i].rich ? ",\"rich\":true" : ",\"rich\":false";
        out += "}";
    }
    out += "]";
    return out;
}

namespace {
using NewDayFn = void(__fastcall*)(void* weather);
NewDayFn g_newDay = nullptr;
void* g_nodeLayout = nullptr;
void* g_amountWrite = nullptr;
void* g_ownerCheck = nullptr;
void* g_richLayout = nullptr;

// 작업 스레드가 설정을 바꾸고 상태를 읽는다. 게임 스레드의 detour 는 하루에 한 번 설정을 읽고 모은 것을 넣는다
std::mutex g_mutex;
std::shared_ptr<const RegionControl> g_control = std::make_shared<const RegionControl>();
std::vector<NodeInfo> g_snapshot;
long long g_snapshotDay = 0;   // 모을 때마다 1 씩 오른다

// 매장지와 영지의 오프셋은 세 함수의 본문으로 확인한다. 하나라도 못 찾았으면 게임의 메모리를 건드리지 않는다
bool ready() { return g_nodeLayout != nullptr && g_amountWrite != nullptr && g_ownerCheck != nullptr; }

void __fastcall NewDayDetour(void* weather) {
    g_newDay(weather);
    if (!ready() || !weather) return;
    std::shared_ptr<const RegionControl> control;
    {
        std::lock_guard lock(g_mutex);
        control = g_control;
    }
    if (!control->enabled) return;
    // 영지를 가릴 수 없으면(features/region_scope) 공통 목표만 쓴다
    IsRegionFn isRegion;
    if (region_scope::canTellRegions()) isRegion = &region_scope::tagIs;
    std::vector<NodeInfo> nodes;
    // 풍부 여부의 자리(deposits_rich)를 확인하지 못했으면 그 자리는 읽지도 쓰지도 않는다
    if (!process(mem::readPtr(static_cast<const uint8_t*>(weather), kWeatherEngineOffset), *control, isRegion, g_richLayout != nullptr, nodes)) return;
    std::lock_guard lock(g_mutex);
    g_snapshot = std::move(nodes);
    ++g_snapshotDay;
}

bool wanted(const NativeControl& c) { return c.region.enabled; }

void configure(const NativeControl& c) {
    std::lock_guard lock(g_mutex);
    if (!c.region.enabled) g_snapshot.clear();   // 꺼진 뒤에 예전 값을 보이지 않는다
    if (*g_control == c.region) return;
    g_control = std::make_shared<const RegionControl>(c.region);
}
}

std::string snapshotJson() {
    std::lock_guard lock(g_mutex);
    return renderNodes(g_snapshot, g_snapshotDay);
}

void registerHook(HookManager& manager) {
    HookSpec day{ "deposits_day", kNewDayPattern, reinterpret_cast<void*>(&NewDayDetour), reinterpret_cast<void**>(&g_newDay), &wanted };
    day.configure = &configure;
    day.status = &snapshotJson;   // 모은 값을 native_status.json 에 싣는다
    day.bodyChecks = {
        "49 8B 86 A8 02 00 00",                  // mov rax,[r14+2A8h]             날씨 객체의 엔진
        "41 FF 8E 2C 04 00 00",                  // dec dword [r14+42Ch]           습격까지 남은 날을 하루 줄인다(날짜가 넘어갈 때의 함수라는 표시)
    };
    day.bodyWindow = 0x600;
    manager.add(day);

    HookSpec nodes{ "deposits_nodes", kNodeLayoutPattern, nullptr, &g_nodeLayout, &wanted };
    nodes.bodyChecks = {
        "4D 8B 82 00 07 00 00",                  // mov r8,[r10+700h]              엔진의 매장지 목록
        "49 63 82 08 07 00 00",                  // movsxd rax,[r10+708h]          그 개수
        "48 39 98 B0 02 00 00",                  // cmp [rax+2B0h],rbx             매장지의 영지
        "8B 88 A8 02 00 00",                     // mov ecx,[rax+2A8h]             매장지의 종류
    };
    nodes.bodyWindow = 0x200;
    manager.add(nodes);

    // 패턴 자체가 세 오프셋(무리 번호, 덩어리 개수, 남은 양)을 담고 있다
    HookSpec amount{ "deposits_amount", kAmountWritePattern, nullptr, &g_amountWrite, &wanted };
    manager.add(amount);

    HookSpec owner{ "deposits_owner", region_scope::kOwnerCheckPattern, nullptr, &g_ownerCheck, &wanted };
    owner.bodyChecks = { region_scope::kOwnerCheckBody };   // 영지의 주인과 isMainPlayer 를 읽는 명령
    owner.bodyWindow = region_scope::kOwnerCheckWindow;
    manager.add(owner);

    HookSpec rich{ "deposits_rich", kFiniteCheckPattern, nullptr, &g_richLayout, &wanted };
    rich.bodyChecks = {
        "80 BA 2C 03 00 00 00",                  // cmp byte [rdx+32Ch],0          첫 덩어리가 철마다 다시 차는가
        "80 B9 AC 02 00 00 00",                  // cmp byte [rcx+2ACh],0          매장지의 풍부 여부
        "44 8B 83 A8 02 00 00",                  // mov r8d,[rbx+2A8h]             매장지의 종류(그 종류의 설정을 얻는다)
        "80 7C 24 21 00",                        // cmp byte [rsp+21h],0           그 종류가 풍부하면 지하 매장지가 되는가
    };
    rich.bodyWindow = 0xB0;
    manager.add(rich);
    // 영지를 설정의 영지 키와 견주는 것은 features/region_scope 의 region_name, region_tag 가 맡는다
}

}
