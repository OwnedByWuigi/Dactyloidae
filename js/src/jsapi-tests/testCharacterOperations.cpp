/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/CharacterOperations.h"

#include <stdio.h>

#ifndef JS_CHARACTER_OPERATIONS_STANDALONE
# include "jsapi-tests/tests.h"
# include "jswin.h"
#elif defined(_WIN32)
# include <windows.h>
#endif

template <typename CharT>
static bool
CheckCharacterSearch()
{
    CharT buffer[128];
    const uint32_t needles[] = {0, 0x7f, 0x80, 0xff, 0x100, 0xd800, 0xdc00, 0xffff};
    for (uint32_t value : needles) {
        if (sizeof(CharT) == 1 && value > 0xff)
            continue;
        const CharT needle = CharT(value);
        const CharT other = CharT(value ^ 1);
        for (size_t offset = 0; offset < 16 / sizeof(CharT); ++offset) {
            CharT* text = buffer + offset;
            for (size_t length = 0; length <= 96; ++length) {
                for (size_t i = 0; i <= length; ++i)
                    text[i] = other;
                // A match just outside the span must never be returned.
                text[length] = needle;
                if (js::FindCharacter(text, length, needle))
                    return false;
                for (size_t position = 0; position < length; ++position) {
                    text[position] = needle;
                    text[length - 1] = needle;
                    if (js::FindCharacter(text, length, needle) != text + position)
                        return false;
                    text[position] = other;
                    text[length - 1] = other;
                }
            }
        }
    }
    return true;
}

static bool
CheckLatin1Detection()
{
    char16_t buffer[128];
    const char16_t nonLatin1[] = {0x100, 0x8000, 0xd800, 0xdc00, 0xffff};
    for (size_t offset = 0; offset < 8; ++offset) {
        char16_t* text = buffer + offset;
        for (size_t length = 0; length <= 96; ++length) {
            for (size_t i = 0; i < length; ++i)
                text[i] = (i & 1) ? 0xff : 0x80;
            text[length] = 0xffff;
            if (!js::CharactersFitInLatin1(text, length))
                return false;
            for (char16_t invalid : nonLatin1) {
                for (size_t position = 0; position < length; ++position) {
                    const char16_t saved = text[position];
                    text[position] = invalid;
                    if (js::CharactersFitInLatin1(text, length))
                        return false;
                    text[position] = saved;
                }
            }
        }
    }
    return true;
}

#ifdef _WIN32
static bool
CheckCharacterPageBoundary()
{
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    char* pages = static_cast<char*>(VirtualAlloc(nullptr, 2 * info.dwPageSize,
                                                MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!pages)
        return false;
    DWORD oldProtection;
    char* end = pages + info.dwPageSize;
    bool ok = !!VirtualProtect(end, info.dwPageSize, PAGE_NOACCESS, &oldProtection);
    if (ok) {
        for (size_t length = 0; length <= 64; ++length) {
            char* bytes = end - length;
            for (size_t i = 0; i < length; ++i)
                bytes[i] = 'a';
            ok = ok && !js::FindCharacter(bytes, length, 'b');
            if (length) {
                bytes[length - 1] = 'b';
                ok = ok && js::FindCharacter(bytes, length, 'b') == bytes + length - 1;
            }
            char16_t* wide = reinterpret_cast<char16_t*>(end) - length;
            for (size_t i = 0; i < length; ++i)
                wide[i] = 0xff;
            ok = ok && !js::FindCharacter(wide, length, char16_t(0x100));
            ok = ok && js::CharactersFitInLatin1(wide, length);
            if (length) {
                wide[length - 1] = 0x100;
                ok = ok && js::FindCharacter(wide, length, char16_t(0x100)) == wide + length - 1;
                ok = ok && !js::CharactersFitInLatin1(wide, length);
            }
        }
    }
    VirtualFree(pages, 0, MEM_RELEASE);
    return ok;
}
#endif

static bool
CheckCharacterOperations()
{
    return CheckCharacterSearch<char>() && CheckCharacterSearch<unsigned char>() &&
           CheckCharacterSearch<char16_t>() && CheckLatin1Detection()
#ifdef _WIN32
           && CheckCharacterPageBoundary()
#endif
           ;
}

// Allow testing these native helpers without rebuilding/linking SpiderMonkey.
#ifdef JS_CHARACTER_OPERATIONS_STANDALONE
int main()
{
    if (!CheckCharacterOperations()) {
        fprintf(stderr, "Character operation regression test failed\n");
        return 1;
    }
    return 0;
}
#else
BEGIN_TEST(testCharacterOperations)
{
    CHECK(CheckCharacterOperations());
    return true;
}
END_TEST(testCharacterOperations)
#endif
