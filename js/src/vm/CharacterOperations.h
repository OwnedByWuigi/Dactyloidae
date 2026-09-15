/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_CharacterOperations_h
#define vm_CharacterOperations_h

#include "mozilla/MathAlgorithms.h"

#include <stddef.h>
#include <stdint.h>

// Use intrinsics only when SSE2 is part of the compiler's target baseline.
// Builds for other architectures (or pre-SSE2 x86) retain scalar operations.
#if defined(__SSE2__) || defined(_M_X64) || \
    (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
# define JS_HAS_SSE2_CHARACTER_OPERATIONS
# include <emmintrin.h>
#endif

namespace js {

template <typename CharT>
inline const CharT*
FindCharacter(const CharT* chars, size_t length, CharT match)
{
    static_assert(sizeof(CharT) == 1 || sizeof(CharT) == 2, "character width");
#ifdef JS_HAS_SSE2_CHARACTER_OPERATIONS
    const size_t lanes = 16 / sizeof(CharT);
    if (length >= lanes) {
        const __m128i needle = sizeof(CharT) == 1
                              ? _mm_set1_epi8(static_cast<char>(match))
                              : _mm_set1_epi16(static_cast<short>(match));
        do {
            // Never read beyond the supplied span, even at a page boundary.
            const __m128i block = _mm_loadu_si128(reinterpret_cast<const __m128i*>(chars));
            const __m128i equal = sizeof(CharT) == 1
                                 ? _mm_cmpeq_epi8(block, needle)
                                 : _mm_cmpeq_epi16(block, needle);
            const uint32_t mask = static_cast<uint32_t>(_mm_movemask_epi8(equal));
            if (mask)
                return chars + mozilla::CountTrailingZeroes32(mask) / sizeof(CharT);
            chars += lanes;
            length -= lanes;
        } while (length >= lanes);
    }
#endif
    for (; length; --length, ++chars) {
        if (*chars == match)
            return chars;
    }
    return nullptr;
}

inline bool
CharactersFitInLatin1(const char16_t* chars, size_t length)
{
#ifdef JS_HAS_SSE2_CHARACTER_OPERATIONS
    if (length >= 8) {
        const __m128i highBytes = _mm_set1_epi16(static_cast<short>(0xff00));
        const __m128i zero = _mm_setzero_si128();
        do {
            const __m128i block = _mm_loadu_si128(reinterpret_cast<const __m128i*>(chars));
            const __m128i fits = _mm_cmpeq_epi16(_mm_and_si128(block, highBytes), zero);
            if (_mm_movemask_epi8(fits) != 0xffff)
                return false;
            chars += 8;
            length -= 8;
        } while (length >= 8);
    }
#endif
    for (; length; --length, ++chars) {
        if (*chars > 0xff)
            return false;
    }
    return true;
}

} // namespace js

#endif // vm_CharacterOperations_h
