#include "test.h"
#include "overlay/core/hangul.h"
#include <string>

using namespace mlt::ov;

// 두벌식 한글 조합기. 게임 창은 IME 가 꺼져 있고 언리얼이 TSF 를 직접 써서 시스템 입력기의 조합이 오버레이로 오지 않는다
// (2026-10-02 실측). 그래서 영문 글쇠를 받아 직접 조합한다.
namespace {
// 글쇠를 차례로 친 뒤 글자 칸에 남는 글. 대문자는 윗글쇠(Shift)를 누른 것
std::string typed(const std::string& keys) {
    HangulComposer c;
    std::string text;
    for (char k : keys) {
        const HangulEdit e = c.key(k, k >= 'A' && k <= 'Z');
        text.erase(text.size() - static_cast<size_t>(e.eraseBefore));
        text += e.text;
    }
    return text;
}
}

TEST(overlay_hangul_composes_syllables_from_two_set_keys) {
    CHECK(typed("r") == "ㄱ");                    // 첫소리만
    CHECK(typed("rk") == "가");
    CHECK(typed("rkr") == "각");                  // 받침
    CHECK(typed("dkssud") == "안녕");             // 받침 뒤의 자음은 다음 글자의 첫소리
    CHECK(typed("gksrmf") == "한글");
    CHECK(typed("xhdlqkrtm dydqudeks").empty() == false);
    CHECK(typed("xhdlqkrtm") == "토이박스");
    CHECK(typed("dydqudeks") == "용병단");
    CHECK(typed("rjatkeo") == "검사대");
}

TEST(overlay_hangul_moves_a_final_consonant_to_the_next_syllable_before_a_vowel) {
    CHECK(typed("rkrk") == "가가");               // 각 + ㅏ -> 가가
    CHECK(typed("ekfr") == "닭");                 // 겹받침 ㄺ
    CHECK(typed("ekfrl") == "달기");              // 겹받침의 둘째 자음만 넘어간다
    CHECK(typed("rkqt") == "값");
    CHECK(typed("rkqtl") == "갑시");
    CHECK(typed("rkqtd") == "값ㅇ");              // 겹받침 뒤의 자음은 새 글자
    CHECK(typed("dkss") == "안ㄴ");               // ㄴㄴ 은 겹받침이 아니다
}

TEST(overlay_hangul_combines_vowels_and_uses_shift_for_double_consonants) {
    CHECK(typed("dhk") == "와");                  // ㅗ + ㅏ
    CHECK(typed("dnpf") == "웰");                 // ㅜ + ㅔ
    CHECK(typed("dml") == "의");
    CHECK(typed("dhl") == "외");
    CHECK(typed("dho") == "왜");
    CHECK(typed("dkk") == "아ㅏ");                // 겹모음이 아니면 새 글자
    CHECK(typed("Rk") == "까");
    CHECK(typed("Tkd") == "쌍");
    CHECK(typed("dlT") == "있");                  // 쌍자음 받침
    CHECK(typed("dkE") == "아ㄸ");                // ㄸ 은 받침이 될 수 없다
    CHECK(typed("OP") == "ㅒㅖ");
    CHECK(typed("A") == "ㅁ");                    // 쌍자음이 없는 글쇠는 윗글쇠를 눌러도 같다
    CHECK(typed("k") == "ㅏ");                    // 모음만
    CHECK(typed("hk") == "ㅘ");
    CHECK(typed("kr") == "ㅏㄱ");
}

TEST(overlay_hangul_backspace_removes_one_jamo) {
    HangulComposer c;
    for (char k : std::string("ekfr")) c.key(k, false);
    CHECK(c.preedit() == "닭");
    HangulEdit e;
    CHECK(c.backspace(e) && e.eraseBefore == 3 && e.text == "달");
    CHECK(c.backspace(e) && e.text == "다");
    CHECK(c.backspace(e) && e.text == "ㄷ");
    CHECK(c.backspace(e) && e.text.empty() && !c.composing());
    CHECK(!c.backspace(e));                       // 조합 중이 아니면 호출자가 평소대로 지운다

    for (char k : std::string("dhk")) c.key(k, false);
    CHECK(c.backspace(e) && e.text == "오");      // 겹모음은 먼저 친 모음으로 돌아간다
    c.commit();
    CHECK(!c.composing() && c.preedit().empty());
    CHECK(HangulComposer::isKey('a') && HangulComposer::isKey('Z') && !HangulComposer::isKey('1') && !HangulComposer::isKey(' ') && !HangulComposer::isKey(0xAC00));
}

// 글자 칸에 붙인 모습: 글과 커서는 글자 칸(ImGui)이 갖고 있고, 프레임마다 쌓인 글쇠를 적용한다
TEST(overlay_hangul_field_types_into_the_text_at_the_cursor) {
    HangulField f;
    std::string text = "AB";
    int cursor = 1;                               // A 와 B 사이
    for (char k : std::string("rkq")) f.push(static_cast<unsigned>(k), false);
    CHECK(f.apply(text, cursor, 100, false, false));
    CHECK(text == "A갑B" && cursor == 4 && f.composing());
    f.push('t', false);
    f.push('l', false);
    f.apply(text, cursor, 100, false, false);
    CHECK(text == "A갑시B" && cursor == 7);
    f.push(' ', false);                           // 글쇠가 아닌 글자는 조합을 끝내고 그대로 들어간다
    f.push('1', false);
    f.push('r', false);
    f.apply(text, cursor, 100, false, false);
    CHECK(text == "A갑시 1ㄱB" && cursor == 12);
    CHECK(!f.apply(text, cursor, 100, false, false));   // 쌓인 글쇠가 없으면 바꾸지 않는다
}

TEST(overlay_hangul_field_ends_the_composition_when_the_text_changes_outside) {
    HangulField f;
    std::string text;
    int cursor = 0;
    f.push('r', false);
    f.push('k', false);
    f.apply(text, cursor, 100, false, false);
    CHECK(text == "가" && f.composing());
    cursor = 0;                                   // 화살표나 마우스로 커서를 옮겼다
    f.push('s', false);
    f.apply(text, cursor, 100, false, false);
    CHECK(text == "ㄴ가" && cursor == 3);         // 앞 글자에 받침으로 붙지 않는다

    f.reset();
    text = "가";
    cursor = 3;
    f.push('s', false);
    f.apply(text, cursor, 100, false, false);
    CHECK(text == "가ㄴ");
    text = "가ㄴX";                               // 붙여넣기 등으로 글이 길어졌다
    cursor = 7;
    f.push('k', false);
    f.apply(text, cursor, 100, false, false);
    CHECK(text == "가ㄴXㅏ");

    f.reset();
    text.clear();
    cursor = 0;
    f.push('r', false);
    f.apply(text, cursor, 100, false, false);
    f.push('k', false);
    f.apply(text, cursor, 100, false, true);      // 골라 둔 범위가 있다
    CHECK(text == "ㄱㅏ");
}

TEST(overlay_hangul_field_backspace_takes_back_one_jamo) {
    HangulField f;
    std::string text = "이름 ";
    int cursor = static_cast<int>(text.size());
    for (char k : std::string("ekfr")) f.push(static_cast<unsigned>(k), false);
    f.apply(text, cursor, 100, false, false);
    CHECK(text == "이름 닭");
    // 글자 칸이 지우기 글쇠를 먼저 처리해 조합 글자를 통째로 지운다. 남은 자모를 다시 넣는다
    text = "이름 ";
    cursor = static_cast<int>(text.size());
    CHECK(f.apply(text, cursor, 100, true, false));
    CHECK(text == "이름 달" && cursor == static_cast<int>(text.size()) && f.composing());
    text = "이름 ";
    cursor = static_cast<int>(text.size());
    f.apply(text, cursor, 100, true, false);
    CHECK(text == "이름 다");
    f.push('s', false);
    f.apply(text, cursor, 100, false, false);
    CHECK(text == "이름 단");                     // 지운 뒤에도 조합이 이어진다

    f.reset();
    text = "이름";
    cursor = static_cast<int>(text.size());
    f.push('r', false);
    f.apply(text, cursor, 100, false, false);
    text = "이름";                                // 낱자음 하나를 지웠다
    cursor = static_cast<int>(text.size());
    CHECK(!f.apply(text, cursor, 100, true, false));
    CHECK(text == "이름" && !f.composing());
    text = "이";                                  // 조합 중이 아닐 때의 지우기는 건드리지 않는다
    cursor = 3;
    CHECK(!f.apply(text, cursor, 100, true, false) && text == "이");
}

TEST(overlay_hangul_field_never_exceeds_the_byte_limit) {
    HangulField f;
    std::string text = "가나";                    // 6 바이트
    int cursor = 6;
    for (char k : std::string("ek")) f.push(static_cast<unsigned>(k), false);
    f.apply(text, cursor, 9, false, false);
    CHECK(text == "가나다");                      // 9 바이트에 꼭 맞는다
    f.push('f', false);                           // 받침은 길이를 늘리지 않는다
    f.apply(text, cursor, 9, false, false);
    CHECK(text == "가나달");
    f.push('k', false);                           // 달 + ㅏ = 다라: 한 글자가 늘어 넘친다. 그 글쇠는 버린다
    f.apply(text, cursor, 9, false, false);
    CHECK(text == "가나달" && cursor == 9);
    f.push('1', false);
    f.apply(text, cursor, 9, false, false);
    CHECK(text == "가나달");
}
