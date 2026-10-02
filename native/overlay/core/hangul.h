#pragma once
#include <string>
#include <vector>

// 두벌식 한글 조합기. 시스템 입력기(IME)에 기대지 않는다.
// 게임 창은 IME 가 꺼져 있고 언리얼이 TSF 를 직접 써서, 창의 IME 를 다시 켜도 조합이 오버레이로 오지 않는다
// (analysis/findings.md "게임 안 오버레이 창 — 한글 입력"). 영문 글쇠는 그대로 들어오므로 그것을 받아 조합한다.
namespace mlt::ov {

// 글자 칸에 할 일: 커서 앞의 바이트 eraseBefore 개를 지우고 그 자리에 text(UTF-8)를 넣는다
struct HangulEdit {
    int eraseBefore = 0;
    std::string text;
};

class HangulComposer {
public:
    // 두벌식 자판의 글쇠인가(영문자)
    static bool isKey(unsigned c);

    // 글쇠 하나. shift 는 윗글쇠를 누른 채였는가(쌍자음, ㅒ, ㅖ). c 의 대소문자는 보지 않는다(Caps Lock 과 무관하게)
    HangulEdit key(char c, bool shift);
    // 지우기 글쇠. 조합 중이면 자모 하나를 지운 결과를 edit 에 넣고 true. 조합 중이 아니면 false(호출자가 평소대로 지운다)
    bool backspace(HangulEdit& edit);
    // 조합을 끝낸다. 조합 중이던 글자는 글자 칸에 그대로 남는다
    void commit();

    bool composing() const { return lead_ != 0 || vowel_ >= 0; }
    // 조합 중인 글자(한 글자). 글자 칸에서는 커서 바로 앞에 있다
    std::string preedit() const;

private:
    void consonant(char32_t c, std::string& committed);
    void vowel(int v, std::string& committed);

    char32_t lead_ = 0;    // 첫소리(호환 자모 코드). 없으면 0
    int vowel_ = -1;       // 가운뎃소리 번호(0~20). 없으면 -1
    int vowelFirst_ = -1;  // 겹모음이면 먼저 친 모음(지우기 글쇠로 되돌릴 때 쓴다)
    char32_t tail_ = 0;    // 받침
    char32_t tail2_ = 0;   // 겹받침의 둘째 자음
};

// 글자 칸 하나에 조합기를 붙인다. 글과 커서는 글자 칸(ImGui)이 갖고 있고, 프레임마다 apply 로 맞춘다
class HangulField {
public:
    // 한글 모드에서 친 글자를 순서대로 쌓는다. 영문자는 자모가 되고, 나머지는 조합을 끝낸 뒤 그대로 들어간다
    void push(unsigned c, bool shift);
    // 쌓인 글자를 text 의 cursor(바이트 위치)에 적용한다. 그 전에, 지난번 apply 뒤로 글이나 커서가 밖에서 바뀌었으면
    // (화살표, 마우스, 붙여넣기, 골라 둔 범위) 조합을 끝낸다. 글자 칸이 지우기 글쇠로 조합 글자를 통째로 지웠으면
    // (backspace) 자모 하나만 지운 모습으로 되돌린다. maxBytes 를 넘기는 글쇠는 버린다. 글을 바꿨으면 true
    bool apply(std::string& text, int& cursor, size_t maxBytes, bool backspace, bool hasSelection);
    // 조합을 끝내고 쌓인 글자를 버린다(글자 칸을 떠났거나 한/영을 바꿨을 때)
    void reset();
    bool composing() const { return composer_.composing(); }
    // 아직 적용하지 않은 글자가 쌓여 있는가
    bool pending() const { return !keys_.empty(); }

private:
    struct Key {
        unsigned c;
        bool shift;
    };
    HangulComposer composer_;
    std::vector<Key> keys_;
    int cursor_ = -1;      // 마지막 apply 뒤의 커서와 글의 길이. 달라져 있으면 밖에서 바뀐 것이다
    size_t length_ = 0;
};

}
