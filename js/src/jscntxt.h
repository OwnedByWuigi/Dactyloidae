/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JS execution context. */

#ifndef jscntxt_h
#define jscntxt_h

#include "mozilla/MemoryReporting.h"

#include "js/CharacterEncoding.h"
#include "js/GCVector.h"
#include "js/Result.h"
#include "js/Utility.h"
#include "js/Vector.h"
#include "threading/ProtectedData.h"
#include "vm/MallocProvider.h"
#include "vm/Runtime.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4100) /* Silence unreferenced formal parameter warnings */
#endif

struct DtoaState;

namespace js {

namespace jit {
class JitContext;
class DebugModeOSRVolatileJitFrameIterator;
} // namespace jit

namespace gc {
class AutoSuppressNurseryCellAlloc;
}

typedef HashSet<Shape*> ShapeSet;

/* Detects cycles when traversing an object graph. */
class MOZ_RAII AutoCycleDetector
{
  public:
    using Set = HashSet<JSObject*, MovableCellHasher<JSObject*>, SystemAllocPolicy>;

    AutoCycleDetector(JSContext* cx, HandleObject objArg
                      MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : cx(cx), obj(cx, objArg), cyclic(true)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    ~AutoCycleDetector();

    bool init();

    bool foundCycle() { return cyclic; }

  private:
    Generation hashsetGenerationAtInit;
    JSContext* cx;
    RootedObject obj;
    Set::AddPtr hashsetAddPointer;
    bool cyclic;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

/* Updates references in the cycle detection set if the GC moves them. */
extern void
TraceCycleDetectionSet(JSTracer* trc, AutoCycleDetector::Set& set);

struct AutoResolving;

namespace frontend { class CompileError; }

/*
 * Execution Context Overview:
 *
 * Several different structures may be used to provide a context for operations
 * on the VM. Each context is thread local, but varies in what data it can
 * access and what other threads may be running.
 *
 * - ExclusiveContext is used by threads operating in one compartment/zone,
 * where other threads may operate in other compartments, but *not* the same
 * compartment or zone which the ExclusiveContext is in. A thread with an
 * ExclusiveContext may enter the atoms compartment and atomize strings, in
 * which case a lock is used.
 *
 * - JSContext is used only by the runtime's main thread. The context may
 * operate in any compartment or zone which is not used by an ExclusiveContext,
 * and will only run in parallel with threads using such contexts.
 *
 * A JSContext coerces to an ExclusiveContext.
 */

struct HelperThread;

class AutoLockScriptData;

void ReportOverRecursed(JSContext* cx, unsigned errorNumber);

/* Thread Local Storage slot for storing the context for a thread. */
extern MOZ_THREAD_LOCAL(JSContext*) TlsContext;

} /* namespace js */

namespace js {

enum class ContextKind
{
    Context_JS,
    Context_Exclusive
};

// ExclusiveContext provides a base for JSContext with context-kind tracking.
class ExclusiveContext : public ContextFriendFields,
                         public MallocProvider<JSContext>
{
  protected:
    JSRuntime* const runtime_;
    ContextKind contextKind_;
    JS::ContextOptions options_;

  public:
    ExclusiveContext(JSRuntime* rt, PerThreadData* pt, ContextKind kind,
                     const JS::ContextOptions& options);

    bool isJSContext() const {
        return contextKind_ == ContextKind::Context_JS;
    }

    JSContext* maybeJSContext() const {
        if (isJSContext())
            return (JSContext*) this;
        return nullptr;
    }

    JSContext* asJSContext() const {
        MOZ_ASSERT(isJSContext());
        return maybeJSContext();
    }

    bool shouldBeJSContext() const {
        MOZ_ASSERT(isJSContext());
        return isJSContext();
    }

    const JS::ContextOptions& options() const {
        return options_;
    }

    bool runtimeMatches(JSRuntime* rt) const {
        return runtime_ == rt;
    }

    PerThreadData* perThreadData;

  public:
    gc::ArenaLists* arenas_;

    // Error reporting
    static JS::Error reportedError;
    static JS::OOM reportedOOM;

    inline JS::Result<> boolToResult(bool ok);

    mozilla::GenericErrorResult<JS::OOM&> alreadyReportedOOM();
    mozilla::GenericErrorResult<JS::Error&> alreadyReportedError();

    // Forwarding methods for JSRuntime members
    JSAtomState& names() { return *runtime_->commonNames; }
    StaticStrings& staticStrings() { return *runtime_->staticStrings; }
    bool isPermanentAtomsInitialized() { return !!runtime_->permanentAtoms; }
    FrozenAtomSet& permanentAtoms() { return *runtime_->permanentAtoms; }
    WellKnownSymbols& wellKnownSymbols() { return *runtime_->wellKnownSymbols; }
    JS::BuildIdOp buildIdOp() { return runtime_->buildIdOp; }
    PropertyName* emptyString() { return runtime_->emptyString; }
    bool jitSupportsFloatingPoint() const { return runtime_->jitSupportsFloatingPoint; }
    bool jitSupportsSimd() const { return runtime_->jitSupportsSimd; }
    JSCompartment* atomsCompartment(AutoLockForExclusiveAccess& lock) { return runtime_->atomsCompartment(lock); }
};

} /* namespace js */

struct JSContext : public js::ExclusiveContext,
                   public JSRuntime
{
    explicit JSContext(JSRuntime* parentRuntime);
    ~JSContext();

    bool init(uint32_t maxBytes, uint32_t maxNurseryBytes);

    JSRuntime* runtime() { return this; }
    js::PerThreadData& mainThread() { return this->JSRuntime::mainThread; }

    static size_t offsetOfActivation() {
        return offsetof(JSContext, activation_);
    }
    static size_t offsetOfWasmActivation() {
        return offsetof(JSContext, wasmActivationStack_);
    }
    static size_t offsetOfProfilingActivation() {
        return offsetof(JSContext, profilingActivation_);
     }
    static size_t offsetOfCompartment() {
        return offsetof(JSContext, compartment_);
    }

    friend class JS::AutoSaveExceptionState;
    friend void js::ReportOverRecursed(JSContext*, unsigned errorNumber);

  private:
    /* Exception state -- the exception member is a GC root by definition. */
    bool                throwing;            /* is there a pending exception? */
    JS::PersistentRooted<JS::Value> unwrappedException_; /* most-recently-thrown exception */
    JS::PersistentRooted<js::SavedFrame*> unwrappedExceptionStack_; /* stack when the exception was thrown */

    // True if the exception currently being thrown is by result of
    // ReportOverRecursed. See Debugger::slowPathOnExceptionUnwind.
    bool                overRecursed_;

    // True if propagating a forced return from an interrupt handler during
    // debug mode.
    bool                propagatingForcedReturn_;

    // A stack of live iterators that need to be updated in case of debug mode
    // OSR.
    js::jit::DebugModeOSRVolatileJitFrameIterator* liveVolatileJitFrameIterators_;

  public:
    js::ContextCaches caches;

    int32_t             reportGranularity;  /* see vm/Probes.h */

    js::AutoResolving*  resolvingList;

    /* True if generating an error, to prevent runaway recursion. */
    bool                generatingError;

    /* State for object and array toSource conversion. */
    js::AutoCycleDetector::Set cycleDetectorSet;

    /* Client opaque pointer. */
    void* data;

    void resetJitStackLimit();

  public:

    /*
     * Return:
     * - The newest scripted frame's version, if there is such a frame.
     * - The version from the compartment.
     * - The default version.
     *
     * Note: if this ever shows up in a profile, just add caching!
     */
    JSVersion findVersion() const;

    void initJitStackLimit();

    JS::ContextOptions& options() {
        return options_;
    }

    js::LifoAlloc& tempLifoAlloc() { return runtime()->tempLifoAlloc; }

    unsigned            outstandingRequests;/* number of JS_BeginRequest calls
                                               without the corresponding
                                               JS_EndRequest. */

    bool jitIsBroken;

    void updateJITEnabled();

    /*
     * Youngest frame of a saved stack that will be picked up as an async stack
     * by any new Activation, and is nullptr when no async stack should be used.
     *
     * The JS::AutoSetAsyncStackForNewCalls class can be used to set this.
     *
     * New activations will reset this to nullptr on construction after getting
     * the current value, and will restore the previous value on destruction.
     */
    JS::PersistentRooted<js::SavedFrame*> asyncStackForNewActivations;

    /*
     * Value of asyncCause to be attached to asyncStackForNewActivations.
     */
    const char* asyncCauseForNewActivations;

    /*
     * True if the async call was explicitly requested, e.g. via
     * callFunctionWithAsyncStack.
     */
    bool asyncCallIsExplicit;

    /* Whether this context has JS frames on the stack. */
    bool currentlyRunning() const;

    bool currentlyRunningInInterpreter() const {
        return activation()->isInterpreter();
    }
    bool currentlyRunningInJit() const {
        return activation()->isJit();
    }
    js::InterpreterFrame* interpreterFrame() const {
        return activation()->asInterpreter()->current();
    }
    js::InterpreterRegs& interpreterRegs() const {
        return activation()->asInterpreter()->regs();
    }

    /*
     * Get the topmost script and optional pc on the stack. By default, this
     * function only returns a JSScript in the current compartment, returning
     * nullptr if the current script is in a different compartment. This
     * behavior can be overridden by passing ALLOW_CROSS_COMPARTMENT.
     */
    enum MaybeAllowCrossCompartment {
        DONT_ALLOW_CROSS_COMPARTMENT = false,
        ALLOW_CROSS_COMPARTMENT = true
    };
    inline JSScript* currentScript(jsbytecode** pc = nullptr,
                                   MaybeAllowCrossCompartment = DONT_ALLOW_CROSS_COMPARTMENT) const;

    // The generational GC nursery may only be used on the main thread.
    inline js::Nursery& nursery() {
        return gc.nursery;
    }

    void minorGC(JS::gcreason::Reason reason) {
        gc.minorGC(reason);
    }

  public:
    bool isExceptionPending() const {
        return throwing;
    }

    MOZ_MUST_USE
    bool getPendingException(JS::MutableHandleValue rval);
    
    js::SavedFrame* getPendingExceptionStack();

    bool isThrowingOutOfMemory();
    bool isThrowingDebuggeeWouldRun();
    bool isClosingGenerator();

    void setPendingException(JS::HandleValue v, js::HandleSavedFrame stack);
    void setPendingExceptionAndCaptureStack(JS::HandleValue v);

    void clearPendingException() {
        throwing = false;
        overRecursed_ = false;
        unwrappedException_.setUndefined();
        unwrappedExceptionStack_ = nullptr;
    }

    bool isThrowingOverRecursed() const { return throwing && overRecursed_; }
    bool isPropagatingForcedReturn() const { return propagatingForcedReturn_; }
    void setPropagatingForcedReturn() { propagatingForcedReturn_ = true; }
    void clearPropagatingForcedReturn() { propagatingForcedReturn_ = false; }

    /*
     * See JS_SetTrustedPrincipals in jsapi.h.
     * Note: !cx->compartment is treated as trusted.
     */
    inline bool runningWithTrustedPrincipals() const;

    JS_FRIEND_API(size_t) sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

    void mark(JSTracer* trc);

  private:
    /*
     * The allocation code calls the function to indicate either OOM failure
     * when p is null or that a memory pressure counter has reached some
     * threshold when p is not null. The function takes the pointer and not
     * a boolean flag to minimize the amount of code in its inlined callers.
     */
    JS_FRIEND_API(void) checkMallocGCPressure(void* p);
}; /* struct JSContext */

namespace js {

inline JS::Result<>
ExclusiveContext::boolToResult(bool ok)
{
    if (MOZ_LIKELY(ok)) {
        MOZ_ASSERT_IF(isJSContext(), !asJSContext()->isExceptionPending());
        MOZ_ASSERT_IF(isJSContext(), !asJSContext()->isPropagatingForcedReturn());
        return JS::Ok();
    }
    return JS::Result<>(reportedError);
}

struct MOZ_RAII AutoResolving {
  public:
    enum Kind {
        LOOKUP,
        WATCH
    };

    AutoResolving(JSContext* cx, HandleObject obj, HandleId id, Kind kind = LOOKUP
                  MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : context(cx), object(obj), id(id), kind(kind), link(cx->resolvingList)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        MOZ_ASSERT(obj);
        cx->resolvingList = this;
    }

    ~AutoResolving() {
        MOZ_ASSERT(context->resolvingList == this);
        context->resolvingList = link;
    }

    bool alreadyStarted() const {
        return link && alreadyStartedSlow();
    }

  private:
    bool alreadyStartedSlow() const;

    JSContext*          const context;
    HandleObject        object;
    HandleId            id;
    Kind                const kind;
    AutoResolving*      const link;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

/*
 * Create and destroy functions for JSContext, which is manually allocated
 * and exclusively owned.
 */
extern JSContext*
NewContext(uint32_t maxBytes, uint32_t maxNurseryBytes, JSRuntime* parentRuntime);

extern void
DestroyContext(JSContext* cx);

enum ErrorArgumentsType {
    ArgumentsAreUnicode,
    ArgumentsAreASCII,
    ArgumentsAreLatin1,
    ArgumentsAreUTF8
};

/*
 * Loads and returns a self-hosted function by name. For performance, define
 * the property name in vm/CommonPropertyNames.h.
 *
 * Defined in SelfHosting.cpp.
 */
JSFunction*
SelfHostedFunction(JSContext* cx, HandlePropertyName propName);

#ifdef va_start
extern bool
ReportErrorVA(JSContext* cx, unsigned flags, const char* format,
              ErrorArgumentsType argumentsType, va_list ap);

extern bool
ReportErrorNumberVA(JSContext* cx, unsigned flags, JSErrorCallback callback,
                    void* userRef, const unsigned errorNumber,
                    ErrorArgumentsType argumentsType, va_list ap);

extern bool
ReportErrorNumberUCArray(JSContext* cx, unsigned flags, JSErrorCallback callback,
                         void* userRef, const unsigned errorNumber,
                         const char16_t** args);
#endif

extern bool
ExpandErrorArgumentsVA(ExclusiveContext* cx, JSErrorCallback callback,
                       void* userRef, const unsigned errorNumber,
                       const char16_t** messageArgs,
                       ErrorArgumentsType argumentsType,
                       JSErrorReport* reportp, va_list ap);

extern bool
ExpandErrorArgumentsVA(ExclusiveContext* cx, JSErrorCallback callback,
                       void* userRef, const unsigned errorNumber,
                       const char16_t** messageArgs,
                       ErrorArgumentsType argumentsType,
                       JSErrorNotes::Note* notep, va_list ap);

/* |callee| requires a usage string provided by JS_DefineFunctionsWithHelp. */
extern void
ReportUsageErrorASCII(JSContext* cx, HandleObject callee, const char* msg);

/*
 * Prints a full report and returns true if the given report is non-nullptr
 * and the report doesn't have the JSREPORT_WARNING flag set or reportWarnings
 * is true.
 * Returns false otherwise.
 */
extern bool
PrintError(JSContext* cx, FILE* file, JS::ConstUTF8CharsZ toStringResult,
           JSErrorReport* report, bool reportWarnings);

/*
 * Send a JSErrorReport to the warningReporter callback.
 */
void
CallWarningReporter(JSContext* cx, JSErrorReport* report);

extern bool
ReportIsNotDefined(JSContext* cx, HandlePropertyName name);

extern bool
ReportIsNotDefined(JSContext* cx, HandleId id);

/*
 * Report an attempt to access the property of a null or undefined value (v).
 */
extern bool
ReportIsNullOrUndefined(JSContext* cx, int spindex, HandleValue v, HandleString fallback);

extern void
ReportMissingArg(JSContext* cx, js::HandleValue v, unsigned arg);

/*
 * Report error using js_DecompileValueGenerator(cx, spindex, v, fallback) as
 * the first argument for the error message. If the error message has less
 * then 3 arguments, use null for arg1 or arg2.
 */
extern bool
ReportValueErrorFlags(JSContext* cx, unsigned flags, const unsigned errorNumber,
                      int spindex, HandleValue v, HandleString fallback,
                      const char* arg1, const char* arg2);

#define ReportValueError(cx,errorNumber,spindex,v,fallback)                   \
    ((void)ReportValueErrorFlags(cx, JSREPORT_ERROR, errorNumber,             \
                                    spindex, v, fallback, nullptr, nullptr))

#define ReportValueError2(cx,errorNumber,spindex,v,fallback,arg1)             \
    ((void)ReportValueErrorFlags(cx, JSREPORT_ERROR, errorNumber,             \
                                    spindex, v, fallback, arg1, nullptr))

#define ReportValueError3(cx,errorNumber,spindex,v,fallback,arg1,arg2)        \
    ((void)ReportValueErrorFlags(cx, JSREPORT_ERROR, errorNumber,             \
                                    spindex, v, fallback, arg1, arg2))

JSObject*
CreateErrorNotesArray(JSContext* cx, JSErrorReport* report);

} /* namespace js */

extern const JSErrorFormatString js_ErrorFormatString[JSErr_Limit];

namespace js {

MOZ_ALWAYS_INLINE bool
CheckForInterrupt(JSContext* cx)
{
    MOZ_ASSERT(!cx->isExceptionPending());
    // Add an inline fast-path since we have to check for interrupts in some hot
    // C++ loops of library builtins.
    JSRuntime* rt = cx->runtime();
    if (MOZ_UNLIKELY(rt->hasPendingInterrupt()))
        return rt->handleInterrupt(cx);
    return true;
}

/************************************************************************/

/* AutoArrayRooter roots an external array of Values. */
class MOZ_RAII AutoArrayRooter : private JS::AutoGCRooter
{
  public:
    AutoArrayRooter(JSContext* cx, size_t len, Value* vec
                    MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : JS::AutoGCRooter(cx, len), array(vec)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        MOZ_ASSERT(tag_ >= 0);
    }

    void changeLength(size_t newLength) {
        tag_ = ptrdiff_t(newLength);
        MOZ_ASSERT(tag_ >= 0);
    }

    void changeArray(Value* newArray, size_t newLength) {
        changeLength(newLength);
        array = newArray;
    }

    Value* start() {
        return array;
    }

    size_t length() {
        MOZ_ASSERT(tag_ >= 0);
        return size_t(tag_);
    }

    MutableHandleValue handleAt(size_t i) {
        MOZ_ASSERT(i < size_t(tag_));
        return MutableHandleValue::fromMarkedLocation(&array[i]);
    }
    HandleValue handleAt(size_t i) const {
        MOZ_ASSERT(i < size_t(tag_));
        return HandleValue::fromMarkedLocation(&array[i]);
    }
    MutableHandleValue operator[](size_t i) {
        MOZ_ASSERT(i < size_t(tag_));
        return MutableHandleValue::fromMarkedLocation(&array[i]);
    }
    HandleValue operator[](size_t i) const {
        MOZ_ASSERT(i < size_t(tag_));
        return HandleValue::fromMarkedLocation(&array[i]);
    }

    friend void JS::AutoGCRooter::trace(JSTracer* trc);

  private:
    Value* array;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class AutoAssertNoException
{
#ifdef DEBUG
    JSContext* cx;
    bool hadException;
#endif

  public:
    explicit AutoAssertNoException(JSContext* cx)
#ifdef DEBUG
      : cx(cx),
        hadException(cx->isExceptionPending())
#endif
    {
    }

    ~AutoAssertNoException()
    {
        MOZ_ASSERT_IF(!hadException, !cx->isExceptionPending());
    }
};

/* Exposed intrinsics for the JITs. */
bool intrinsic_IsSuspendedStarGenerator(JSContext* cx, unsigned argc, Value* vp);

class MOZ_RAII AutoLockForExclusiveAccess
{
    JSRuntime* runtime;

    void init(JSRuntime* rt) {
        runtime = rt;
        if (runtime->numExclusiveThreads) {
            runtime->exclusiveAccessLock.lock();
        } else {
            MOZ_ASSERT(!runtime->mainThreadHasExclusiveAccess);
#ifdef DEBUG
            runtime->mainThreadHasExclusiveAccess = true;
#endif
        }
    }

  public:
    explicit AutoLockForExclusiveAccess(ExclusiveContext* cx MOZ_GUARD_OBJECT_NOTIFIER_PARAM) {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        init(cx->runtime_);
    }
    explicit AutoLockForExclusiveAccess(JSRuntime* rt MOZ_GUARD_OBJECT_NOTIFIER_PARAM) {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        init(rt);
    }
    explicit AutoLockForExclusiveAccess(JSContext* cx MOZ_GUARD_OBJECT_NOTIFIER_PARAM) {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        init(cx->runtime());
    }
    ~AutoLockForExclusiveAccess() {
        if (runtime->numExclusiveThreads) {
            runtime->exclusiveAccessLock.unlock();
        } else {
            MOZ_ASSERT(runtime->mainThreadHasExclusiveAccess);
#ifdef DEBUG
            runtime->mainThreadHasExclusiveAccess = false;
#endif
        }
    }

    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class MOZ_RAII AutoLockScriptData
{
    JSRuntime* runtime;

  public:
    explicit AutoLockScriptData(JSRuntime* rt MOZ_GUARD_OBJECT_NOTIFIER_PARAM) {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        runtime = rt;
        if (runtime->hasHelperThreadZones()) {
            runtime->scriptDataLock.lock();
        } else {
            MOZ_ASSERT(!runtime->activeThreadHasScriptDataAccess);
#ifdef DEBUG
            runtime->activeThreadHasScriptDataAccess = true;
#endif
        }
    }
    ~AutoLockScriptData() {
        if (runtime->hasHelperThreadZones()) {
            runtime->scriptDataLock.unlock();
        } else {
            MOZ_ASSERT(runtime->activeThreadHasScriptDataAccess);
#ifdef DEBUG
            runtime->activeThreadHasScriptDataAccess = false;
#endif
        }
    }

    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class MOZ_RAII AutoKeepAtoms
{
    JSContext* cx;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

extern JS::TwoByteCharsZ
LossyUTF8CharsToNewTwoByteCharsZ(ExclusiveContext* cx, const JS::ConstUTF8CharsZ& utf8, size_t* outlen);

extern JS::Latin1CharsZ
LossyUTF8CharsToNewLatin1CharsZ(ExclusiveContext* cx, const JS::UTF8Chars utf8, size_t* outlen);

} /* namespace js */

#ifdef _MSC_VER
#pragma warning(pop)
#endif

inline JSContext*
JSRuntime::unsafeContextFromAnyThread()
{
    return static_cast<JSContext*>(this);
}

inline JSContext*
JSRuntime::contextFromMainThread()
{
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(this));
    return unsafeContextFromAnyThread();
}

#endif /* jscntxt_h */
