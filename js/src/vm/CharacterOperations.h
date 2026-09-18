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
        while (length >= 4 * lanes) {
            const __m128i block0 = _mm_loadu_si128(
                reinterpret_cast<const __m128i*>(chars));
            const uint32_t mask0 = static_cast<uint32_t>(
                _mm_movemask_epi8(sizeof(CharT) == 1
                                  ? _mm_cmpeq_epi8(block0, needle)
                                  : _mm_cmpeq_epi16(block0, needle)));
            if (mask0)
                return chars + mozilla::CountTrailingZeroes32(mask0) / sizeof(CharT);

            const __m128i block1 = _mm_loadu_si128(
                reinterpret_cast<const __m128i*>(chars + lanes));
            const uint32_t mask1 = static_cast<uint32_t>(
                _mm_movemask_epi8(sizeof(CharT) == 1
                                  ? _mm_cmpeq_epi8(block1, needle)
                                  : _mm_cmpeq_epi16(block1, needle)));
            if (mask1)
                return chars + lanes + mozilla::CountTrailingZeroes32(mask1) / sizeof(CharT);

            const __m128i block2 = _mm_loadu_si128(
                reinterpret_cast<const __m128i*>(chars + 2 * lanes));
            const uint32_t mask2 = static_cast<uint32_t>(
                _mm_movemask_epi8(sizeof(CharT) == 1
                                  ? _mm_cmpeq_epi8(block2, needle)
                                  : _mm_cmpeq_epi16(block2, needle)));
            if (mask2)
                return chars + 2 * lanes + mozilla::CountTrailingZeroes32(mask2) / sizeof(CharT);

            const __m128i block3 = _mm_loadu_si128(
                reinterpret_cast<const __m128i*>(chars + 3 * lanes));
            const uint32_t mask3 = static_cast<uint32_t>(
                _mm_movemask_epi8(sizeof(CharT) == 1
                                  ? _mm_cmpeq_epi8(block3, needle)
                                  : _mm_cmpeq_epi16(block3, needle)));
            if (mask3)
                return chars + 3 * lanes + mozilla::CountTrailingZeroes32(mask3) / sizeof(CharT);

            chars += 4 * lanes;
            length -= 4 * lanes;
        }
        while (length >= lanes) {
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
        }
    }
#endif
    for (; length; --length, ++chars) {
        if (*chars == match)
            return chars;
    }
    return nullptr;
}

template <typename CharT>
inline const CharT*
FindCharacterReverse(const CharT* chars, size_t length, CharT match)
{
    static_assert(sizeof(CharT) == 1 || sizeof(CharT) == 2, "character width");
#ifdef JS_HAS_SSE2_CHARACTER_OPERATIONS
    const size_t lanes = 16 / sizeof(CharT);
    const CharT* end = chars + length;
    if (length >= lanes) {
        const __m128i needle = sizeof(CharT) == 1
                              ? _mm_set1_epi8(static_cast<char>(match))
                              : _mm_set1_epi16(static_cast<short>(match));
        while (length >= 4 * lanes) {
            end -= 4 * lanes;
            length -= 4 * lanes;

            const __m128i block3 = _mm_loadu_si128(
                reinterpret_cast<const __m128i*>(end + 3 * lanes));
            const uint32_t mask3 = static_cast<uint32_t>(
                _mm_movemask_epi8(sizeof(CharT) == 1
                                  ? _mm_cmpeq_epi8(block3, needle)
                                  : _mm_cmpeq_epi16(block3, needle)));
            if (mask3)
                return end + 3 * lanes +
                       (31 - mozilla::CountLeadingZeroes32(mask3)) / sizeof(CharT);

            const __m128i block2 = _mm_loadu_si128(
                reinterpret_cast<const __m128i*>(end + 2 * lanes));
            const uint32_t mask2 = static_cast<uint32_t>(
                _mm_movemask_epi8(sizeof(CharT) == 1
                                  ? _mm_cmpeq_epi8(block2, needle)
                                  : _mm_cmpeq_epi16(block2, needle)));
            if (mask2)
                return end + 2 * lanes +
                       (31 - mozilla::CountLeadingZeroes32(mask2)) / sizeof(CharT);

            const __m128i block1 = _mm_loadu_si128(
                reinterpret_cast<const __m128i*>(end + lanes));
            const uint32_t mask1 = static_cast<uint32_t>(
                _mm_movemask_epi8(sizeof(CharT) == 1
                                  ? _mm_cmpeq_epi8(block1, needle)
                                  : _mm_cmpeq_epi16(block1, needle)));
            if (mask1)
                return end + lanes +
                       (31 - mozilla::CountLeadingZeroes32(mask1)) / sizeof(CharT);

            const __m128i block0 = _mm_loadu_si128(
                reinterpret_cast<const __m128i*>(end));
            const uint32_t mask0 = static_cast<uint32_t>(
                _mm_movemask_epi8(sizeof(CharT) == 1
                                  ? _mm_cmpeq_epi8(block0, needle)
                                  : _mm_cmpeq_epi16(block0, needle)));
            if (mask0)
                return end + (31 - mozilla::CountLeadingZeroes32(mask0)) / sizeof(CharT);
        }
        while (length >= lanes) {
            end -= lanes;
            length -= lanes;
            const __m128i block = _mm_loadu_si128(reinterpret_cast<const __m128i*>(end));
            const __m128i equal = sizeof(CharT) == 1
                                 ? _mm_cmpeq_epi8(block, needle)
                                 : _mm_cmpeq_epi16(block, needle);
            const uint32_t mask = static_cast<uint32_t>(_mm_movemask_epi8(equal));
            if (mask)
                return end + (31 - mozilla::CountLeadingZeroes32(mask)) / sizeof(CharT);
        }
    }
#else
    const CharT* end = chars + length;
#endif

    while (length) {
        --end;
        --length;
        if (*end == match)
            return end;
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
        while (length >= 32) {
            const __m128i block0 = _mm_loadu_si128(
                reinterpret_cast<const __m128i*>(chars));
            if (_mm_movemask_epi8(_mm_cmpeq_epi16(
                    _mm_and_si128(block0, highBytes), zero)) != 0xffff)
                return false;

            const __m128i block1 = _mm_loadu_si128(
                reinterpret_cast<const __m128i*>(chars + 8));
            if (_mm_movemask_epi8(_mm_cmpeq_epi16(
                    _mm_and_si128(block1, highBytes), zero)) != 0xffff)
                return false;

            const __m128i block2 = _mm_loadu_si128(
                reinterpret_cast<const __m128i*>(chars + 16));
            if (_mm_movemask_epi8(_mm_cmpeq_epi16(
                    _mm_and_si128(block2, highBytes), zero)) != 0xffff)
                return false;

            const __m128i block3 = _mm_loadu_si128(
                reinterpret_cast<const __m128i*>(chars + 24));
            if (_mm_movemask_epi8(_mm_cmpeq_epi16(
                    _mm_and_si128(block3, highBytes), zero)) != 0xffff)
                return false;

            chars += 32;
            length -= 32;
        }
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
