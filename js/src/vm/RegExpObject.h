/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_RegExpObject_h
#define vm_RegExpObject_h

#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"

#include "jscntxt.h"

#include "builtin/SelfHostingDefines.h"
#include "gc/Marking.h"
#include "gc/Zone.h"
#include "proxy/Proxy.h"
#include "vm/ArrayObject.h"
#include "vm/Shape.h"
#include "vm/RegExpShared.h"
#include "irregexp/InfallibleVector.h"

/*
 * JavaScript Regular Expressions
 *
 * There are several engine concepts associated with a single logical regexp:
 *
 *   RegExpObject - The JS-visible object whose .[[Class]] equals "RegExp"
 *
 *   RegExpShared - The compiled representation of the regexp.
 *
 *   RegExpCompartment - Owns all RegExpShared instances in a compartment.
 *
 * To save memory, a RegExpShared is not created for a RegExpObject until it is
 * needed for execution. When a RegExpShared needs to be created, it is looked
 * up in a per-compartment table to allow reuse between objects. Lastly, on GC,
 * every RegExpShared that is not in active use is discarded.
 */
namespace js {

struct MatchPair;
class MatchPairs;
class RegExpShared;
class RegExpStatics;

using RootedRegExpShared = JS::Rooted<RegExpShared*>;
using HandleRegExpShared = JS::Handle<RegExpShared*>;
using MutableHandleRegExpShared = JS::MutableHandle<RegExpShared*>;

namespace frontend { class TokenStream; }

extern RegExpObject*
RegExpAlloc(ExclusiveContext* cx, HandleObject proto = nullptr);

// |regexp| is under-typed because this function's used in the JIT.
extern JSObject*
CloneRegExpObject(JSContext* cx, JSObject* regexp);

/*
 * A RegExpShared is the compiled representation of a regexp. A RegExpShared is
 * potentially pointed to by multiple RegExpObjects. Additionally, C++ code may
 * have pointers to RegExpShareds on the stack. The RegExpShareds are kept in a
 * table so that they can be reused when compiling the same regex string.
 *
 * During a GC, RegExpShared instances are marked and swept like GC things.
 * Usually, RegExpObjects clear their pointers to their RegExpShareds rather
 * than explicitly tracing them, so that the RegExpShared and any jitcode can
 * be reclaimed quicker. However, the RegExpShareds are traced through by
 * objects when we are preserving jitcode in their zone, to avoid the same
 * recompilation inefficiencies as normal Ion and baseline compilation.
 */
class RegExpObject : public NativeObject
{
    static const unsigned LAST_INDEX_SLOT          = 0;
    static const unsigned SOURCE_SLOT              = 1;
    static const unsigned FLAGS_SLOT               = 2;

    static_assert(RegExpObject::FLAGS_SLOT == REGEXP_FLAGS_SLOT,
                  "FLAGS_SLOT values should be in sync with self-hosted JS");

  public:
    static const unsigned RESERVED_SLOTS = 3;
    static const unsigned PRIVATE_SLOT = 3;

    static const Class class_;
    static const Class protoClass_;

    // The maximum number of pairs a MatchResult can have, without having to
    // allocate a bigger MatchResult.
    static const size_t MaxPairCount = 14;

    static RegExpObject*
    create(ExclusiveContext* cx, const char16_t* chars, size_t length, RegExpFlag flags,
           frontend::TokenStream* ts, LifoAlloc& alloc);

    static RegExpObject*
    create(ExclusiveContext* cx, HandleAtom atom, RegExpFlag flags,
           frontend::TokenStream* ts, LifoAlloc& alloc);

    /*
     * Compute the initial shape to associate with fresh RegExp objects,
     * encoding their initial properties. Return the shape after
     * changing |obj|'s last property to it.
     */
    static Shape*
    assignInitialShape(ExclusiveContext* cx, Handle<RegExpObject*> obj);

    /* Accessors. */

    static unsigned lastIndexSlot() { return LAST_INDEX_SLOT; }

    static bool isInitialShape(RegExpObject* rx) {
        Shape* shape = rx->lastProperty();
        if (!shape->hasSlot())
            return false;
        if (shape->maybeSlot() != LAST_INDEX_SLOT)
            return false;
        return true;
    }

    const Value& getLastIndex() const { return getSlot(LAST_INDEX_SLOT); }

    void setLastIndex(double d) {
        setSlot(LAST_INDEX_SLOT, NumberValue(d));
    }

    void zeroLastIndex(ExclusiveContext* cx) {
        MOZ_ASSERT(lookupPure(cx->names().lastIndex)->writable(),
                   "can't infallibly zero a non-writable lastIndex on a "
                   "RegExp that's been exposed to script");
        setSlot(LAST_INDEX_SLOT, Int32Value(0));
    }

    JSFlatString* toString(JSContext* cx) const;

    JSAtom* getSource() const { return &getSlot(SOURCE_SLOT).toString()->asAtom(); }

    void setSource(JSAtom* source) {
        setSlot(SOURCE_SLOT, StringValue(source));
    }

    /* Flags. */

    static unsigned flagsSlot() { return FLAGS_SLOT; }

    RegExpFlag getFlags() const {
        return RegExpFlag(getFixedSlot(FLAGS_SLOT).toInt32());
    }
    void setFlags(RegExpFlag flags) {
        setSlot(FLAGS_SLOT, Int32Value(flags));
    }

    bool hasIndices() const { return getFlags() & HasIndicesFlag; }
    bool global() const     { return getFlags() & GlobalFlag; }
    bool ignoreCase() const { return getFlags() & IgnoreCaseFlag; }
    bool multiline() const  { return getFlags() & MultilineFlag; }
    bool dotAll() const     { return getFlags() & DotAllFlag; }
    bool unicode() const    { return getFlags() & UnicodeFlag; }
    bool sticky() const     { return getFlags() & StickyFlag; }

    static bool isOriginalFlagGetter(JSNative native, RegExpFlag* mask);

    static MOZ_MUST_USE bool getShared(JSContext* cx, Handle<RegExpObject*> regexp,
                                       MutableHandleRegExpShared shared);

    bool hasShared() {
        return !!sharedRef();
    }

    void setShared(RegExpShared& shared) {
        MOZ_ASSERT(!hasShared());
        sharedRef().init(&shared);
    }

    static void trace(JSTracer* trc, JSObject* obj);
    void trace(JSTracer* trc);

    void initIgnoringLastIndex(HandleAtom source, RegExpFlag flags);

    // NOTE: This method is *only* safe to call on RegExps that haven't been
    //       exposed to script, because it requires that the "lastIndex"
    //       property be writable.
    void initAndZeroLastIndex(HandleAtom source, RegExpFlag flags, ExclusiveContext* cx);

#ifdef DEBUG
    static MOZ_MUST_USE bool dumpBytecode(JSContext* cx, Handle<RegExpObject*> regexp,
                                          bool match_only, HandleLinearString input);
#endif

  private:
    /*
     * Precondition: the syntax for |source| has already been validated.
     * Side effect: sets the private field.
     */
    static MOZ_MUST_USE bool createShared(JSContext* cx, Handle<RegExpObject*> regexp,
                                          MutableHandleRegExpShared shared);

    PreBarriered<RegExpShared*>& sharedRef() {
        auto& ref = NativeObject::privateRef(PRIVATE_SLOT);
        return reinterpret_cast<PreBarriered<RegExpShared*>&>(ref);
    }

    /* Call setShared in preference to setPrivate. */
    void setPrivate(void* priv) = delete;
};

/*
 * Parse regexp flags. Report an error and return false if an invalid
 * sequence of flags is encountered (repeat/invalid flag).
 *
 * N.B. flagStr must be rooted.
 */
bool
ParseRegExpFlags(JSContext* cx, JSString* flagStr, RegExpFlag* flagsOut);

/* Assuming GetBuiltinClass(obj) is ESClass::RegExp, return a RegExpShared for obj. */
inline bool
RegExpToShared(JSContext* cx, HandleObject obj, MutableHandleRegExpShared shared)
{
    if (obj->is<RegExpObject>())
        return RegExpObject::getShared(cx, obj.as<RegExpObject>(), shared);

    return Proxy::regexp_toShared(cx, obj, shared);
}

template<XDRMode mode>
bool
XDRScriptRegExpObject(XDRState<mode>* xdr, MutableHandle<RegExpObject*> objp);

extern JSObject*
CloneScriptRegExpObject(JSContext* cx, RegExpObject& re);

/* Escape all slashes and newlines in the given string. */
extern JSAtom*
EscapeRegExpPattern(JSContext* cx, HandleAtom src);

template <typename CharT>
extern bool
HasRegExpMetaChars(const CharT* chars, size_t length);

extern bool
StringHasRegExpMetaChars(JSLinearString* str);

} /* namespace js */

#endif /* vm_RegExpObject_h */
