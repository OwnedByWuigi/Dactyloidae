/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jscntxt.h"
#include "jscompartment.h"
#include "jsnum.h"
#include "jsstr.h"

#include "jsapi-tests/tests.h"

#include "vm/String-inl.h"

using mozilla::ArrayLength;

static const struct TestPair {
    uint32_t num;
    const char* expected;
} tests[] = {
    { 0, "0" },
    { 1, "1" },
    { 2, "2" },
    { 9, "9" },
    { 10, "10" },
    { 15, "15" },
    { 16, "16" },
    { 17, "17" },
    { 99, "99" },
    { 100, "100" },
    { 255, "255" },
    { 256, "256" },
    { 257, "257" },
    { 999, "999" },
    { 1000, "1000" },
    { 4095, "4095" },
    { 4096, "4096" },
    { 9999, "9999" },
    { 1073741823, "1073741823" },
    { 1073741824, "1073741824" },
    { 1073741825, "1073741825" },
    { 2147483647, "2147483647" },
    { 2147483648u, "2147483648" },
    { 2147483649u, "2147483649" },
    { 4294967294u, "4294967294" },
    { 4294967295u, "4294967295" },
};

BEGIN_TEST(testIndexToString)
{
    for (size_t i = 0, sz = ArrayLength(tests); i < sz; i++) {
        uint32_t u = tests[i].num;
        JSString* str = js::IndexToString(cx, u);
        CHECK(str);

        if (!js::StaticStrings::hasUint(u))
            CHECK(cx->compartment()->dtoaCache.lookup(10, u) == str);

        bool match = false;
        CHECK(JS_StringEqualsAscii(cx, str, tests[i].expected, &match));
        CHECK(match);
    }

    return true;
}
END_TEST(testIndexToString)

BEGIN_TEST(testDtoaCacheInterleaved)
{
    JS::RootedString first(cx, js::NumberToString<js::CanGC>(cx, 1234.5));
    CHECK(first);
    JS::RootedString second(cx, js::NumberToString<js::CanGC>(cx, 6789.5));
    CHECK(second);
    JS::RootedString third(cx, js::IndexToString(cx, 123456));
    CHECK(third);
    JS::RootedString fourth(cx, js::IndexToString(cx, 654321));
    CHECK(fourth);

    for (size_t i = 0; i < 10; i++) {
        CHECK(js::NumberToString<js::CanGC>(cx, 1234.5) == first);
        CHECK(js::NumberToString<js::CanGC>(cx, 6789.5) == second);
        CHECK(js::IndexToString(cx, 123456) == third);
        CHECK(js::IndexToString(cx, 654321) == fourth);
    }

    // Every raw string pointer must be invalidated, not just the latest one.
    JS_GC(cx);
    CHECK(!cx->compartment()->dtoaCache.lookup(10, 1234.5));
    CHECK(!cx->compartment()->dtoaCache.lookup(10, 6789.5));
    CHECK(!cx->compartment()->dtoaCache.lookup(10, 123456));
    CHECK(!cx->compartment()->dtoaCache.lookup(10, 654321));

    // The radix is part of the key. Signed zero can share its string.
    js::DtoaCache cache;
    cache.cache(10, 0.0, &first->asFlat());
    cache.cache(16, 0.0, &second->asFlat());
    CHECK(cache.lookup(10, -0.0) == first);
    CHECK(cache.lookup(16, -0.0) == second);
    CHECK(!cache.lookup(2, 0.0));
    cache.purge();
    CHECK(!cache.lookup(10, 0.0));
    CHECK(!cache.lookup(16, 0.0));

    for (size_t i = 0; i < 12; i++)
        cache.cache(10, double(i), &first->asFlat());
    for (size_t i = 0; i < 8; i++)
        CHECK(!cache.lookup(10, double(i)));
    for (size_t i = 8; i < 12; i++)
        CHECK(cache.lookup(10, double(i)) == first);

    // A failed string allocation must never turn into a cache hit.
    cache.cache(10, 12.0, nullptr);
    CHECK(!cache.lookup(10, 12.0));
    return true;
}
END_TEST(testDtoaCacheInterleaved)

BEGIN_TEST(testStringIsIndex)
{
    for (size_t i = 0, sz = ArrayLength(tests); i < sz; i++) {
        uint32_t u = tests[i].num;
        JSFlatString* str = js::IndexToString(cx, u);
        CHECK(str);

        uint32_t n;
        CHECK(str->isIndex(&n));
        CHECK(u == n);
    }

    return true;
}
END_TEST(testStringIsIndex)

BEGIN_TEST(testStringToPropertyName)
{
    uint32_t index;

    static const char16_t hiChars[] = { 'h', 'i' };
    JSFlatString* hiStr = NewString(cx, hiChars);
    CHECK(hiStr);
    CHECK(!hiStr->isIndex(&index));
    CHECK(hiStr->toPropertyName(cx) != nullptr);

    static const char16_t maxChars[] = { '4', '2', '9', '4', '9', '6', '7', '2', '9', '5' };
    JSFlatString* maxStr = NewString(cx, maxChars);
    CHECK(maxStr);
    CHECK(maxStr->isIndex(&index));
    CHECK(index == UINT32_MAX);

    static const char16_t maxPlusOneChars[] = { '4', '2', '9', '4', '9', '6', '7', '2', '9', '6' };
    JSFlatString* maxPlusOneStr = NewString(cx, maxPlusOneChars);
    CHECK(maxPlusOneStr);
    CHECK(!maxPlusOneStr->isIndex(&index));
    CHECK(maxPlusOneStr->toPropertyName(cx) != nullptr);

    return true;
}

template<size_t N> static JSFlatString*
NewString(JSContext* cx, const char16_t (&chars)[N])
{
    return js::NewStringCopyN<js::CanGC>(cx, chars, N);
}

END_TEST(testStringToPropertyName)
