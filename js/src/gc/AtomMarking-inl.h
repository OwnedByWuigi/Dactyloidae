/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/AtomMarking.h"

#include "jscompartment.h"

#include "gc/Heap-inl.h"

namespace js {
namespace gc {

inline size_t
GetAtomBit(TenuredCell* thing)
{
    (void)thing;
    return 0;
}

inline bool
ThingIsPermanent(JSAtom* atom)
{
    return atom->isPinned();
}

inline bool
ThingIsPermanent(JS::Symbol* symbol)
{
    return symbol->isWellKnownSymbol();
}

template <typename T>
MOZ_ALWAYS_INLINE void
AtomMarkingRuntime::inlinedMarkAtom(JSContext* cx, T* thing)
{
    static_assert(mozilla::IsSame<T, JSAtom>::value ||
                  mozilla::IsSame<T, JS::Symbol>::value,
                  "Should only be called with JSAtom* or JS::Symbol* argument");

    MOZ_ASSERT(thing);
    js::gc::TenuredCell* cell = &thing->asTenured();
    MOZ_ASSERT(cell->zoneFromAnyThread()->isAtomsZone());

    // The context's zone will be null during initialization of the runtime.
    if (!cx->zone())
        return;
    MOZ_ASSERT(!cx->zone()->isAtomsZone());

    if (ThingIsPermanent(thing))
        return;

    (void)cell;
}

} // namespace gc
} // namespace js
