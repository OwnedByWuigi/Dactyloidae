/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_SweepingAPI_h
#define js_SweepingAPI_h

#include "js/HeapAPI.h"
#include "mozilla/LinkedList.h"

namespace JS {
template <typename T> class WeakCache;

namespace detail {
class WeakCacheBase : public mozilla::LinkedListElement<WeakCacheBase>
{
  public:
    WeakCacheBase() = default;
    WeakCacheBase(WeakCacheBase&& other)
      : mozilla::LinkedListElement<WeakCacheBase>(mozilla::Move(other)) {}
    virtual ~WeakCacheBase() = default;
    virtual size_t sweep() = 0;
    // Generic sweep policies run atomically; specialized caches may opt in.
    virtual bool needsSweep() const { return true; }
    virtual bool setNeedsIncrementalBarrier(bool) { return false; }
    virtual bool needsIncrementalBarrier() const { return false; }
};
}

namespace shadow {
JS_PUBLIC_API(void)
RegisterWeakCache(JS::Zone* zone, JS::WeakCache<void*>* cachep);
JS_PUBLIC_API(void)
RegisterWeakCache(JS::Zone* zone, JS::detail::WeakCacheBase* cachep);
} // namespace shadow

// A WeakCache stores the given Sweepable container and links itself into a
// list of such caches that are swept during each GC.
template <typename T>
class WeakCache : public js::MutableWrappedPtrOperations<T, WeakCache<T>>,
                  public detail::WeakCacheBase
{
    friend class mozilla::LinkedListElement<WeakCache<T>>;
    friend class mozilla::LinkedList<WeakCache<T>>;

    WeakCache() = delete;
    WeakCache(const WeakCache&) = delete;

    using SweepFn = void (*)(T*);
    SweepFn sweeper;
    T cache;

  public:
    using Type = T;

    template <typename U>
    WeakCache(Zone* zone, U&& initial)
      : cache(mozilla::Forward<U>(initial))
    {
        sweeper = GCPolicy<T>::sweep;
        shadow::RegisterWeakCache(zone, static_cast<detail::WeakCacheBase*>(this));
    }
    WeakCache(WeakCache&& other)
      : detail::WeakCacheBase(mozilla::Move(other)),
        sweeper(other.sweeper),
        cache(mozilla::Move(other.cache))
    {
    }

    const T& get() const { return cache; }
    T& get() { return cache; }

    size_t sweep() override { sweeper(&cache); return 1; }
};

} // namespace JS

#endif // js_SweepingAPI_h
