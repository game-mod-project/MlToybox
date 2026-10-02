#include "hangul.h"

namespace mlt::ov {

namespace {
// 자음은 호환 자모 코드(U+3131 ㄱ ~ U+314E ㅎ)로 다룬다. 그 순서대로의 첫소리 번호와 받침 번호(없으면 -1)
constexpr char32_t kFirstConsonant = 0x3131;
constexpr int kLead[30] = { 0, 1, -1, 2, -1, -1, 3, 4, 5, -1, -1, -1, -1, -1, -1, -1, 6, 7, 8, -1, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18 };
constexpr int kTail[30] = { 1, 2, 3, 4, 5, 6, 7, -1, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, -1, 18, 19, 20, 21, 22, -1, 23, 24, 25, 26, 27 };
constexpr char32_t kFirstVowel = 0x314F;   // ㅏ. 모음은 가운뎃소리 번호(0~20)로 다룬다: 코드 = kFirstVowel + 번호

constexpr char32_t G = 0x3131, GG = 0x3132, N = 0x3134, D = 0x3137, DD = 0x3138, R = 0x3139, M = 0x3141, B = 0x3142, BB = 0x3143,
    S = 0x3145, SS = 0x3146, NG = 0x3147, J = 0x3148, JJ = 0x3149, C = 0x314A, K = 0x314B, T = 0x314C, P = 0x314D, H = 0x314E;

int leadIndex(char32_t c) { return kLead[c - kFirstConsonant]; }
int tailIndex(char32_t c) { return kTail[c - kFirstConsonant]; }

// 겹받침(호환 자모 코드). 없으면 0
char32_t compoundTail(char32_t first, char32_t second) {
    if (first == G && second == S) return 0x3133;   // ㄳ
    if (first == N && second == J) return 0x3135;   // ㄵ
    if (first == N && second == H) return 0x3136;   // ㄶ
    if (first == R) {
        if (second == G) return 0x313A;             // ㄺ
        if (second == M) return 0x313B;             // ㄻ
        if (second == B) return 0x313C;             // ㄼ
        if (second == S) return 0x313D;             // ㄽ
        if (second == T) return 0x313E;             // ㄾ
        if (second == P) return 0x313F;             // ㄿ
        if (second == H) return 0x3140;             // ㅀ
    }
    if (first == B && second == S) return 0x3144;   // ㅄ
    return 0;
}

// 겹모음의 가운뎃소리 번호. 없으면 -1
int compoundVowel(int first, int second) {
    if (first == 8) {                                // ㅗ
        if (second == 0) return 9;                   // ㅘ
        if (second == 1) return 10;                  // ㅙ
        if (second == 20) return 11;                 // ㅚ
    }
    if (first == 13) {                               // ㅜ
        if (second == 4) return 14;                  // ㅝ
        if (second == 5) return 15;                  // ㅞ
        if (second == 20) return 16;                 // ㅟ
    }
    if (first == 18 && second == 20) return 19;      // ㅢ
    return -1;
}

struct Jamo {
    char32_t consonant = 0;   // 자음이면 그 코드
    int vowel = -1;           // 모음이면 가운뎃소리 번호
};

// 두벌식 자판. 글쇠는 소문자로 받는다
Jamo jamoFor(char key, bool shift) {
    switch (key) {
    case 'q': return { shift ? BB : B };
    case 'w': return { shift ? JJ : J };
    case 'e': return { shift ? DD : D };
    case 'r': return { shift ? GG : G };
    case 't': return { shift ? SS : S };
    case 'a': return { M };
    case 's': return { N };
    case 'd': return { NG };
    case 'f': return { R };
    case 'g': return { H };
    case 'z': return { K };
    case 'x': return { T };
    case 'c': return { C };
    case 'v': return { P };
    case 'y': return { 0, 12 };                      // ㅛ
    case 'u': return { 0, 6 };                       // ㅕ
    case 'i': return { 0, 2 };                       // ㅑ
    case 'o': return { 0, shift ? 3 : 1 };           // ㅒ ㅐ
    case 'p': return { 0, shift ? 7 : 5 };           // ㅖ ㅔ
    case 'h': return { 0, 8 };                       // ㅗ
    case 'j': return { 0, 4 };                       // ㅓ
    case 'k': return { 0, 0 };                       // ㅏ
    case 'l': return { 0, 20 };                      // ㅣ
    case 'b': return { 0, 17 };                      // ㅠ
    case 'n': return { 0, 13 };                      // ㅜ
    default: return { 0, 18 };                       // m: ㅡ
    }
}

void appendUtf8(std::string& out, char32_t c) {
    if (c < 0x80) {
        out += static_cast<char>(c);
    } else if (c < 0x800) {
        out += static_cast<char>(0xC0 | (c >> 6));
        out += static_cast<char>(0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
        out += static_cast<char>(0xE0 | (c >> 12));
        out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (c & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (c >> 18));
        out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (c & 0x3F));
    }
}
}

bool HangulComposer::isKey(unsigned c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

std::string HangulComposer::preedit() const {
    std::string out;
    if (lead_ && vowel_ >= 0) {
        const int tail = tail2_ ? tailIndex(compoundTail(tail_, tail2_)) : (tail_ ? tailIndex(tail_) : 0);
        appendUtf8(out, static_cast<char32_t>(0xAC00 + (leadIndex(lead_) * 21 + vowel_) * 28 + tail));
    } else if (lead_) {
        appendUtf8(out, lead_);
    } else if (vowel_ >= 0) {
        appendUtf8(out, kFirstVowel + static_cast<char32_t>(vowel_));
    }
    return out;
}

void HangulComposer::commit() {
    lead_ = 0;
    vowel_ = -1;
    vowelFirst_ = -1;
    tail_ = 0;
    tail2_ = 0;
}

// 자음 하나. 조합이 끝나 확정된 글자는 committed 에 더한다
void HangulComposer::consonant(char32_t c, std::string& committed) {
    if (lead_ && vowel_ >= 0) {
        if (!tail_) {
            if (tailIndex(c) > 0) {
                tail_ = c;                        // 받침
                return;
            }
        } else if (!tail2_ && compoundTail(tail_, c)) {
            tail2_ = c;                           // 겹받침
            return;
        }
    }
    committed += preedit();                       // 낱자음, 낱모음, 다 찬 글자 뒤의 자음은 새 글자의 첫소리
    commit();
    lead_ = c;
}

void HangulComposer::vowel(int v, std::string& committed) {
    if (lead_ && vowel_ < 0) {                    // 첫소리 + 모음
        vowel_ = v;
        return;
    }
    if (vowel_ >= 0 && !tail_) {                  // 모음 뒤의 모음: 겹모음이 되면 합친다(한 번만)
        const int both = vowelFirst_ < 0 ? compoundVowel(vowel_, v) : -1;
        if (both >= 0) {
            vowelFirst_ = vowel_;
            vowel_ = both;
            return;
        }
    }
    // 받침이 있으면 마지막 받침이 다음 글자의 첫소리로 넘어간다
    char32_t next = 0;
    if (tail2_) {
        next = tail2_;
        tail2_ = 0;
    } else if (tail_) {
        next = tail_;
        tail_ = 0;
    }
    committed += preedit();
    commit();
    lead_ = next;
    vowel_ = v;
}

HangulEdit HangulComposer::key(char c, bool shift) {
    HangulEdit edit;
    edit.eraseBefore = static_cast<int>(preedit().size());
    const Jamo jamo = jamoFor(c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c, shift);
    if (jamo.vowel >= 0) vowel(jamo.vowel, edit.text);
    else consonant(jamo.consonant, edit.text);
    edit.text += preedit();
    return edit;
}

bool HangulComposer::backspace(HangulEdit& edit) {
    if (!composing()) return false;
    edit.eraseBefore = static_cast<int>(preedit().size());
    if (tail2_) tail2_ = 0;
    else if (tail_) tail_ = 0;
    else if (vowelFirst_ >= 0) {
        vowel_ = vowelFirst_;
        vowelFirst_ = -1;
    } else if (vowel_ >= 0) vowel_ = -1;
    else lead_ = 0;
    edit.text = preedit();
    return true;
}

void HangulField::push(unsigned c, bool shift) {
    keys_.push_back({ c, shift });
}

void HangulField::reset() {
    composer_.commit();
    keys_.clear();
    cursor_ = -1;
    length_ = 0;
}

bool HangulField::apply(std::string& text, int& cursor, size_t maxBytes, bool backspace, bool hasSelection) {
    bool changed = false;
    if (cursor < 0) cursor = 0;
    if (cursor > static_cast<int>(text.size())) cursor = static_cast<int>(text.size());

    if (composer_.composing()) {
        const int pre = static_cast<int>(composer_.preedit().size());
        const bool untouched = !hasSelection && cursor == cursor_ && text.size() == length_;
        if (!untouched) {
            // 글자 칸이 지우기 글쇠를 먼저 처리해 조합 글자를 통째로 지운 경우: 자모 하나만 지운 모습을 다시 넣는다
            const bool erased = backspace && !hasSelection && cursor == cursor_ - pre && text.size() + static_cast<size_t>(pre) == length_;
            HangulEdit edit;
            if (erased && composer_.backspace(edit)) {
                text.insert(static_cast<size_t>(cursor), edit.text);
                cursor += static_cast<int>(edit.text.size());
                changed = !edit.text.empty();
            } else {
                composer_.commit();   // 커서를 옮겼거나 글이 밖에서 바뀌었다. 조합 중이던 글자는 그대로 둔다
            }
        }
    }

    for (const Key& key : keys_) {
        if (HangulComposer::isKey(key.c)) {
            const HangulComposer before = composer_;
            const HangulEdit edit = composer_.key(static_cast<char>(key.c), key.shift);
            const int erase = edit.eraseBefore <= cursor ? edit.eraseBefore : 0;
            if (text.size() - static_cast<size_t>(erase) + edit.text.size() > maxBytes) {
                composer_ = before;   // 넘친다. 이 글쇠는 없던 것으로 한다
                continue;
            }
            text.replace(static_cast<size_t>(cursor - erase), static_cast<size_t>(erase), edit.text);
            cursor += static_cast<int>(edit.text.size()) - erase;
            changed = true;
        } else {
            composer_.commit();
            std::string raw;
            appendUtf8(raw, static_cast<char32_t>(key.c));
            if (text.size() + raw.size() > maxBytes) continue;
            text.insert(static_cast<size_t>(cursor), raw);
            cursor += static_cast<int>(raw.size());
            changed = true;
        }
    }
    keys_.clear();
    cursor_ = cursor;
    length_ = text.size();
    return changed;
}

}
