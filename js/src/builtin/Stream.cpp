/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/Stream.h"

#include "js/CompilationAndEvaluation.h"
#include "js/Stream.h"

#include "jscntxt.h"

#include "gc/Heap.h"
#include "vm/SelfHosting.h"

#include "jsobjinlines.h"

#include "vm/NativeObject-inl.h"

#include "zlib.h"

#include <string.h>

using namespace js;

enum StreamSlots {
    StreamSlot_Controller,
    StreamSlot_Reader,
    StreamSlot_State,
    StreamSlot_StoredError,
    StreamSlotCount
};

enum ReaderSlots {
    ReaderSlot_Stream,
    ReaderSlot_Requests,
    ReaderSlot_ClosedPromise,
    ReaderSlotCount,
};

enum ReaderType {
    ReaderType_Default,
    ReaderType_BYOB
};

// ReadableStreamDefaultController and ReadableByteStreamController are both
// queue containers and must have these slots at identical offsets.
enum QueueContainerSlots {
    QueueContainerSlot_Queue,
    QueueContainerSlot_TotalSize,
    QueueContainerSlotCount
};

// These slots are identical between the two types of ReadableStream
// controllers.
enum ControllerSlots {
    ControllerSlot_Stream = QueueContainerSlotCount,
    ControllerSlot_UnderlyingSource,
    ControllerSlot_StrategyHWM,
    ControllerSlot_Flags,
    ControllerSlotCount
};

enum DefaultControllerSlots {
    DefaultControllerSlot_StrategySize = ControllerSlotCount,
    DefaultControllerSlotCount
};

enum ByteControllerSlots {
    ByteControllerSlot_BYOBRequest = ControllerSlotCount,
    ByteControllerSlot_PendingPullIntos,
    ByteControllerSlot_AutoAllocateSize,
    ByteControllerSlotCount
};

enum ControllerFlags {
    ControllerFlag_Started        = 1 << 0,
    ControllerFlag_Pulling        = 1 << 1,
    ControllerFlag_PullAgain      = 1 << 2,
    ControllerFlag_CloseRequested = 1 << 3,
    ControllerFlag_TeeBranch      = 1 << 4,
    ControllerFlag_TeeBranch1     = 1 << 5,
    ControllerFlag_TeeBranch2     = 1 << 6,
    ControllerFlag_ExternalSource = 1 << 7,
    ControllerFlag_SourceLocked   = 1 << 8,
};

// Offset at which embedding flags are stored.
constexpr uint8_t ControllerEmbeddingFlagsOffset = 24;

enum BYOBRequestSlots {
    BYOBRequestSlot_Controller,
    BYOBRequestSlot_View,
    BYOBRequestSlotCount
};

template<class T>
MOZ_ALWAYS_INLINE bool
Is(const HandleValue v)
{
    return v.isObject() && v.toObject().is<T>();
}

template<class T>
MOZ_ALWAYS_INLINE bool
IsMaybeWrapped(const HandleValue v)
{
    if (!v.isObject())
        return false;

    JSObject* obj = &v.toObject();
    if (obj->is<T>())
        return true;

    JSObject* unwrapped = CheckedUnwrap(obj);
    return unwrapped && unwrapped->is<T>();
}

template<class T>
MOZ_ALWAYS_INLINE T*
UnwrapInternalSlotObject(JSObject* obj)
{
    JSObject* unwrapped = UncheckedUnwrap(obj);
    MOZ_ASSERT(unwrapped);
    MOZ_ASSERT(unwrapped->is<T>());
    return &unwrapped->as<T>();
}

template<class T>
MOZ_ALWAYS_INLINE T*
UnwrapMaybeWrappedArg(const HandleValue v)
{
    if (!v.isObject())
        return nullptr;

    JSObject* obj = &v.toObject();
    if (obj->is<T>())
        return &obj->as<T>();

    JSObject* unwrapped = CheckedUnwrap(obj);
    if (!unwrapped || !unwrapped->is<T>())
        return nullptr;

    return &unwrapped->as<T>();
}

static bool
IsReadableStreamController(const JSObject* controller)
{
    return controller->is<ReadableStreamDefaultController>() ||
           controller->is<ReadableByteStreamController>();
}

[[nodiscard]] static NativeObject*
UnwrapMaybeWrappedReadableStreamController(const HandleValue v)
{
    if (!v.isObject())
        return nullptr;

    JSObject* obj = &v.toObject();
    if (IsReadableStreamController(obj))
        return &obj->as<NativeObject>();

    JSObject* unwrapped = CheckedUnwrap(obj);
    if (!unwrapped || !IsReadableStreamController(unwrapped))
        return nullptr;

    return &unwrapped->as<NativeObject>();
}

static inline uint32_t
ControllerFlags(const NativeObject* controller)
{
    MOZ_ASSERT(IsReadableStreamController(controller));
    return controller->getFixedSlot(ControllerSlot_Flags).toInt32();
}

static inline void
AddControllerFlags(NativeObject* controller, uint32_t flags)
{
    MOZ_ASSERT(IsReadableStreamController(controller));
    controller->setFixedSlot(ControllerSlot_Flags,
                             Int32Value(ControllerFlags(controller) | flags));
}

static inline void
RemoveControllerFlags(NativeObject* controller, uint32_t flags)
{
    MOZ_ASSERT(IsReadableStreamController(controller));
    controller->setFixedSlot(ControllerSlot_Flags,
                             Int32Value(ControllerFlags(controller) & ~flags));
}

static inline uint32_t
StreamState(const ReadableStream* stream)
{
    return stream->getFixedSlot(StreamSlot_State).toInt32();
}

static inline void
SetStreamState(ReadableStream* stream, uint32_t state)
{
    MOZ_ASSERT_IF(stream->disturbed(), state & ReadableStream::Disturbed);
    MOZ_ASSERT_IF(stream->closed() || stream->errored(), !(state & ReadableStream::Readable));
    stream->setFixedSlot(StreamSlot_State, Int32Value(state));
}

bool
ReadableStream::readable() const
{
    return StreamState(this) & Readable;
}

bool
ReadableStream::closed() const
{
    return StreamState(this) & Closed;
}

bool
ReadableStream::errored() const
{
    return StreamState(this) & Errored;
}

bool
ReadableStream::disturbed() const
{
    return StreamState(this) & Disturbed;
}

inline static bool
ReaderHasStream(const NativeObject* reader)
{
    MOZ_ASSERT(JS::IsReadableStreamReader(reader));
    return !reader->getFixedSlot(ReaderSlot_Stream).isUndefined();
}

bool
js::ReadableStreamReaderIsClosed(const JSObject* reader)
{
    return !ReaderHasStream(&reader->as<NativeObject>());
}

inline static MOZ_MUST_USE ReadableStream*
StreamFromController(const NativeObject* controller)
{
    MOZ_ASSERT(IsReadableStreamController(controller));
    return UnwrapInternalSlotObject<ReadableStream>(
        &controller->getFixedSlot(ControllerSlot_Stream).toObject());
}

inline static MOZ_MUST_USE NativeObject*
ControllerFromStream(const ReadableStream* stream)
{
    Value controllerVal = stream->getFixedSlot(StreamSlot_Controller);
    MOZ_ASSERT(IsReadableStreamController(&controllerVal.toObject()));
    return &controllerVal.toObject().as<NativeObject>();
}

inline static bool
HasController(const ReadableStream* stream)
{
    return !stream->getFixedSlot(StreamSlot_Controller).isUndefined();
}

JS::ReadableStreamMode
ReadableStream::mode() const
{
    NativeObject* controller = ControllerFromStream(this);
    if (controller->is<ReadableStreamDefaultController>())
        return JS::ReadableStreamMode::Default;
    return controller->as<ReadableByteStreamController>().hasExternalSource()
           ? JS::ReadableStreamMode::ExternalSource
           : JS::ReadableStreamMode::Byte;
}

inline static MOZ_MUST_USE ReadableStream*
StreamFromReader(const NativeObject* reader)
{
    MOZ_ASSERT(ReaderHasStream(reader));
    return UnwrapInternalSlotObject<ReadableStream>(
        &reader->getFixedSlot(ReaderSlot_Stream).toObject());
}

inline static MOZ_MUST_USE NativeObject*
ReaderFromStream(const NativeObject* stream)
{
    Value readerVal = stream->getFixedSlot(StreamSlot_Reader);
    NativeObject* reader = UnwrapInternalSlotObject<NativeObject>(&readerVal.toObject());
    MOZ_ASSERT(JS::IsReadableStreamReader(reader));
    return reader;
}

inline static bool
HasReader(const ReadableStream* stream)
{
    return !stream->getFixedSlot(StreamSlot_Reader).isUndefined();
}

inline static MOZ_MUST_USE JSFunction*
NewHandler(JSContext *cx, Native handler, HandleObject target)
{
    RootedAtom funName(cx, cx->names().empty);
    RootedFunction handlerFun(cx, NewNativeFunction(cx, handler, 0, funName,
                                                    gc::AllocKind::FUNCTION_EXTENDED,
                                                    GenericObject));
    if (!handlerFun)
        return nullptr;
    handlerFun->setExtendedSlot(0, ObjectValue(*target));
    return handlerFun;
}

template<class T>
inline static MOZ_MUST_USE T*
TargetFromHandler(JSObject& handler)
{
    return &handler.as<JSFunction>().getExtendedSlot(0).toObject().as<T>();
}

inline static MOZ_MUST_USE bool
ResetQueue(JSContext* cx, HandleNativeObject container);

inline static MOZ_MUST_USE bool
InvokeOrNoop(JSContext* cx, HandleValue O, HandlePropertyName P, HandleValue arg,
             MutableHandleValue rval);

static MOZ_MUST_USE JSObject*
PromiseInvokeOrNoop(JSContext* cx, HandleValue O, HandlePropertyName P, HandleValue arg);

static MOZ_MUST_USE JSObject*
PromiseRejectedWithPendingError(JSContext* cx) {
    // Not much we can do about uncatchable exceptions, just bail.
    RootedValue exn(cx);
    if (!GetAndClearException(cx, &exn))
        return nullptr;
    return PromiseObject::unforgeableReject(cx, exn);
}

static bool
ReportArgTypeError(JSContext* cx, const char* funName, const char* expectedType,
                   HandleValue arg)
{
    UniqueChars bytes = DecompileValueGenerator(cx, JSDVG_SEARCH_STACK, arg, nullptr);
    if (!bytes)
        return false;

    return JS_ReportErrorFlagsAndNumberLatin1(cx, JSREPORT_ERROR, GetErrorMessage,
                                              nullptr, JSMSG_NOT_EXPECTED_TYPE,
                                              funName, expectedType, bytes.get());
}

static MOZ_MUST_USE bool
ReturnPromiseRejectedWithPendingError(JSContext* cx, const CallArgs& args)
{
    JSObject* promise = PromiseRejectedWithPendingError(cx);
    if (!promise)
        return false;

    args.rval().setObject(*promise);
    return true;
}

static MOZ_MUST_USE bool
RejectNonGenericMethod(JSContext* cx, const CallArgs& args,
                       const char* className, const char* methodName)
{
    ReportValueError3(cx, JSMSG_INCOMPATIBLE_PROTO, JSDVG_SEARCH_STACK, args.thisv(),
                      nullptr, className, methodName);

    return ReturnPromiseRejectedWithPendingError(cx, args);
}

[[nodiscard]] static bool
ResolveMaybeWrappedPromise(JSContext* cx, HandleObject promise, HandleValue value)
{
    RootedObject wrappedPromise(cx, promise);
    RootedValue wrappedValue(cx, value);
    if (!JS_WrapObject(cx, &wrappedPromise) || !JS_WrapValue(cx, &wrappedValue))
        return false;
    return ResolvePromise(cx, wrappedPromise, wrappedValue);
}

[[nodiscard]] static bool
RejectMaybeWrappedPromise(JSContext* cx, HandleObject promise, HandleValue reason)
{
    RootedObject wrappedPromise(cx, promise);
    RootedValue wrappedReason(cx, reason);
    if (!JS_WrapObject(cx, &wrappedPromise) || !JS_WrapValue(cx, &wrappedReason))
        return false;
    return RejectPromise(cx, wrappedPromise, wrappedReason);
}

[[nodiscard]] inline static NativeObject*
SetNewList(JSContext* cx, HandleNativeObject container, uint32_t slot)
{
    RootedNativeObject list(cx);
    {
        JSAutoCompartment ac(cx, container);
        list = NewObjectWithNullTaggedProto<PlainObject>(cx);
        if (!list)
            return nullptr;
        container->setFixedSlot(slot, ObjectValue(*list));
    }
    return list;
}

inline static MOZ_MUST_USE bool
AppendToList(JSContext* cx, HandleNativeObject list, HandleValue value)
{
    JSAutoCompartment ac(cx, list);
    RootedValue wrappedValue(cx, value);
    if (!JS_WrapValue(cx, &wrappedValue))
        return false;

    uint32_t length = list->getDenseInitializedLength();

    if (!list->ensureElements(cx, length + 1))
        return false;

    list->ensureDenseInitializedLength(cx, length, 1);
    list->setDenseElement(length, wrappedValue);

    return true;
}

template<class T>
inline static MOZ_MUST_USE T*
PeekList(NativeObject* list)
{
    MOZ_ASSERT(list->getDenseInitializedLength() > 0);
    return &list->getDenseElement(0).toObject().as<T>();
}

template<class T>
inline static MOZ_MUST_USE T*
ShiftFromList(JSContext* cx, HandleNativeObject list)
{
    JSAutoCompartment ac(cx, list);
    uint32_t length = list->getDenseInitializedLength();
    MOZ_ASSERT(length > 0);

    Rooted<T*> entry(cx, &list->getDenseElement(0).toObject().as<T>());
    list->moveDenseElements(0, 1, length - 1);
    list->shrinkElements(cx, length - 1);

    list->setDenseInitializedLength(length - 1);

    return entry;
}

class ByteStreamChunk : public NativeObject
{
  private:
    enum Slots {
        Slot_Buffer = 0,
        Slot_ByteOffset,
        Slot_ByteLength,
        SlotCount
    };

  public:
    static const Class class_;

    ArrayBufferObject* buffer() {
        return &getFixedSlot(Slot_Buffer).toObject().as<ArrayBufferObject>();
    }
    uint32_t byteOffset() { return getFixedSlot(Slot_ByteOffset).toInt32(); }
    void SetByteOffset(uint32_t offset) {
        setFixedSlot(Slot_ByteOffset, Int32Value(offset));
    }
    uint32_t byteLength() { return getFixedSlot(Slot_ByteLength).toInt32(); }
    void SetByteLength(uint32_t length) {
        setFixedSlot(Slot_ByteLength, Int32Value(length));
    }

    static ByteStreamChunk* create(JSContext* cx, HandleObject buffer, uint32_t byteOffset,
                                   uint32_t byteLength)
   {
        Rooted<ByteStreamChunk*> chunk(cx, NewObjectWithClassProto<ByteStreamChunk>(cx));
        if (!chunk)
            return nullptr;

        chunk->setFixedSlot(Slot_Buffer, ObjectValue(*buffer));
        chunk->setFixedSlot(Slot_ByteOffset, Int32Value(byteOffset));
        chunk->setFixedSlot(Slot_ByteLength, Int32Value(byteLength));
        return chunk;
   }
};

const Class ByteStreamChunk::class_ = {
    "ByteStreamChunk",
    JSCLASS_HAS_RESERVED_SLOTS(SlotCount)
};

class PullIntoDescriptor : public NativeObject
{
  private:
    enum Slots {
        Slot_buffer,
        Slot_ByteOffset,
        Slot_ByteLength,
        Slot_BytesFilled,
        Slot_ElementSize,
        Slot_Ctor,
        Slot_ReaderType,
        SlotCount
    };
  public:
    static const Class class_;

    ArrayBufferObject* buffer() {
        return &getFixedSlot(Slot_buffer).toObject().as<ArrayBufferObject>();
    }
    void setBuffer(ArrayBufferObject* buffer) { setFixedSlot(Slot_buffer, ObjectValue(*buffer)); }
    JSObject* ctor() { return getFixedSlot(Slot_Ctor).toObjectOrNull(); }
    uint32_t byteOffset() const { return getFixedSlot(Slot_ByteOffset).toInt32(); }
    uint32_t byteLength() const { return getFixedSlot(Slot_ByteLength).toInt32(); }
    uint32_t bytesFilled() const { return getFixedSlot(Slot_BytesFilled).toInt32(); }
    void setBytesFilled(int32_t bytes) { setFixedSlot(Slot_BytesFilled, Int32Value(bytes)); }
    uint32_t elementSize() const { return getFixedSlot(Slot_ElementSize).toInt32(); }
    uint32_t readerType() const { return getFixedSlot(Slot_ReaderType).toInt32(); }

    static PullIntoDescriptor* create(JSContext* cx, HandleArrayBufferObject buffer,
                                      uint32_t byteOffset, uint32_t byteLength,
                                      uint32_t bytesFilled, uint32_t elementSize,
                                      HandleObject ctor, uint32_t readerType)
   {
        Rooted<PullIntoDescriptor*> descriptor(cx, NewObjectWithClassProto<PullIntoDescriptor>(cx));
        if (!descriptor)
            return nullptr;

        descriptor->setFixedSlot(Slot_buffer, ObjectValue(*buffer));
        descriptor->setFixedSlot(Slot_Ctor, ObjectOrNullValue(ctor));
        descriptor->setFixedSlot(Slot_ByteOffset, Int32Value(byteOffset));
        descriptor->setFixedSlot(Slot_ByteLength, Int32Value(byteLength));
        descriptor->setFixedSlot(Slot_BytesFilled, Int32Value(bytesFilled));
        descriptor->setFixedSlot(Slot_ElementSize, Int32Value(elementSize));
        descriptor->setFixedSlot(Slot_ReaderType, Int32Value(readerType));
        return descriptor;
   }
};

const Class PullIntoDescriptor::class_ = {
    "PullIntoDescriptor",
    JSCLASS_HAS_RESERVED_SLOTS(SlotCount)
};

class QueueEntry : public NativeObject
{
  private:
    enum Slots {
        Slot_Value = 0,
        Slot_Size,
        SlotCount
    };

  public:
    static const Class class_;

    Value value() { return getFixedSlot(Slot_Value); }
    double size() { return getFixedSlot(Slot_Size).toNumber(); }

    static QueueEntry* create(JSContext* cx, HandleValue value, double size)
   {
        Rooted<QueueEntry*> entry(cx, NewObjectWithClassProto<QueueEntry>(cx));
        if (!entry)
            return nullptr;

        entry->setFixedSlot(Slot_Value, value);
        entry->setFixedSlot(Slot_Size, NumberValue(size));

        return entry;
   }
};

const Class QueueEntry::class_ = {
    "QueueEntry",
    JSCLASS_HAS_RESERVED_SLOTS(SlotCount)
};

class TeeState : public NativeObject
{
  private:
    enum Slots {
        Slot_Flags = 0,
        Slot_Reason1,
        Slot_Reason2,
        Slot_Promise,
        Slot_Stream,
        Slot_Branch1,
        Slot_Branch2,
        SlotCount
    };

    enum Flags
    {
        Flag_ClosedOrErrored = 1 << 0,
        Flag_Canceled1 =       1 << 1,
        Flag_Canceled2 =       1 << 2,
        Flag_CloneForBranch2 = 1 << 3,
    };
    uint32_t flags() const { return getFixedSlot(Slot_Flags).toInt32(); }
    void setFlags(uint32_t flags) { setFixedSlot(Slot_Flags, Int32Value(flags)); }

  public:
    static const Class class_;

    bool cloneForBranch2() const { return flags() & Flag_CloneForBranch2; }

    bool closedOrErrored() const { return flags() & Flag_ClosedOrErrored; }
    void setClosedOrErrored() {
        MOZ_ASSERT(!(flags() & Flag_ClosedOrErrored));
        setFlags(flags() | Flag_ClosedOrErrored);
    }

    bool canceled1() const { return flags() & Flag_Canceled1; }
    void setCanceled1(HandleValue reason) {
        MOZ_ASSERT(!(flags() & Flag_Canceled1));
        setFlags(flags() | Flag_Canceled1);
        setFixedSlot(Slot_Reason1, reason);
    }

    bool canceled2() const { return flags() & Flag_Canceled2; }
    void setCanceled2(HandleValue reason) {
        MOZ_ASSERT(!(flags() & Flag_Canceled2));
        setFlags(flags() | Flag_Canceled2);
        setFixedSlot(Slot_Reason2, reason);
    }

    Value reason1() const {
        MOZ_ASSERT(canceled1());
        return getFixedSlot(Slot_Reason1);
    }

    Value reason2() const {
        MOZ_ASSERT(canceled2());
        return getFixedSlot(Slot_Reason2);
    }

    PromiseObject* promise() {
        return &getFixedSlot(Slot_Promise).toObject().as<PromiseObject>();
    }
    ReadableStream* stream() {
        return &getFixedSlot(Slot_Stream).toObject().as<ReadableStream>();
    }
    ReadableStreamDefaultReader* reader() {
        return &ReaderFromStream(stream())->as<ReadableStreamDefaultReader>();
    }

    ReadableStreamDefaultController* branch1() {
        ReadableStreamDefaultController* controller = &getFixedSlot(Slot_Branch1).toObject()
                                                       .as<ReadableStreamDefaultController>();
        MOZ_ASSERT(ControllerFlags(controller) & ControllerFlag_TeeBranch);
        MOZ_ASSERT(ControllerFlags(controller) & ControllerFlag_TeeBranch1);
        return controller;
    }
    void setBranch1(ReadableStreamDefaultController* controller) {
        MOZ_ASSERT(ControllerFlags(controller) & ControllerFlag_TeeBranch);
        MOZ_ASSERT(ControllerFlags(controller) & ControllerFlag_TeeBranch1);
        setFixedSlot(Slot_Branch1, ObjectValue(*controller));
    }

    ReadableStreamDefaultController* branch2() {
        ReadableStreamDefaultController* controller = &getFixedSlot(Slot_Branch2).toObject()
                                                       .as<ReadableStreamDefaultController>();
        MOZ_ASSERT(ControllerFlags(controller) & ControllerFlag_TeeBranch);
        MOZ_ASSERT(ControllerFlags(controller) & ControllerFlag_TeeBranch2);
        return controller;
    }
    void setBranch2(ReadableStreamDefaultController* controller) {
        MOZ_ASSERT(ControllerFlags(controller) & ControllerFlag_TeeBranch);
        MOZ_ASSERT(ControllerFlags(controller) & ControllerFlag_TeeBranch2);
        setFixedSlot(Slot_Branch2, ObjectValue(*controller));
    }

    static TeeState* create(JSContext* cx, Handle<ReadableStream*> stream) {
        Rooted<TeeState*> state(cx, NewObjectWithClassProto<TeeState>(cx));
        if (!state)
            return nullptr;

        Rooted<PromiseObject*> promise(cx, PromiseObject::createSkippingExecutor(cx));
        if (!promise)
            return nullptr;

        state->setFixedSlot(Slot_Flags, Int32Value(0));
        state->setFixedSlot(Slot_Promise, ObjectValue(*promise));
        state->setFixedSlot(Slot_Stream, ObjectValue(*stream));

        return state;
   }
};

const Class TeeState::class_ = {
    "TeeState",
    JSCLASS_HAS_RESERVED_SLOTS(SlotCount)
};

#define CLASS_SPEC(cls, nCtorArgs, nSlots, specFlags, classFlags, classOps) \
const ClassSpec cls::classSpec_ = { \
    GenericCreateConstructor<cls::constructor, nCtorArgs, gc::AllocKind::FUNCTION>, \
    GenericCreatePrototype, \
    nullptr, \
    nullptr, \
    cls##_methods, \
    cls##_properties, \
    nullptr, \
    specFlags \
}; \
\
const Class cls::class_ = { \
    #cls, \
    JSCLASS_HAS_RESERVED_SLOTS(nSlots) | \
    JSCLASS_HAS_CACHED_PROTO(JSProto_##cls) | \
    classFlags, \
    classOps, \
    &cls::classSpec_ \
}; \
\
const Class cls::protoClass_ = { \
    "object", \
    JSCLASS_HAS_CACHED_PROTO(JSProto_##cls), \
    JS_NULL_CLASS_OPS, \
    &cls::classSpec_ \
};

// Streams spec, 3.2.3., steps 1-4.
ReadableStream*
ReadableStream::createStream(JSContext* cx, HandleObject proto /* = nullptr */)
{
    Rooted<ReadableStream*> stream(cx, NewObjectWithClassProto<ReadableStream>(cx, proto));
    if (!stream)
        return nullptr;

    // Step 1: Set this.[[state]] to "readable".
    // Step 2: Set this.[[reader]] and this.[[storedError]] to undefined (implicit).
    // Step 3: Set this.[[disturbed]] to false (implicit).
    // Step 4: Set this.[[readableStreamController]] to undefined (implicit).
    stream->setFixedSlot(StreamSlot_State, Int32Value(Readable));

    return stream;
}

static MOZ_MUST_USE ReadableStreamDefaultController*
CreateReadableStreamDefaultController(JSContext* cx, Handle<ReadableStream*> stream,
                                      HandleValue underlyingSource, HandleValue size,
                                      HandleValue highWaterMarkVal);

// Streams spec, 3.2.3., steps 1-4, 8.
ReadableStream*
ReadableStream::createDefaultStream(JSContext* cx, HandleValue underlyingSource,
                                    HandleValue size, HandleValue highWaterMark,
                                    HandleObject proto /* = nullptr */)
{
    // Steps 1-4.
    Rooted<ReadableStream*> stream(cx, createStream(cx));
    if (!stream)
        return nullptr;

    // Step 8.b: Set this.[[readableStreamController]] to
    //           ? Construct(ReadableStreamDefaultController,
    //                       « this, underlyingSource, size,
    //                         highWaterMark »).
    RootedObject controller(cx, CreateReadableStreamDefaultController(cx, stream,
                                                                      underlyingSource,
                                                                      size,
                                                                      highWaterMark));
    if (!controller)
        return nullptr;

    stream->setFixedSlot(StreamSlot_Controller, ObjectValue(*controller));

    return stream;
}

static MOZ_MUST_USE ReadableByteStreamController*
CreateReadableByteStreamController(JSContext* cx, Handle<ReadableStream*> stream,
                                   HandleValue underlyingByteSource,
                                   HandleValue highWaterMarkVal);

// Streams spec, 3.2.3., steps 1-4, 7.
ReadableStream*
ReadableStream::createByteStream(JSContext* cx, HandleValue underlyingSource,
                                 HandleValue highWaterMark, HandleObject proto /* = nullptr */)
{
    // Steps 1-4.
    Rooted<ReadableStream*> stream(cx, createStream(cx, proto));
    if (!stream)
        return nullptr;

    // Step 7.b: Set this.[[readableStreamController]] to
    //           ? Construct(ReadableByteStreamController,
    //                       « this, underlyingSource, highWaterMark »).
    RootedObject controller(cx, CreateReadableByteStreamController(cx, stream,
                                                                   underlyingSource,
                                                                   highWaterMark));
    if (!controller)
        return nullptr;

    stream->setFixedSlot(StreamSlot_Controller, ObjectValue(*controller));

    return stream;
}

static MOZ_MUST_USE ReadableByteStreamController*
CreateReadableByteStreamController(JSContext* cx, Handle<ReadableStream*> stream,
                                   void* underlyingSource);

ReadableStream*
ReadableStream::createExternalSourceStream(JSContext* cx, void* underlyingSource,
                                           uint8_t flags, HandleObject proto /* = nullptr */)
{
    Rooted<ReadableStream*> stream(cx, createStream(cx, proto));
    if (!stream)
        return nullptr;

    RootedNativeObject controller(cx, CreateReadableByteStreamController(cx, stream,
                                                                         underlyingSource));
    if (!controller)
        return nullptr;

    stream->setFixedSlot(StreamSlot_Controller, ObjectValue(*controller));
    AddControllerFlags(controller, flags << ControllerEmbeddingFlagsOffset);

    return stream;
}

// Streams spec, 3.2.3.
bool
ReadableStream::constructor(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedValue val(cx, args.get(0));
    RootedValue underlyingSource(cx, args.get(0));
    RootedValue options(cx, args.get(1));

    // Do argument handling first to keep the right order of error reporting.
    if (underlyingSource.isUndefined()) {
        RootedObject sourceObj(cx, NewBuiltinClassInstance<PlainObject>(cx));
        if (!sourceObj)
            return false;
        underlyingSource = ObjectValue(*sourceObj);
    }
    RootedValue size(cx);
    RootedValue highWaterMark(cx);

    if (!options.isUndefined()) {
        if (!GetProperty(cx, options, cx->names().size, &size))
            return false;

        if (!GetProperty(cx, options, cx->names().highWaterMark, &highWaterMark))
            return false;
    }

    if (!ThrowIfNotConstructing(cx, args, "ReadableStream"))
        return false;

    // Step 5: Let type be ? GetV(underlyingSource, "type").
    RootedValue typeVal(cx);
    if (!GetProperty(cx, underlyingSource, cx->names().type, &typeVal))
        return false;

    // Step 6: Let typeString be ? ToString(type).
    RootedString type(cx, ToString<CanGC>(cx, typeVal));
    if (!type)
        return false;

    int32_t notByteStream;
    if (!CompareStrings(cx, type, cx->names().bytes, &notByteStream))
        return false;

    // Step 7.a & 8.a (reordered): If highWaterMark is undefined, let
    //                             highWaterMark be 1 (or 0 for byte streams).
    if (highWaterMark.isUndefined())
        highWaterMark = Int32Value(notByteStream ? 1 : 0);

    Rooted<ReadableStream*> stream(cx);

    // Step 7: If typeString is "bytes",
    if (!notByteStream) {
        // Step 7.b: Set this.[[readableStreamController]] to
        //           ? Construct(ReadableByteStreamController,
        //                       « this, underlyingSource, highWaterMark »).
        stream = createByteStream(cx, underlyingSource, highWaterMark);
    } else if (typeVal.isUndefined()) {
        // Step 8: Otherwise, if type is undefined,
        // Step 8.b: Set this.[[readableStreamController]] to
        //           ? Construct(ReadableStreamDefaultController,
        //                       « this, underlyingSource, size, highWaterMark »).
        stream = createDefaultStream(cx, underlyingSource, size, highWaterMark);
    } else {
        // Step 9: Otherwise, throw a RangeError exception.
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAM_UNDERLYINGSOURCE_TYPE_WRONG);
        return false;
    }
    if (!stream)
        return false;

    args.rval().setObject(*stream);
    return true;
}

// Streams spec, 3.2.4.1. get locked
static MOZ_MUST_USE bool
ReadableStream_locked_impl(JSContext* cx, const CallArgs& args)
{
    Rooted<ReadableStream*> stream(cx, &args.thisv().toObject().as<ReadableStream>());

    // Step 2: Return ! IsReadableStreamLocked(this).
    args.rval().setBoolean(stream->locked());
    return true;
}

static bool
ReadableStream_locked(JSContext* cx, unsigned argc, Value* vp)
{
    // Step 1: If ! IsReadableStream(this) is false, throw a TypeError exception.
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<Is<ReadableStream>, ReadableStream_locked_impl>(cx, args);
}

// Streams spec, 3.2.4.2. cancel ( reason )
static MOZ_MUST_USE bool
ReadableStream_cancel(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    // Step 1: If ! IsReadableStream(this) is false, return a promise rejected
    //         with a TypeError exception.
    if (!Is<ReadableStream>(args.thisv())) {
        ReportValueError3(cx, JSMSG_INCOMPATIBLE_PROTO, JSDVG_SEARCH_STACK, args.thisv(),
                          nullptr, "cancel", "");
        return ReturnPromiseRejectedWithPendingError(cx, args);
    }

    Rooted<ReadableStream*> stream(cx, &args.thisv().toObject().as<ReadableStream>());

    // Step 2: If ! IsReadableStreamLocked(this) is true, return a promise
    //         rejected with a TypeError exception.
    if (stream->locked()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAM_NOT_LOCKED, "cancel");
        return ReturnPromiseRejectedWithPendingError(cx, args);
    }

    // Step 3: Return ! ReadableStreamCancel(this, reason).
    RootedObject cancelPromise(cx, ReadableStream::cancel(cx, stream, args.get(0)));
    if (!cancelPromise)
        return false;
    args.rval().setObject(*cancelPromise);
    return true;
}

static MOZ_MUST_USE ReadableStreamDefaultReader*
CreateReadableStreamDefaultReader(JSContext* cx, Handle<ReadableStream*> stream);

static MOZ_MUST_USE ReadableStreamBYOBReader*
CreateReadableStreamBYOBReader(JSContext* cx, Handle<ReadableStream*> stream);

// Streams spec, 3.2.4.3. getReader()
static MOZ_MUST_USE bool
ReadableStream_getReader_impl(JSContext* cx, const CallArgs& args)
{
    Rooted<ReadableStream*> stream(cx, &args.thisv().toObject().as<ReadableStream>());
    RootedObject reader(cx);

    // Step 2: If mode is undefined, return
    //         ? AcquireReadableStreamDefaultReader(this).
    RootedValue modeVal(cx);
    HandleValue optionsVal = args.get(0);
    if (!optionsVal.isUndefined()) {
        if (!GetProperty(cx, optionsVal, cx->names().mode, &modeVal))
            return false;
    }

    if (modeVal.isUndefined()) {
        reader = CreateReadableStreamDefaultReader(cx, stream);
    } else {
        // Step 3: Set mode to ? ToString(mode) (implicit).
        RootedString mode(cx, ToString<CanGC>(cx, modeVal));
        if (!mode)
            return false;

        // Step 4: If mode is "byob", return ? AcquireReadableStreamBYOBReader(this).
        int32_t notByob;
        if (!CompareStrings(cx, mode, cx->names().byob, &notByob))
            return false;
        if (notByob) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                      JSMSG_READABLESTREAM_INVALID_READER_MODE);
            // Step 5: Throw a RangeError exception.
            return false;

        }
        reader = CreateReadableStreamBYOBReader(cx, stream);
    }

    // Reordered second part of steps 2 and 4.
    if (!reader)
        return false;
    args.rval().setObject(*reader);
    return true;
}

static bool
ReadableStream_getReader(JSContext* cx, unsigned argc, Value* vp)
{
    // Step 1: If ! IsReadableStream(this) is false, throw a TypeError exception.
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<Is<ReadableStream>, ReadableStream_getReader_impl>(cx, args);
}

[[nodiscard]] static bool
CallStreamExtra(JSContext* cx, const char* name, HandleValue streamVal,
                HandleValue arg0, HandleValue arg1, MutableHandleValue rval)
{
    RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
    if (!global)
        return false;

    RootedValue fun(cx);
    if (!JS_GetProperty(cx, global, name, &fun))
        return false;

    if (!fun.isObject() || !IsCallable(fun)) {
        if (!InitStreamExtras(cx, global))
            return false;
        if (!JS_GetProperty(cx, global, name, &fun))
            return false;
    }

    if (!fun.isObject() || !IsCallable(fun)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAM_METHOD_NOT_IMPLEMENTED, name);
        return false;
    }

    FixedInvokeArgs<3> invokeArgs(cx);
    invokeArgs[0].set(streamVal);
    invokeArgs[1].set(arg0);
    invokeArgs[2].set(arg1);
    return Call(cx, fun, UndefinedHandleValue, invokeArgs, rval);
}

// Streams spec, 3.2.4.4. pipeThrough({ writable, readable }, options)
[[nodiscard]] static bool
ReadableStream_pipeThrough(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (!Is<ReadableStream>(args.thisv())) {
        ReportValueError3(cx, JSMSG_INCOMPATIBLE_PROTO, JSDVG_SEARCH_STACK, args.thisv(),
                          nullptr, "pipeThrough", "");
        return false;
    }

    RootedValue rval(cx);
    if (!CallStreamExtra(cx, "__uxpStreamsPipeThrough", args.thisv(),
                         args.get(0), args.get(1), &rval))
        return false;

    args.rval().set(rval);
    return true;
}

// Streams spec, 3.2.4.5. pipeTo(dest, { preventClose, preventAbort, preventCancel } = {})
[[nodiscard]] static bool
ReadableStream_pipeTo(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (!Is<ReadableStream>(args.thisv())) {
        ReportValueError3(cx, JSMSG_INCOMPATIBLE_PROTO, JSDVG_SEARCH_STACK, args.thisv(),
                          nullptr, "pipeTo", "");
        return false;
    }

    RootedValue rval(cx);
    if (!CallStreamExtra(cx, "__uxpStreamsPipeTo", args.thisv(),
                         args.get(0), args.get(1), &rval))
        return false;

    args.rval().set(rval);
    return true;
}

[[nodiscard]] static bool
ReadableStreamTee(JSContext* cx, Handle<ReadableStream*> stream, bool cloneForBranch2,
                  MutableHandle<ReadableStream*> branch1, MutableHandle<ReadableStream*> branch2);

// Streams spec, 3.2.4.6. tee()
static MOZ_MUST_USE bool
ReadableStream_tee_impl(JSContext* cx, const CallArgs& args)
{
    Rooted<ReadableStream*> stream(cx, &args.thisv().toObject().as<ReadableStream>());

    // Step 2: Let branches be ? ReadableStreamTee(this, false).
    Rooted<ReadableStream*> branch1(cx);
    Rooted<ReadableStream*> branch2(cx);
    if (!ReadableStreamTee(cx, stream, false, &branch1, &branch2))
        return false;

    // Step 3: Return ! CreateArrayFromList(branches).
    RootedNativeObject branches(cx, NewDenseFullyAllocatedArray(cx, 2));
    if (!branches)
        return false;
    branches->setDenseInitializedLength(2);
    branches->initDenseElement(0, ObjectValue(*branch1));
    branches->initDenseElement(1, ObjectValue(*branch2));

    args.rval().setObject(*branches);
    return true;
}

static bool
ReadableStream_tee(JSContext* cx, unsigned argc, Value* vp)
{
    // Step 1: If ! IsReadableStream(this) is false, throw a TypeError exception
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<Is<ReadableStream>, ReadableStream_tee_impl>(cx, args);
}

static const JSFunctionSpec ReadableStream_methods[] = {
    JS_FN("cancel",         ReadableStream_cancel,      1, 0),
    JS_FN("getReader",      ReadableStream_getReader,   0, 0),
    JS_FN("pipeThrough",    ReadableStream_pipeThrough, 2, 0),
    JS_FN("pipeTo",         ReadableStream_pipeTo,      1, 0),
    JS_FN("tee",            ReadableStream_tee,         0, 0),
    JS_FS_END
};

static const JSPropertySpec ReadableStream_properties[] = {
    JS_PSG("locked", ReadableStream_locked, 0),
    JS_PS_END
};

CLASS_SPEC(ReadableStream, 0, StreamSlotCount, 0, 0, JS_NULL_CLASS_OPS);

// Streams spec, 3.3.1. AcquireReadableStreamBYOBReader ( stream )
// Always inlined.

// Streams spec, 3.3.2. AcquireReadableStreamDefaultReader ( stream )
// Always inlined.

// Streams spec, 3.3.3. IsReadableStream ( x )
// Using is<T> instead.

// Streams spec, 3.3.4. IsReadableStreamDisturbed ( stream )
// Using stream->disturbed() instead.

// Streams spec, 3.3.5. IsReadableStreamLocked ( stream )
bool
ReadableStream::locked() const
{
    // Step 1: Assert: ! IsReadableStream(stream) is true (implicit).
    // Step 2: If stream.[[reader]] is undefined, return false.
    // Step 3: Return true.
    // Special-casing for streams with external sources. Those can be locked
    // explicitly via JSAPI, which is indicated by a controller flag.
    // IsReadableStreamLocked is called from the controller's constructor, at
    // which point we can't yet call ControllerFromStream(stream), but the
    // source also can't be locked yet.
    if (HasController(this) &&
        (ControllerFlags(ControllerFromStream(this)) & ControllerFlag_SourceLocked))
    {
        return true;
    }
    return HasReader(this);
}

static MOZ_MUST_USE bool
ReadableStreamDefaultControllerClose(JSContext* cx,
                                     Handle<ReadableStreamDefaultController*> controller);
static MOZ_MUST_USE bool
ReadableStreamDefaultControllerEnqueue(JSContext* cx,
                                       Handle<ReadableStreamDefaultController*> controller,
                                       HandleValue chunk);

static bool
TeeReaderReadHandler(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    Rooted<TeeState*> teeState(cx, TargetFromHandler<TeeState>(args.callee()));
    HandleValue resultVal = args.get(0);

    // Step a: Assert: Type(result) is Object.
    RootedObject result(cx, &resultVal.toObject());

    // Step b: Let value be ? Get(result, "value").
    RootedValue value(cx);
    if (!GetPropertyPure(cx, result, NameToId(cx->names().value), value.address()))
        return false;

    // Step c: Let done be ? Get(result, "done").
    RootedValue doneVal(cx);
    if (!GetPropertyPure(cx, result, NameToId(cx->names().done), doneVal.address()))
        return false;

    // Step d: Assert: Type(done) is Boolean.
    bool done = doneVal.toBoolean();

    // Step e: If done is true and teeState.[[closedOrErrored]] is false,
    if (done && !teeState->closedOrErrored()) {
        // Step i: If teeState.[[canceled1]] is false,
        if (!teeState->canceled1()) {
            // Step 1: Perform ! ReadableStreamDefaultControllerClose(branch1).
            Rooted<ReadableStreamDefaultController*> branch1(cx, teeState->branch1());
            if (!ReadableStreamDefaultControllerClose(cx, branch1))
                return false;
        }

        // Step ii: If teeState.[[canceled2]] is false,
        if (!teeState->canceled2()) {
            // Step 1: Perform ! ReadableStreamDefaultControllerClose(branch1).
            Rooted<ReadableStreamDefaultController*> branch2(cx, teeState->branch2());
            if (!ReadableStreamDefaultControllerClose(cx, branch2))
                return false;
        }

        // Step iii: Set teeState.[[closedOrErrored]] to true.
        teeState->setClosedOrErrored();
    }

    // Step f: If teeState.[[closedOrErrored]] is true, return.
    if (teeState->closedOrErrored())
        return true;

    // Step g: Let value1 and value2 be value.
    RootedValue value1(cx, value);
    RootedValue value2(cx, value);

    // Step h: If teeState.[[canceled2]] is false and cloneForBranch2 is
    //         true, set value2 to
    //         ? StructuredDeserialize(StructuredSerialize(value2),
    //                                 the current Realm Record).
    // TODO: add StructuredClone() intrinsic.
    MOZ_ASSERT(!teeState->cloneForBranch2(), "tee(cloneForBranch2=true) should not be exposed");

    // Step i: If teeState.[[canceled1]] is false, perform
    //         ? ReadableStreamDefaultControllerEnqueue(branch1, value1).
    Rooted<ReadableStreamDefaultController*> controller(cx);
    if (!teeState->canceled1()) {
        controller = teeState->branch1();
        if (!ReadableStreamDefaultControllerEnqueue(cx, controller, value1))
            return false;
    }

    // Step j: If teeState.[[canceled2]] is false,
    //         perform ? ReadableStreamDefaultControllerEnqueue(branch2, value2).
    if (!teeState->canceled2()) {
        controller = teeState->branch2();
        if (!ReadableStreamDefaultControllerEnqueue(cx, controller, value2))
            return false;
    }

    args.rval().setUndefined();
    return true;
}

static MOZ_MUST_USE JSObject*
ReadableStreamTee_Pull(JSContext* cx, Handle<TeeState*> teeState,
                       Handle<ReadableStream*> branchStream)
{
    // Step 1: Let reader be F.[[reader]], branch1 be F.[[branch1]],
    //         branch2 be F.[[branch2]], teeState be F.[[teeState]], and
    //         cloneForBranch2 be F.[[cloneForBranch2]].

    // Step 2: Return the result of transforming
    //         ! ReadableStreamDefaultReaderRead(reader) by a fulfillment
    //         handler which takes the argument result and performs the
    //         following steps:
    Rooted<ReadableStreamDefaultReader*> reader(cx, teeState->reader());
    RootedObject readPromise(cx, ReadableStreamDefaultReader::read(cx, reader));
    if (!readPromise)
        return nullptr;

    RootedObject onFulfilled(cx, NewHandler(cx, TeeReaderReadHandler, teeState));
    if (!onFulfilled)
        return nullptr;

    return JS::CallOriginalPromiseThen(cx, readPromise, onFulfilled, nullptr);
}

static MOZ_MUST_USE JSObject*
ReadableStreamTee_Cancel(JSContext* cx, Handle<TeeState*> teeState,
                         Handle<ReadableStreamDefaultController*> branch, HandleValue reason)
{
    // Step 1: Let stream be F.[[stream]] and teeState be F.[[teeState]].
    Rooted<ReadableStream*> stream(cx, teeState->stream());

    bool bothBranchesCanceled = false;

    // Step 2: Set teeState.[[canceled1]] to true.
    // Step 3: Set teeState.[[reason1]] to reason.
    if (ControllerFlags(branch) & ControllerFlag_TeeBranch1) {
        teeState->setCanceled1(reason);
        bothBranchesCanceled = teeState->canceled2();
    } else {
        MOZ_ASSERT(ControllerFlags(branch) & ControllerFlag_TeeBranch2);
        teeState->setCanceled2(reason);
        bothBranchesCanceled = teeState->canceled1();
    }

    // Step 4: If teeState.[[canceled1]] is true,
    // Step 4: If teeState.[[canceled2]] is true,
    if (bothBranchesCanceled) {
        // Step a: Let compositeReason be
        //         ! CreateArrayFromList(« teeState.[[reason1]], teeState.[[reason2]] »).
        RootedNativeObject compositeReason(cx, NewDenseFullyAllocatedArray(cx, 2));
        if (!compositeReason)
            return nullptr;

        compositeReason->setDenseInitializedLength(2);
        compositeReason->initDenseElement(0, teeState->reason1());
        compositeReason->initDenseElement(1, teeState->reason2());
        RootedValue compositeReasonVal(cx, ObjectValue(*compositeReason));

        Rooted<PromiseObject*> promise(cx, teeState->promise());

        // Step b: Let cancelResult be ! ReadableStreamCancel(stream, compositeReason).
        RootedObject cancelResult(cx, ReadableStream::cancel(cx, stream, compositeReasonVal));
        if (!cancelResult) {
            if (!RejectPromiseWithPendingError(cx, promise))
                return nullptr;
        } else {
            // Step c: Resolve teeState.[[promise]] with cancelResult.
            RootedValue resultVal(cx, ObjectValue(*cancelResult));
            if (!PromiseObject::resolve(cx, promise, resultVal))
                return nullptr;
        }
    }

    // Step 5: Return teeState.[[promise]].
    return teeState->promise();
}

static MOZ_MUST_USE bool
ReadableStreamControllerError(JSContext* cx, HandleNativeObject controller, HandleValue e);

// Streams spec, 3.3.6. step 21:
// Upon rejection of reader.[[closedPromise]] with reason r,
static bool
TeeReaderClosedHandler(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    Rooted<TeeState*> teeState(cx, TargetFromHandler<TeeState>(args.callee()));
    HandleValue reason = args.get(0);

    // Step a: If teeState.[[closedOrErrored]] is false, then:
    if (!teeState->closedOrErrored()) {
        // Step a.i: Perform ! ReadableStreamDefaultControllerError(pull.[[branch1]], r).
        Rooted<ReadableStreamDefaultController*> branch1(cx, teeState->branch1());
        if (!ReadableStreamControllerError(cx, branch1, reason))
            return false;

        // Step a.ii: Perform ! ReadableStreamDefaultControllerError(pull.[[branch2]], r).
        Rooted<ReadableStreamDefaultController*> branch2(cx, teeState->branch2());
        if (!ReadableStreamControllerError(cx, branch2, reason))
            return false;

        // Step a.iii: Set teeState.[[closedOrErrored]] to true.
        teeState->setClosedOrErrored();
    }

    return true;
}

// Streams spec, 3.3.6. ReadableStreamTee ( stream, cloneForBranch2 )
static MOZ_MUST_USE bool
ReadableStreamTee(JSContext* cx, Handle<ReadableStream*> stream, bool cloneForBranch2,
                  MutableHandle<ReadableStream*> branch1Stream,
                  MutableHandle<ReadableStream*> branch2Stream)
{
    // Step 1: Assert: ! IsReadableStream(stream) is true (implicit).
    // Step 2: Assert: Type(cloneForBranch2) is Boolean (implicit).

    // Step 3: Let reader be ? AcquireReadableStreamDefaultReader(stream).
    Rooted<ReadableStreamDefaultReader*> reader(cx, CreateReadableStreamDefaultReader(cx, stream));
    if (!reader)
        return false;

    // Step 4: Let teeState be Record {[[closedOrErrored]]: false,
    //                                 [[canceled1]]: false,
    //                                 [[canceled2]]: false,
    //                                 [[reason1]]: undefined,
    //                                 [[reason2]]: undefined,
    //                                 [[promise]]: a new promise}.
    Rooted<TeeState*> teeState(cx, TeeState::create(cx, stream));
    if (!teeState)
        return false;

    // Steps 5-10 omitted because our implementation works differently.

    // Step 5: Let pull be a new ReadableStreamTee pull function.
    // Step 6: Set pull.[[reader]] to reader, pull.[[teeState]] to teeState, and
    //         pull.[[cloneForBranch2]] to cloneForBranch2.
    // Step 7: Let cancel1 be a new ReadableStreamTee branch 1 cancel function.
    // Step 8: Set cancel1.[[stream]] to stream and cancel1.[[teeState]] to
    //         teeState.

    // Step 9: Let cancel2 be a new ReadableStreamTee branch 2 cancel function.
    // Step 10: Set cancel2.[[stream]] to stream and cancel2.[[teeState]] to
    //          teeState.

    // Step 11: Let underlyingSource1 be ! ObjectCreate(%ObjectPrototype%).
    // Step 12: Perform ! CreateDataProperty(underlyingSource1, "pull", pull).
    // Step 13: Perform ! CreateDataProperty(underlyingSource1, "cancel", cancel1).

    // Step 14: Let branch1Stream be ! Construct(ReadableStream, underlyingSource1).
    RootedValue hwmValue(cx, NumberValue(1));
    RootedValue underlyingSource(cx, ObjectValue(*teeState));
    branch1Stream.set(ReadableStream::createDefaultStream(cx, underlyingSource,
                                                          UndefinedHandleValue,
                                                          hwmValue));
    if (!branch1Stream)
        return false;

    Rooted<ReadableStreamDefaultController*> branch1(cx);
    branch1 = &ControllerFromStream(branch1Stream)->as<ReadableStreamDefaultController>();
    AddControllerFlags(branch1, ControllerFlag_TeeBranch | ControllerFlag_TeeBranch1);
    teeState->setBranch1(branch1);

    // Step 15: Let underlyingSource2 be ! ObjectCreate(%ObjectPrototype%).
    // Step 16: Perform ! CreateDataProperty(underlyingSource2, "pull", pull).
    // Step 17: Perform ! CreateDataProperty(underlyingSource2, "cancel", cancel2).

    // Step 18: Let branch2Stream be ! Construct(ReadableStream, underlyingSource2).
    branch2Stream.set(ReadableStream::createDefaultStream(cx, underlyingSource,
                                                          UndefinedHandleValue,
                                                          hwmValue));
    if (!branch2Stream)
        return false;

    Rooted<ReadableStreamDefaultController*> branch2(cx);
    branch2 = &ControllerFromStream(branch2Stream)->as<ReadableStreamDefaultController>();
    AddControllerFlags(branch2, ControllerFlag_TeeBranch | ControllerFlag_TeeBranch2);
    teeState->setBranch2(branch2);

    // Step 19: Set pull.[[branch1]] to branch1Stream.[[readableStreamController]].
    // Step 20: Set pull.[[branch2]] to branch2Stream.[[readableStreamController]].

    // Step 21: Upon rejection of reader.[[closedPromise]] with reason r,
    RootedObject closedPromise(cx, &reader->getFixedSlot(ReaderSlot_ClosedPromise).toObject());

    RootedObject onRejected(cx, NewHandler(cx, TeeReaderClosedHandler, teeState));
    if (!onRejected)
        return false;

    if (!JS::AddPromiseReactions(cx, closedPromise, nullptr, onRejected))
        return false;

    // Step 22: Return « branch1, branch2 ».
    return true;
}

// Streams spec, 3.4.1. ReadableStreamAddReadIntoRequest ( stream )
static MOZ_MUST_USE PromiseObject*
ReadableStreamAddReadIntoRequest(JSContext* cx, Handle<ReadableStream*> stream)
{
    // Step 1: MOZ_ASSERT: ! IsReadableStreamBYOBReader(stream.[[reader]]) is true.
    RootedValue val(cx, stream->getFixedSlot(StreamSlot_Reader));
    RootedNativeObject reader(cx, ReaderFromStream(stream));
    MOZ_ASSERT(reader->is<ReadableStreamBYOBReader>());

    // Step 2: MOZ_ASSERT: stream.[[state]] is "readable" or "closed".
    MOZ_ASSERT(stream->readable() || stream->closed());

    // Step 3: Let promise be a new promise.
    Rooted<PromiseObject*> promise(cx, PromiseObject::createSkippingExecutor(cx));
    if (!promise)
        return nullptr;

    // Step 4: Let readIntoRequest be Record {[[promise]]: promise}.
    // Step 5: Append readIntoRequest as the last element of stream.[[reader]].[[readIntoRequests]].
    val = reader->getFixedSlot(ReaderSlot_Requests);
    RootedNativeObject readIntoRequests(cx);
    readIntoRequests = UnwrapInternalSlotObject<NativeObject>(&val.toObject());
    // Since [[promise]] is the Record's only field, we store it directly.
    val = ObjectValue(*promise);
    if (!AppendToList(cx, readIntoRequests, val))
        return nullptr;

    // Step 6: Return promise.
    return promise;
}

// Streams spec, 3.4.2. ReadableStreamAddReadRequest ( stream )
static MOZ_MUST_USE PromiseObject*
ReadableStreamAddReadRequest(JSContext* cx, Handle<ReadableStream*> stream)
{
  MOZ_ASSERT(stream->is<ReadableStream>());

  // Step 1: Assert: ! IsReadableStreamDefaultReader(stream.[[reader]]) is true.
  RootedNativeObject reader(cx, ReaderFromStream(stream));

  // Step 2: Assert: stream.[[state]] is "readable".
  MOZ_ASSERT(stream->readable());

  // Step 3: Let promise be a new promise.
  Rooted<PromiseObject*> promise(cx, PromiseObject::createSkippingExecutor(cx));
  if (!promise)
      return nullptr;

  // Step 4: Let readRequest be Record {[[promise]]: promise}.
  // Step 5: Append readRequest as the last element of stream.[[reader]].[[readRequests]].
  RootedValue val(cx, reader->getFixedSlot(ReaderSlot_Requests));
  RootedNativeObject readRequests(cx);
  readRequests = UnwrapInternalSlotObject<NativeObject>(&val.toObject());

  // Since [[promise]] is the Record's only field, we store it directly.
  val = ObjectValue(*promise);
  if (!AppendToList(cx, readRequests, val))
      return nullptr;

  // Step 6: Return promise.
  return promise;
}

static MOZ_MUST_USE JSObject*
ReadableStreamControllerCancelSteps(JSContext* cx,
                                    HandleNativeObject controller, HandleValue reason);

// Used for transforming the result of promise fulfillment/rejection.
static bool
ReturnUndefined(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setUndefined();
    return true;
}

MOZ_MUST_USE bool
ReadableStreamCloseInternal(JSContext* cx, Handle<ReadableStream*> stream);

// Streams spec, 3.4.3. ReadableStreamCancel ( stream, reason )
/* static */ MOZ_MUST_USE JSObject*
ReadableStream::cancel(JSContext* cx, Handle<ReadableStream*> stream, HandleValue reason)
{
    // Step 1: Set stream.[[disturbed]] to true.
    uint32_t state = StreamState(stream) | ReadableStream::Disturbed;
    SetStreamState(stream, state);

    // Step 2: If stream.[[state]] is "closed", return a new promise resolved
    //         with undefined.
    if (stream->closed())
        return PromiseObject::unforgeableResolve(cx, UndefinedHandleValue);

    // Step 3: If stream.[[state]] is "errored", return a new promise rejected
    //         with stream.[[storedError]].
    if (stream->errored()) {
        RootedValue storedError(cx, stream->getFixedSlot(StreamSlot_StoredError));
        if (!JS_WrapValue(cx, &storedError))
            return nullptr;
        return PromiseObject::unforgeableReject(cx, storedError);
    }

    // Step 4: Perform ! ReadableStreamClose(stream).
    if (!ReadableStreamCloseInternal(cx, stream))
        return nullptr;

    // Step 5: Let sourceCancelPromise be
    //         ! stream.[[readableStreamController]].[[CancelSteps]](reason).
    RootedNativeObject controller(cx, ControllerFromStream(stream));
    RootedObject sourceCancelPromise(cx);
    sourceCancelPromise = ReadableStreamControllerCancelSteps(cx, controller, reason);
    if (!sourceCancelPromise)
        return nullptr;

    // Step 6: Return the result of transforming sourceCancelPromise by a
    //         fulfillment handler that returns undefined.
    RootedAtom funName(cx, cx->names().empty);
    RootedFunction returnUndefined(cx, NewNativeFunction(cx, ReturnUndefined, 0, funName));
    if (!returnUndefined)
        return nullptr;
    return JS::CallOriginalPromiseThen(cx, sourceCancelPromise, returnUndefined, nullptr);
}

// Streams spec, 3.4.4. ReadableStreamClose ( stream )
MOZ_MUST_USE bool
ReadableStreamCloseInternal(JSContext* cx, Handle<ReadableStream*> stream)
{
  // Step 1: Assert: stream.[[state]] is "readable".
  MOZ_ASSERT(stream->readable());

  uint32_t state = StreamState(stream);
  // Step 2: Set stream.[[state]] to "closed".
  SetStreamState(stream, (state & ReadableStream::Disturbed) | ReadableStream::Closed);

  // Step 3: Let reader be stream.[[reader]].
  RootedValue val(cx, stream->getFixedSlot(StreamSlot_Reader));

  // Step 4: If reader is undefined, return.
  if (val.isUndefined())
      return true;

  // Step 5: If ! IsReadableStreamDefaultReader(reader) is true,
  RootedNativeObject reader(cx, ReaderFromStream(stream));
  if (reader->is<ReadableStreamDefaultReader>()) {
      // Step a: Repeat for each readRequest that is an element of
      //         reader.[[readRequests]],
      val = reader->getFixedSlot(ReaderSlot_Requests);
      if (!val.isUndefined()) {
          RootedNativeObject readRequests(cx);
          readRequests = UnwrapInternalSlotObject<NativeObject>(&val.toObject());
          uint32_t len = readRequests->getDenseInitializedLength();
          RootedObject readRequest(cx);
          RootedObject resultObj(cx);
          RootedValue resultVal(cx);
          for (uint32_t i = 0; i < len; i++) {
              // Step i: Resolve readRequest.[[promise]] with
              //         ! CreateIterResultObject(undefined, true).
              readRequest = &readRequests->getDenseElement(i).toObject();
              resultObj = CreateIterResultObject(cx, UndefinedHandleValue, true);
              if (!resultObj)
                  return false;
              resultVal = ObjectValue(*resultObj);
              if (!ResolveMaybeWrappedPromise(cx, readRequest, resultVal))
                  return false;
          }

          // Step b: Set reader.[[readRequests]] to an empty List.
          reader->setFixedSlot(ReaderSlot_Requests, UndefinedValue());
      }
  }

  // Step 6: Resolve reader.[[closedPromise]] with undefined.
  // Step 7: Return (implicit).
  RootedObject closedPromise(cx, &reader->getFixedSlot(ReaderSlot_ClosedPromise).toObject());
  if (!ResolveMaybeWrappedPromise(cx, closedPromise, UndefinedHandleValue))
      return false;

  if (stream->mode() == JS::ReadableStreamMode::ExternalSource &&
      cx->runtime()->readableStreamClosedCallback)
  {
      NativeObject* controller = ControllerFromStream(stream);
      void* source = controller->getFixedSlot(ControllerSlot_UnderlyingSource).toPrivate();
      cx->runtime()->readableStreamClosedCallback(cx, stream, source, stream->embeddingFlags());
  }

  return true;
}

// Streams spec, 3.4.5. ReadableStreamError ( stream, e )
MOZ_MUST_USE bool
ReadableStreamErrorInternal(JSContext* cx, Handle<ReadableStream*> stream, HandleValue e)
{
    // Step 1: Assert: ! IsReadableStream(stream) is true (implicit).

    // Step 2: Assert: stream.[[state]] is "readable".
    MOZ_ASSERT(stream->readable());

    // Step 3: Set stream.[[state]] to "errored".
    uint32_t state = StreamState(stream);
    SetStreamState(stream, (state & ReadableStream::Disturbed) | ReadableStream::Errored);

    // Step 4: Set stream.[[storedError]] to e.
    RootedValue storedError(cx, e);
    {
        JSAutoCompartment ac(cx, stream);
        if (!JS_WrapValue(cx, &storedError))
            return false;
        stream->setFixedSlot(StreamSlot_StoredError, storedError);
    }

    // Step 5: Let reader be stream.[[reader]].
    RootedValue val(cx, stream->getFixedSlot(StreamSlot_Reader));

    // Step 6: If reader is undefined, return.
    if (val.isUndefined())
        return true;
    RootedNativeObject reader(cx, ReaderFromStream(stream));

    // Steps 7,8: (Identical in our implementation.)
    // Step a: Repeat for each readRequest that is an element of
    //         reader.[[readRequests]],
    val = reader->getFixedSlot(ReaderSlot_Requests);
    RootedNativeObject readRequests(cx);
    readRequests = UnwrapInternalSlotObject<NativeObject>(&val.toObject());
    RootedObject readRequest(cx);
    uint32_t len = readRequests->getDenseInitializedLength();
    for (uint32_t i = 0; i < len; i++) {
        // Step i: Reject readRequest.[[promise]] with e.
        val = readRequests->getDenseElement(i);
        readRequest = &val.toObject();
        if (!RejectMaybeWrappedPromise(cx, readRequest, e))
            return false;
    }

    // Step b: Set reader.[[readRequests]] to a new empty List.
    if (!SetNewList(cx, reader, ReaderSlot_Requests))
        return false;

    // Step 9: Reject reader.[[closedPromise]] with e.
    val = reader->getFixedSlot(ReaderSlot_ClosedPromise);
    RootedObject closedPromise(cx, &val.toObject());
    if (!RejectMaybeWrappedPromise(cx, closedPromise, e))
        return false;

    if (stream->mode() == JS::ReadableStreamMode::ExternalSource &&
        cx->runtime()->readableStreamErroredCallback)
    {
        NativeObject* controller = ControllerFromStream(stream);
        void* source = controller->getFixedSlot(ControllerSlot_UnderlyingSource).toPrivate();
        cx->runtime()->readableStreamErroredCallback(cx, stream, source,
                                                     stream->embeddingFlags(), e);
    }

    return true;
}

// Streams spec, 3.4.6. ReadableStreamFulfillReadIntoRequest( stream, chunk, done )
// Streams spec, 3.4.7. ReadableStreamFulfillReadRequest ( stream, chunk, done )
// These two spec functions are identical in our implementation.
static MOZ_MUST_USE bool
ReadableStreamFulfillReadOrReadIntoRequest(JSContext* cx, Handle<ReadableStream*> stream,
                                           HandleValue chunk, bool done)
{
    // Step 1: Let reader be stream.[[reader]].
    RootedValue val(cx, stream->getFixedSlot(StreamSlot_Reader));
    RootedNativeObject reader(cx, ReaderFromStream(stream));

    // Step 2: Let readIntoRequest be the first element of
    //         reader.[[readIntoRequests]].
    // Step 3: Remove readIntoRequest from reader.[[readIntoRequests]], shifting
    //         all other elements downward (so that the second becomes the first,
    //         and so on).
    val = reader->getFixedSlot(ReaderSlot_Requests);
    RootedNativeObject readIntoRequests(cx);
    readIntoRequests = UnwrapInternalSlotObject<NativeObject>(&val.toObject());
    Rooted<PromiseObject*> readIntoRequest(cx);
    readIntoRequest = ShiftFromList<PromiseObject>(cx, readIntoRequests);
    MOZ_ASSERT(readIntoRequest);

    // Step 4: Resolve readIntoRequest.[[promise]] with
    //         ! CreateIterResultObject(chunk, done).
    RootedObject iterResult(cx, CreateIterResultObject(cx, chunk, done));
    if (!iterResult)
        return false;
    val = ObjectValue(*iterResult);
    RootedObject readIntoRequestObj(cx, readIntoRequest);
    return ResolveMaybeWrappedPromise(cx, readIntoRequestObj, val);
}

// Streams spec, 3.4.8. ReadableStreamGetNumReadIntoRequests ( stream )
// Streams spec, 3.4.9. ReadableStreamGetNumReadRequests ( stream )
// (Identical implementation.)
static uint32_t
ReadableStreamGetNumReadRequests(ReadableStream* stream)
{
    // Step 1: Return the number of elements in
    //         stream.[[reader]].[[readRequests]].
    if (!HasReader(stream))
        return 0;
    NativeObject* reader = ReaderFromStream(stream);
    Value readRequests = reader->getFixedSlot(ReaderSlot_Requests);
    NativeObject* requests = UnwrapInternalSlotObject<NativeObject>(&readRequests.toObject());
    return requests->getDenseInitializedLength();
}

// Stream spec 3.4.10. ReadableStreamHasBYOBReader ( stream )
static MOZ_MUST_USE bool
ReadableStreamHasBYOBReader(ReadableStream* stream)
{
    // Step 1: Let reader be stream.[[reader]].
    // Step 2: If reader is undefined, return false.
    // Step 3: If ! IsReadableStreamBYOBReader(reader) is false, return false.
    // Step 4: Return true.
    Value reader = stream->getFixedSlot(StreamSlot_Reader);
    if (!reader.isObject())
        return false;
    JSObject* unwrapped = UncheckedUnwrap(&reader.toObject());
    return unwrapped && unwrapped->is<ReadableStreamBYOBReader>();
}

// Streap spec 3.4.11. ReadableStreamHasDefaultReader ( stream )
static MOZ_MUST_USE bool
ReadableStreamHasDefaultReader(ReadableStream* stream)
{
    // Step 1: Let reader be stream.[[reader]].
    // Step 2: If reader is undefined, return false.
    // Step 3: If ! ReadableStreamDefaultReader(reader) is false, return false.
    // Step 4: Return true.
    Value reader = stream->getFixedSlot(StreamSlot_Reader);
    if (!reader.isObject())
        return false;
    JSObject* unwrapped = UncheckedUnwrap(&reader.toObject());
    return unwrapped && unwrapped->is<ReadableStreamDefaultReader>();
}

static MOZ_MUST_USE bool
ReadableStreamReaderGenericInitialize(JSContext* cx,
                                      HandleNativeObject reader,
                                      Handle<ReadableStream*> stream);

// Stream spec, 3.5.3. new ReadableStreamDefaultReader ( stream )
// Steps 2-4.
static MOZ_MUST_USE ReadableStreamDefaultReader*
CreateReadableStreamDefaultReader(JSContext* cx, Handle<ReadableStream*> stream)
{
    Rooted<ReadableStreamDefaultReader*> reader(cx);
    reader = NewBuiltinClassInstance<ReadableStreamDefaultReader>(cx);
    if (!reader)
        return nullptr;

    // Step 2: If ! IsReadableStreamLocked(stream) is true, throw a TypeError
    //         exception.
    if (stream->locked()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAM_LOCKED);
        return nullptr;
    }

    // Step 3: Perform ! ReadableStreamReaderGenericInitialize(this, stream).
    if (!ReadableStreamReaderGenericInitialize(cx, reader, stream))
        return nullptr;

    // Step 4: Set this.[[readRequests]] to a new empty List.
    if (!SetNewList(cx, reader, ReaderSlot_Requests))
        return nullptr;

    return reader;
}

// Stream spec, 3.5.3. new ReadableStreamDefaultReader ( stream )
bool
ReadableStreamDefaultReader::constructor(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!ThrowIfNotConstructing(cx, args, "ReadableStreamDefaultReader"))
        return false;

    // Step 1: If ! IsReadableStream(stream) is false, throw a TypeError exception.
    Rooted<ReadableStream*> stream(cx, UnwrapMaybeWrappedArg<ReadableStream>(args.get(0)));
    if (!stream) {
        ReportArgTypeError(cx, "ReadableStreamDefaultReader", "ReadableStream",
                           args.get(0));
        return false;
    }

    RootedObject reader(cx, CreateReadableStreamDefaultReader(cx, stream));
    if (!reader)
        return false;

    args.rval().setObject(*reader);
    return true;
}

// Streams spec, 3.5.4.1 get closed
[[nodiscard]] static bool
ReadableStreamDefaultReader_closed_impl(JSContext* cx, const CallArgs& args)
{
    // Step 2: Return this.[[closedPromise]].
    NativeObject* reader = &args.thisv().toObject().as<NativeObject>();
    args.rval().set(reader->getFixedSlot(ReaderSlot_ClosedPromise));
    return true;
}

static bool
ReadableStreamDefaultReader_closed(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Step 1: If ! IsReadableStreamDefaultReader(this) is false, return a promise
    //         rejected with a TypeError exception.
    if (!Is<ReadableStreamDefaultReader>(args.thisv())) {
        if (IsMaybeWrapped<ReadableStreamDefaultReader>(args.thisv())) {
            return CallNonGenericMethod<Is<ReadableStreamDefaultReader>,
                                        ReadableStreamDefaultReader_closed_impl>(cx, args);
        }
        return RejectNonGenericMethod(cx, args, "ReadableStreamDefaultReader", "get closed");
    }

    return ReadableStreamDefaultReader_closed_impl(cx, args);
}

static MOZ_MUST_USE JSObject*
ReadableStreamReaderGenericCancel(JSContext* cx, HandleNativeObject reader, HandleValue reason);

// Streams spec, 3.5.4.2. cancel ( reason )
[[nodiscard]] static bool
ReadableStreamDefaultReader_cancel_impl(JSContext* cx, const CallArgs& args)
{
    // Step 2: If this.[[ownerReadableStream]] is undefined, return a promise
    //         rejected with a TypeError exception.
    RootedNativeObject reader(cx, &args.thisv().toObject().as<NativeObject>());
    if (!ReaderHasStream(reader)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAMREADER_NOT_OWNED, "cancel");
        return ReturnPromiseRejectedWithPendingError(cx, args);
    }

    // Step 3: Return ! ReadableStreamReaderGenericCancel(this, reason).
    JSObject* cancelPromise = ReadableStreamReaderGenericCancel(cx, reader, args.get(0));
    if (!cancelPromise)
        return false;
    args.rval().setObject(*cancelPromise);
    return true;
}

static bool
ReadableStreamDefaultReader_cancel(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Step 1: If ! IsReadableStreamDefaultReader(this) is false, return a promise
    //         rejected with a TypeError exception.
    if (!Is<ReadableStreamDefaultReader>(args.thisv())) {
        if (IsMaybeWrapped<ReadableStreamDefaultReader>(args.thisv())) {
            return CallNonGenericMethod<Is<ReadableStreamDefaultReader>,
                                        ReadableStreamDefaultReader_cancel_impl>(cx, args);
        }
        return RejectNonGenericMethod(cx, args, "ReadableStreamDefaultReader", "cancel");
    }

    return ReadableStreamDefaultReader_cancel_impl(cx, args);
}

// Streams spec, 3.5.4.3 read ( )
[[nodiscard]] static bool
ReadableStreamDefaultReader_read_impl(JSContext* cx, const CallArgs& args)
{
    // Step 2: If this.[[ownerReadableStream]] is undefined, return a promise
    //         rejected with a TypeError exception.
    Rooted<ReadableStreamDefaultReader*> reader(cx);
    reader = &args.thisv().toObject().as<ReadableStreamDefaultReader>();
    if (!ReaderHasStream(reader)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAMREADER_NOT_OWNED, "read");
        return ReturnPromiseRejectedWithPendingError(cx, args);
    }

    // Step 3: Return ! ReadableStreamDefaultReaderRead(this).
    JSObject* readPromise = ReadableStreamDefaultReader::read(cx, reader);
    if (!readPromise)
        return false;
    args.rval().setObject(*readPromise);
    return true;
}

static bool
ReadableStreamDefaultReader_read(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Step 1: If ! IsReadableStreamDefaultReader(this) is false, return a promise
    //         rejected with a TypeError exception.
    if (!Is<ReadableStreamDefaultReader>(args.thisv())) {
        if (IsMaybeWrapped<ReadableStreamDefaultReader>(args.thisv())) {
            return CallNonGenericMethod<Is<ReadableStreamDefaultReader>,
                                        ReadableStreamDefaultReader_read_impl>(cx, args);
        }
        return RejectNonGenericMethod(cx, args, "ReadableStreamDefaultReader", "read");
    }

    return ReadableStreamDefaultReader_read_impl(cx, args);
}

[[nodiscard]] static bool
ReadableStreamReaderGenericRelease(JSContext* cx, HandleNativeObject reader);

// Streams spec, 3.5.4.4. releaseLock ( )
static MOZ_MUST_USE bool
ReadableStreamDefaultReader_releaseLock_impl(JSContext* cx, const CallArgs& args)
{
    Rooted<ReadableStreamDefaultReader*> reader(cx);
    reader = &args.thisv().toObject().as<ReadableStreamDefaultReader>();

    // Step 2: If this.[[ownerReadableStream]] is undefined, return.
    if (!ReaderHasStream(reader)) {
        args.rval().setUndefined();
        return true;
    }

    // Step 3: If this.[[readRequests]] is not empty, throw a TypeError exception.
    Value val = reader->getFixedSlot(ReaderSlot_Requests);
    if (!val.isUndefined()) {
        NativeObject* readRequests = &val.toObject().as<NativeObject>();
        uint32_t len = readRequests->getDenseInitializedLength();
        if (len != 0) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                      JSMSG_READABLESTREAMREADER_NOT_EMPTY,
                                      "releaseLock");
            return false;
        }
    }

    // Step 4: Perform ! ReadableStreamReaderGenericRelease(this).
    return ReadableStreamReaderGenericRelease(cx, reader);
}

static bool
ReadableStreamDefaultReader_releaseLock(JSContext* cx, unsigned argc, Value* vp)
{
    // Step 1: If ! IsReadableStreamDefaultReader(this) is false,
    //         throw a TypeError exception.
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<Is<ReadableStreamDefaultReader>,
                                ReadableStreamDefaultReader_releaseLock_impl>(cx, args);
}

static const JSFunctionSpec ReadableStreamDefaultReader_methods[] = {
    JS_FN("cancel",         ReadableStreamDefaultReader_cancel,         1, 0),
    JS_FN("read",           ReadableStreamDefaultReader_read,           0, 0),
    JS_FN("releaseLock",    ReadableStreamDefaultReader_releaseLock,    0, 0),
    JS_FS_END
};

static const JSPropertySpec ReadableStreamDefaultReader_properties[] = {
    JS_PSG("closed", ReadableStreamDefaultReader_closed, 0),
    JS_PS_END
};

CLASS_SPEC(ReadableStreamDefaultReader, 1, ReaderSlotCount, 0, 0, JS_NULL_CLASS_OPS);


// Streams spec, 3.6.3 new ReadableStreamBYOBReader ( stream )
// Steps 2-5.
static MOZ_MUST_USE ReadableStreamBYOBReader*
CreateReadableStreamBYOBReader(JSContext* cx, Handle<ReadableStream*> stream)
{
    // Step 2: If ! IsReadableByteStreamController(stream.[[readableStreamController]])
    //         is false, throw a TypeError exception.
    if (!ControllerFromStream(stream)->is<ReadableByteStreamController>()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAM_NOT_BYTE_STREAM_CONTROLLER,
                                  "ReadableStream.getReader('byob')");
        return nullptr;
    }

    // Step 3: If ! IsReadableStreamLocked(stream) is true, throw a TypeError
    //         exception.
    if (stream->locked()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_READABLESTREAM_LOCKED);
        return nullptr;
    }

    Rooted<ReadableStreamBYOBReader*> reader(cx);
    reader = NewBuiltinClassInstance<ReadableStreamBYOBReader>(cx);
    if (!reader)
        return nullptr;

    // Step 4: Perform ! ReadableStreamReaderGenericInitialize(this, stream).
    if (!ReadableStreamReaderGenericInitialize(cx, reader, stream))
        return nullptr;

    // Step 5: Set this.[[readIntoRequests]] to a new empty List.
    if (!SetNewList(cx, reader, ReaderSlot_Requests))
        return nullptr;

    return reader;
}

// Streams spec, 3.6.3 new ReadableStreamBYOBReader ( stream )
bool
ReadableStreamBYOBReader::constructor(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!ThrowIfNotConstructing(cx, args, "ReadableStreamBYOBReader"))
        return false;

    // Step 1: If ! IsReadableStream(stream) is false, throw a TypeError exception.
    Rooted<ReadableStream*> stream(cx, UnwrapMaybeWrappedArg<ReadableStream>(args.get(0)));
    if (!stream) {
        ReportArgTypeError(cx, "ReadableStreamBYOBReader", "ReadableStream", args.get(0));
        return false;
    }

    RootedObject reader(cx, CreateReadableStreamBYOBReader(cx, stream));
    if (!reader)
        return false;

    args.rval().setObject(*reader);
    return true;
}

// Streams spec, 3.6.4.1 get closed
static MOZ_MUST_USE bool
ReadableStreamBYOBReader_closed(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Step 1: If ! IsReadableStreamBYOBReader(this) is false, return a promise
    //         rejected with a TypeError exception.
    if (!Is<ReadableStreamBYOBReader>(args.thisv()))
        return RejectNonGenericMethod(cx, args, "ReadableStreamBYOBReader", "get closed");

    // Step 2: Return this.[[closedPromise]].
    NativeObject* reader = &args.thisv().toObject().as<NativeObject>();
    args.rval().set(reader->getFixedSlot(ReaderSlot_ClosedPromise));
    return true;
}

// Streams spec, 3.6.4.2. cancel ( reason )
static MOZ_MUST_USE bool
ReadableStreamBYOBReader_cancel(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Step 1: If ! IsReadableStreamBYOBReader(this) is false, return a promise
    //         rejected with a TypeError exception.
    if (!Is<ReadableStreamBYOBReader>(args.thisv()))
        return RejectNonGenericMethod(cx, args, "ReadableStreamBYOBReader", "cancel");

    // Step 2: If this.[[ownerReadableStream]] is undefined, return a promise
    //         rejected with a TypeError exception.
    RootedNativeObject reader(cx, &args.thisv().toObject().as<NativeObject>());
    if (!ReaderHasStream(reader)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAMREADER_NOT_OWNED, "cancel");
        return ReturnPromiseRejectedWithPendingError(cx, args);
    }

    // Step 3: Return ! ReadableStreamReaderGenericCancel(this, reason).
    JSObject* cancelPromise = ReadableStreamReaderGenericCancel(cx, reader, args.get(0));
    if (!cancelPromise)
        return false;
    args.rval().setObject(*cancelPromise);
    return true;
}

// Streams spec, 3.6.4.3 read ( )
static MOZ_MUST_USE bool
ReadableStreamBYOBReader_read(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    HandleValue viewVal = args.get(0);

    // Step 1: If ! IsReadableStreamBYOBReader(this) is false, return a promise
    //         rejected with a TypeError exception.
    if (!Is<ReadableStreamBYOBReader>(args.thisv()))
        return RejectNonGenericMethod(cx, args, "ReadableStreamBYOBReader", "read");

    // Step 2: If this.[[ownerReadableStream]] is undefined, return a promise
    //         rejected with a TypeError exception.
    Rooted<ReadableStreamBYOBReader*> reader(cx);
    reader = &args.thisv().toObject().as<ReadableStreamBYOBReader>();
    if (!ReaderHasStream(reader)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAMREADER_NOT_OWNED, "read");
        return ReturnPromiseRejectedWithPendingError(cx, args);
    }

    // Step 3: If Type(view) is not Object, return a promise rejected with a
    //         TypeError exception.
    // Step 4: If view does not have a [[ViewedArrayBuffer]] internal slot,
    //         return a promise rejected with a TypeError exception.
    if (!Is<ArrayBufferViewObject>(viewVal)) {
        ReportArgTypeError(cx, "ReadableStreamBYOBReader.read", "Typed Array", viewVal);
        return ReturnPromiseRejectedWithPendingError(cx, args);
    }

    Rooted<ArrayBufferViewObject*> view(cx, &viewVal.toObject().as<ArrayBufferViewObject>());

    // Step 5: If view.[[ByteLength]] is 0, return a promise rejected with a
    //         TypeError exception.
    // Note: It's ok to use the length in number of elements here because all we
    // want to know is whether it's < 0.
    if (JS_GetArrayBufferViewByteLength(view) == 0) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAMBYOBREADER_READ_EMPTY_VIEW);
        return ReturnPromiseRejectedWithPendingError(cx, args);
    }

    // Step 6: Return ! ReadableStreamBYOBReaderRead(this, view).
    JSObject* readPromise = ReadableStreamBYOBReader::read(cx, reader, view);
    if (!readPromise)
        return false;
    args.rval().setObject(*readPromise);
    return true;
}

static MOZ_MUST_USE bool
ReadableStreamReaderGenericRelease(JSContext* cx, HandleNativeObject reader);

// Streams spec, 3.6.4.4. releaseLock ( )
static MOZ_MUST_USE bool
ReadableStreamBYOBReader_releaseLock_impl(JSContext* cx, const CallArgs& args)
{
    Rooted<ReadableStreamBYOBReader*> reader(cx);
    reader = &args.thisv().toObject().as<ReadableStreamBYOBReader>();

    // Step 2: If this.[[ownerReadableStream]] is undefined, return.
    if (!ReaderHasStream(reader)) {
        args.rval().setUndefined();
        return true;
    }

    // Step 3: If this.[[readRequests]] is not empty, throw a TypeError exception.
    Value val = reader->getFixedSlot(ReaderSlot_Requests);
    if (!val.isUndefined()) {
        NativeObject* readRequests = &val.toObject().as<NativeObject>();
        uint32_t len = readRequests->getDenseInitializedLength();
        if (len != 0) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                      JSMSG_READABLESTREAMREADER_NOT_EMPTY, "releaseLock");
            return false;
        }
    }

    // Step 4: Perform ! ReadableStreamReaderGenericRelease(this).
    return ReadableStreamReaderGenericRelease(cx, reader);
}

static bool
ReadableStreamBYOBReader_releaseLock(JSContext* cx, unsigned argc, Value* vp)
{
    // Step 1: If ! IsReadableStreamBYOBReader(this) is false,
    //         throw a TypeError exception.
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<Is<ReadableStreamBYOBReader>,
                                ReadableStreamBYOBReader_releaseLock_impl>(cx, args);
}

static const JSPropertySpec ReadableStreamBYOBReader_properties[] = {
    JS_PSG("closed", ReadableStreamBYOBReader_closed, 0),
    JS_PS_END
};

static const JSFunctionSpec ReadableStreamBYOBReader_methods[] = {
    JS_FN("cancel",         ReadableStreamBYOBReader_cancel,        1, 0),
    JS_FN("read",           ReadableStreamBYOBReader_read,          1, 0),
    JS_FN("releaseLock",    ReadableStreamBYOBReader_releaseLock,   0, 0),
    JS_FS_END
};

CLASS_SPEC(ReadableStreamBYOBReader, 1, 3, 0, 0, JS_NULL_CLASS_OPS);

inline static MOZ_MUST_USE bool
ReadableStreamControllerCallPullIfNeeded(JSContext* cx, HandleNativeObject controller);

// Streams spec, 3.7.1. IsReadableStreamDefaultReader ( x )
// Implemented via intrinsic_isInstanceOfBuiltin<ReadableStreamDefaultReader>()

// Streams spec, 3.7.2. IsReadableStreamBYOBReader ( x )
// Implemented via intrinsic_isInstanceOfBuiltin<ReadableStreamBYOBReader>()

// Streams spec, 3.7.3. ReadableStreamReaderGenericCancel ( reader, reason )
static MOZ_MUST_USE JSObject*
ReadableStreamReaderGenericCancel(JSContext* cx, HandleNativeObject reader, HandleValue reason)
{
    // Step 1: Let stream be reader.[[ownerReadableStream]].
    Rooted<ReadableStream*> stream(cx, StreamFromReader(reader));

    // Step 2: Assert: stream is not undefined (implicit).

    // Step 3: Return ! ReadableStreamCancel(stream, reason).
    RootedObject cancelPromise(cx);
    {
        JSAutoCompartment ac(cx, stream);
        RootedValue wrappedReason(cx, reason);
        if (!JS_WrapValue(cx, &wrappedReason))
            return nullptr;

        cancelPromise = ReadableStreamCancel(cx, stream, wrappedReason);
        if (!cancelPromise)
            return nullptr;
    }

    if (!JS_WrapObject(cx, &cancelPromise))
        return nullptr;
    return cancelPromise;
}

// Streams spec, 3.7.4. ReadableStreamReaderGenericInitialize ( reader, stream )
static MOZ_MUST_USE bool
ReadableStreamReaderGenericInitialize(JSContext* cx, HandleNativeObject reader,
                                      Handle<ReadableStream*> stream)
{
    // Step 1: Set reader.[[ownerReadableStream]] to stream.
    RootedValue streamVal(cx, ObjectValue(*stream));
    if (!JS_WrapValue(cx, &streamVal))
        return false;
    reader->setFixedSlot(ReaderSlot_Stream, streamVal);

    // Step 2: Set stream.[[reader]] to reader.
    RootedValue readerVal(cx, ObjectValue(*reader));
    {
        JSAutoCompartment ac(cx, stream);
        if (!JS_WrapValue(cx, &readerVal))
            return false;
        stream->setFixedSlot(StreamSlot_Reader, readerVal);
    }

    // Step 3: If stream.[[state]] is "readable",
    RootedObject promise(cx);
    if (stream->readable()) {
        // Step a: Set reader.[[closedPromise]] to a new promise.
        promise = PromiseObject::createSkippingExecutor(cx);
    } else if (stream->closed()) {
        // Step 4: Otherwise
        // Step a: If stream.[[state]] is "closed",
        // Step i: Set reader.[[closedPromise]] to a new promise resolved with
        //         undefined.
        promise = PromiseObject::unforgeableResolve(cx, UndefinedHandleValue);
    } else {
        // Step b: Otherwise,
        // Step i: Assert: stream.[[state]] is "errored".
        MOZ_ASSERT(stream->errored());

        // Step ii: Set reader.[[closedPromise]] to a new promise rejected with
        //          stream.[[storedError]].
        RootedValue storedError(cx, stream->getFixedSlot(StreamSlot_StoredError));
        if (!JS_WrapValue(cx, &storedError))
            return false;
        promise = PromiseObject::unforgeableReject(cx, storedError);
    }

    if (!promise)
        return false;

    reader->setFixedSlot(ReaderSlot_ClosedPromise, ObjectValue(*promise));
    return true;
}

// Streams spec, 3.7.5. ReadableStreamReaderGenericRelease ( reader )
static MOZ_MUST_USE bool
ReadableStreamReaderGenericRelease(JSContext* cx, HandleNativeObject reader)
{
    // Step 1: Assert: reader.[[ownerReadableStream]] is not undefined.
    Rooted<ReadableStream*> stream(cx, StreamFromReader(reader));

    // Step 2: Assert: reader.[[ownerReadableStream]].[[reader]] is reader.
    MOZ_ASSERT(ReaderFromStream(stream) == reader);

    // Create an exception to reject promises with below. We don't have a
    // clean way to do this, unfortunately.
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_READABLESTREAMREADER_RELEASED);
    RootedValue exn(cx);
    // Not much we can do about uncatchable exceptions, just bail.
    if (!GetAndClearException(cx, &exn))
        return false;

    // Step 3: If reader.[[ownerReadableStream]].[[state]] is "readable", reject
    //         reader.[[closedPromise]] with a TypeError exception.
    if (stream->readable()) {
            Value val = reader->getFixedSlot(ReaderSlot_ClosedPromise);
            Rooted<PromiseObject*> closedPromise(cx, &val.toObject().as<PromiseObject>());
            if (!PromiseObject::reject(cx, closedPromise, exn))
                return false;
    } else {
        // Step 4: Otherwise, set reader.[[closedPromise]] to a new promise rejected
        //         with a TypeError exception.
        RootedObject closedPromise(cx, PromiseObject::unforgeableReject(cx, exn));
        if (!closedPromise)
            return false;
        reader->setFixedSlot(ReaderSlot_ClosedPromise, ObjectValue(*closedPromise));
    }

    // Step 5: Set reader.[[ownerReadableStream]].[[reader]] to undefined.
    stream->setFixedSlot(StreamSlot_Reader, UndefinedValue());

    // Step 6: Set reader.[[ownerReadableStream]] to undefined.
    reader->setFixedSlot(ReaderSlot_Stream, UndefinedValue());

    return true;
}

static MOZ_MUST_USE JSObject*
ReadableByteStreamControllerPullInto(JSContext* cx,
                                     Handle<ReadableByteStreamController*> controller,
                                     Handle<ArrayBufferViewObject*> view);

// Streams spec, 3.7.6. ReadableStreamBYOBReaderRead ( reader, view )
/* static */ MOZ_MUST_USE JSObject*
ReadableStreamBYOBReader::read(JSContext* cx, Handle<ReadableStreamBYOBReader*> reader,
                               Handle<ArrayBufferViewObject*> view)
{
    MOZ_ASSERT(reader->is<ReadableStreamBYOBReader>());

    // Step 1: Let stream be reader.[[ownerReadableStream]].
    // Step 2: Assert: stream is not undefined.
    Rooted<ReadableStream*> stream(cx, StreamFromReader(reader));

    // Step 3: Set stream.[[disturbed]] to true.
    SetStreamState(stream, StreamState(stream) | ReadableStream::Disturbed);

    // Step 4: If stream.[[state]] is "errored", return a promise rejected with
    //         stream.[[storedError]].
    if (stream->errored()) {
        RootedValue storedError(cx, stream->getFixedSlot(StreamSlot_StoredError));
        if (!JS_WrapValue(cx, &storedError))
            return nullptr;
        return PromiseObject::unforgeableReject(cx, storedError);
    }

    // Step 5: Return ! ReadableByteStreamControllerPullInto(stream.[[readableStreamController]], view).
    Rooted<ReadableByteStreamController*> controller(cx);
    controller = &ControllerFromStream(stream)->as<ReadableByteStreamController>();
    return ReadableByteStreamControllerPullInto(cx, controller, view);
}

static MOZ_MUST_USE JSObject*
ReadableStreamControllerPullSteps(JSContext* cx, HandleNativeObject controller);

// Streams spec, 3.7.7. ReadableStreamDefaultReaderRead ( reader )
MOZ_MUST_USE JSObject*
ReadableStreamDefaultReader::read(JSContext* cx, Handle<ReadableStreamDefaultReader*> reader)
{
    // Step 1: Let stream be reader.[[ownerReadableStream]].
    // Step 2: Assert: stream is not undefined.
    Rooted<ReadableStream*> stream(cx, StreamFromReader(reader));

    // Step 3: Set stream.[[disturbed]] to true.
    SetStreamState(stream, StreamState(stream) | ReadableStream::Disturbed);

    // Step 4: If stream.[[state]] is "closed", return a new promise resolved with
    //         ! CreateIterResultObject(undefined, true).
    if (stream->closed()) {
        RootedObject iterResult(cx, CreateIterResultObject(cx, UndefinedHandleValue, true));
        if (!iterResult)
            return nullptr;
        RootedValue iterResultVal(cx, ObjectValue(*iterResult));
        return PromiseObject::unforgeableResolve(cx, iterResultVal);
    }

    // Step 5: If stream.[[state]] is "errored", return a new promise rejected with
    //         stream.[[storedError]].
    if (stream->errored()) {
        RootedValue storedError(cx, stream->getFixedSlot(StreamSlot_StoredError));
        return PromiseObject::unforgeableReject(cx, storedError);
    }

    // Step 6: Assert: stream.[[state]] is "readable".
    MOZ_ASSERT(stream->readable());

    // Step 7: Return ! stream.[[readableStreamController]].[[PullSteps]]().
    RootedNativeObject controller(cx, ControllerFromStream(stream));
    return ReadableStreamControllerPullSteps(cx, controller);
}

// Streams spec, 3.8.3, step 11.a.
// and
// Streams spec, 3.10.3, step 16.a.
static bool
ControllerStartHandler(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedNativeObject controller(cx, TargetFromHandler<NativeObject>(args.callee()));

    // Step i: Set controller.[[started]] to true.
    AddControllerFlags(controller, ControllerFlag_Started);

    // Step ii: Assert: controller.[[pulling]] is false.
    // Step iii: Assert: controller.[[pullAgain]] is false.
    MOZ_ASSERT(!(ControllerFlags(controller) &
                 (ControllerFlag_Pulling | ControllerFlag_PullAgain)));

    // Step iv: Perform ! ReadableStreamDefaultControllerCallPullIfNeeded(controller).
    // or
    // Step iv: Perform ! ReadableByteStreamControllerCallPullIfNeeded((controller).
    if (!ReadableStreamControllerCallPullIfNeeded(cx, controller))
        return false;
    args.rval().setUndefined();
    return true;
}

static MOZ_MUST_USE bool
ReadableStreamDefaultControllerErrorIfNeeded(JSContext* cx,
                                             Handle<ReadableStreamDefaultController*> controller,
                                             HandleValue e);

static MOZ_MUST_USE bool
ReadableStreamControllerError(JSContext* cx, HandleNativeObject controller, HandleValue e);

// Streams spec, 3.8.3, step 11.b.
// and
// Streams spec, 3.10.3, step 16.b.
static bool
ControllerStartFailedHandler(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedNativeObject controllerObj(cx, TargetFromHandler<NativeObject>(args.callee()));

    // 3.8.3, Step 11.b.i:
    // Perform ! ReadableStreamDefaultControllerErrorIfNeeded(controller, r).
    if (controllerObj->is<ReadableStreamDefaultController>()) {
        Rooted<ReadableStreamDefaultController*> controller(cx);
        controller = &controllerObj->as<ReadableStreamDefaultController>();
        return ReadableStreamDefaultControllerErrorIfNeeded(cx, controller, args.get(0));
    }

    // 3.10.3, Step 16.b.i: If stream.[[state]] is "readable", perform
    //                      ! ReadableByteStreamControllerError(controller, r).
    if (StreamFromController(controllerObj)->readable())
        return ReadableStreamControllerError(cx, controllerObj, args.get(0));

    args.rval().setUndefined();
    return true;
}

static MOZ_MUST_USE bool
ValidateAndNormalizeHighWaterMark(JSContext* cx,
                                  HandleValue highWaterMarkVal,
                                  double* highWaterMark);

static MOZ_MUST_USE bool
ValidateAndNormalizeQueuingStrategy(JSContext* cx,
                                    HandleValue size,
                                    HandleValue highWaterMarkVal,
                                    double* highWaterMark);

// Streams spec, 3.8.3 new ReadableStreamDefaultController ( stream, underlyingSource,
//                                                           size, highWaterMark )
// Steps 3 - 11.
static MOZ_MUST_USE ReadableStreamDefaultController*
CreateReadableStreamDefaultController(JSContext* cx, Handle<ReadableStream*> stream,
                                      HandleValue underlyingSource, HandleValue size,
                                      HandleValue highWaterMarkVal)
{
    Rooted<ReadableStreamDefaultController*> controller(cx);
    controller = NewBuiltinClassInstance<ReadableStreamDefaultController>(cx);
    if (!controller)
        return nullptr;

    // Step 3: Set this.[[controlledReadableStream]] to stream.
    controller->setFixedSlot(ControllerSlot_Stream, ObjectValue(*stream));

    // Step 4: Set this.[[underlyingSource]] to underlyingSource.
    controller->setFixedSlot(ControllerSlot_UnderlyingSource, underlyingSource);

    // Step 5: Perform ! ResetQueue(this).
    if (!ResetQueue(cx, controller))
        return nullptr;

    // Step 6: Set this.[[started]], this.[[closeRequested]], this.[[pullAgain]],
    //         and this.[[pulling]] to false.
    controller->setFixedSlot(ControllerSlot_Flags, Int32Value(0));

    // Step 7: Let normalizedStrategy be
    //         ? ValidateAndNormalizeQueuingStrategy(size, highWaterMark).
    double highWaterMark;
    if (!ValidateAndNormalizeQueuingStrategy(cx, size, highWaterMarkVal, &highWaterMark))
        return nullptr;

    // Step 8: Set this.[[strategySize]] to normalizedStrategy.[[size]] and
    //         this.[[strategyHWM]] to normalizedStrategy.[[highWaterMark]].
    controller->setFixedSlot(DefaultControllerSlot_StrategySize, size);
    controller->setFixedSlot(ControllerSlot_StrategyHWM, NumberValue(highWaterMark));

    // Step 9: Let controller be this (implicit).

    // Step 10: Let startResult be
    //          ? InvokeOrNoop(underlyingSource, "start", « this »).
    RootedValue startResult(cx);
    RootedValue controllerVal(cx, ObjectValue(*controller));
    if (!InvokeOrNoop(cx, underlyingSource, cx->names().start, controllerVal, &startResult))
        return nullptr;

    // Step 11: Let startPromise be a promise resolved with startResult:
    RootedObject startPromise(cx, PromiseObject::unforgeableResolve(cx, startResult));
    if (!startPromise)
        return nullptr;

    RootedObject onStartFulfilled(cx, NewHandler(cx, ControllerStartHandler, controller));
    if (!onStartFulfilled)
        return nullptr;

    RootedObject onStartRejected(cx, NewHandler(cx, ControllerStartFailedHandler, controller));
    if (!onStartRejected)
        return nullptr;

    if (!JS::AddPromiseReactions(cx, startPromise, onStartFulfilled, onStartRejected))
        return nullptr;

    return controller;
}

// Streams spec, 3.8.3.
// new ReadableStreamDefaultController( stream, underlyingSource, size,
//                                      highWaterMark )
bool
ReadableStreamDefaultController::constructor(JSContext* cx, unsigned argc, Value* vp)
{
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_NO_CONSTRUCTOR,
                              "ReadableStreamDefaultController");
    return false;
}

[[nodiscard]] static double
ReadableStreamControllerGetDesiredSizeUnchecked(NativeObject* controller);

// Streams spec, 3.8.4.1. get desiredSize
// and
// Streams spec, 3.10.4.2. get desiredSize
static MOZ_MUST_USE bool
ReadableStreamController_desiredSize_impl(JSContext* cx, const CallArgs& args)
{
    RootedNativeObject controller(cx);
    controller = UnwrapMaybeWrappedReadableStreamController(args.thisv());
    MOZ_ASSERT(controller);

    // Streams spec, 3.9.8. steps 1-4.
    // 3.9.8. Step 1: Let stream be controller.[[controlledReadableStream]].
    ReadableStream* stream = StreamFromController(controller);

    // 3.9.8. Step 2: Let state be stream.[[state]].
    // 3.9.8. Step 3: If state is "errored", return null.
    if (stream->errored()) {
        args.rval().setNull();
        return true;
    }

    // 3.9.8. Step 4: If state is "closed", return 0.
    if (stream->closed()) {
        args.rval().setInt32(0);
        return true;
    }

    // Step 2: Return ! ReadableStreamDefaultControllerGetDesiredSize(this).
    args.rval().setNumber(ReadableStreamControllerGetDesiredSizeUnchecked(controller));
    return true;
}

static bool
ReadableStreamDefaultController_desiredSize(JSContext* cx, unsigned argc, Value* vp)
{
    // Step 1: If ! IsReadableStreamDefaultController(this) is false, throw a
    //         TypeError exception.
    CallArgs args = CallArgsFromVp(argc, vp);
    if (!IsMaybeWrapped<ReadableStreamDefaultController>(args.thisv())) {
        ReportValueError3(cx, JSMSG_INCOMPATIBLE_PROTO, JSDVG_SEARCH_STACK, args.thisv(),
                          nullptr, "ReadableStreamDefaultController", "get desiredSize");
        return false;
    }
    return ReadableStreamController_desiredSize_impl(cx, args);
}

static MOZ_MUST_USE bool
ReadableStreamDefaultControllerClose(JSContext* cx,
                                     Handle<ReadableStreamDefaultController*> controller);

// Unified implementation of steps 2-3 of 3.8.4.2 and 3.10.4.3.
static MOZ_MUST_USE bool
VerifyControllerStateForClosing(JSContext* cx, HandleNativeObject controller)
{
    // Step 2: If this.[[closeRequested]] is true, throw a TypeError exception.
    if (ControllerFlags(controller) & ControllerFlag_CloseRequested) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAMCONTROLLER_CLOSED, "close");
        return false;
    }

    // Step 3: If this.[[controlledReadableStream]].[[state]] is not "readable",
    //         throw a TypeError exception.
    ReadableStream* stream = StreamFromController(controller);
    if (!stream->readable()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAMCONTROLLER_NOT_READABLE, "close");
        return false;
    }

    return true;
}

// Streams spec, 3.8.4.2 close()
static MOZ_MUST_USE bool
ReadableStreamDefaultController_close_impl(JSContext* cx, const CallArgs& args)
{
    Rooted<ReadableStreamDefaultController*> controller(cx);
    controller = UnwrapMaybeWrappedArg<ReadableStreamDefaultController>(args.thisv());
    MOZ_ASSERT(controller);

    // Steps 2-3.
    if (!VerifyControllerStateForClosing(cx, controller))
        return false;

    // Step 4: Perform ! ReadableStreamDefaultControllerClose(this).
    if (!ReadableStreamDefaultControllerClose(cx, controller))
        return false;
    args.rval().setUndefined();
    return true;
}

static bool
ReadableStreamDefaultController_close(JSContext* cx, unsigned argc, Value* vp)
{
    // Step 1: If ! IsReadableStreamDefaultController(this) is false, throw a
    //         TypeError exception.

    CallArgs args = CallArgsFromVp(argc, vp);
    if (!IsMaybeWrapped<ReadableStreamDefaultController>(args.thisv())) {
        ReportValueError3(cx, JSMSG_INCOMPATIBLE_PROTO, JSDVG_SEARCH_STACK, args.thisv(),
                          nullptr, "ReadableStreamDefaultController", "close");
        return false;
    }
    return ReadableStreamDefaultController_close_impl(cx, args);
}

static MOZ_MUST_USE bool
ReadableStreamDefaultControllerEnqueue(JSContext* cx,
                                       Handle<ReadableStreamDefaultController*> controller,
                                       HandleValue chunk);

// Streams spec, 3.8.4.3. enqueue ( chunk )
static MOZ_MUST_USE bool
ReadableStreamDefaultController_enqueue_impl(JSContext* cx, const CallArgs& args)
{
    Rooted<ReadableStreamDefaultController*> controller(cx);
    controller = UnwrapMaybeWrappedArg<ReadableStreamDefaultController>(args.thisv());
    MOZ_ASSERT(controller);

    // Step 2: If this.[[closeRequested]] is true, throw a TypeError exception.
    if (ControllerFlags(controller) & ControllerFlag_CloseRequested) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAMCONTROLLER_CLOSED, "close");
        return false;
    }

    // Step 3: If this.[[controlledReadableStream]].[[state]] is not "readable",
    //         throw a TypeError exception.
    ReadableStream* stream = StreamFromController(controller);
    if (!stream->readable()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAMCONTROLLER_NOT_READABLE, "close");
        return false;
    }

    // Step 4: Return ! ReadableStreamDefaultControllerEnqueue(this, chunk).
    if (!ReadableStreamDefaultControllerEnqueue(cx, controller, args.get(0)))
        return false;
    args.rval().setUndefined();
    return true;
}

static bool
ReadableStreamDefaultController_enqueue(JSContext* cx, unsigned argc, Value* vp)
{
    // Step 1: If ! IsReadableStreamDefaultController(this) is false, throw a
    //         TypeError exception.

    CallArgs args = CallArgsFromVp(argc, vp);
    if (!IsMaybeWrapped<ReadableStreamDefaultController>(args.thisv())) {
        ReportValueError3(cx, JSMSG_INCOMPATIBLE_PROTO, JSDVG_SEARCH_STACK, args.thisv(),
                          nullptr, "ReadableStreamDefaultController", "enqueue");
        return false;
    }
    return ReadableStreamDefaultController_enqueue_impl(cx, args);
}

// Streams spec, 3.8.4.4. error ( e )
static MOZ_MUST_USE bool
ReadableStreamDefaultController_error_impl(JSContext* cx, const CallArgs& args)
{
    Rooted<ReadableStreamDefaultController*> controller(cx);
    controller = UnwrapMaybeWrappedArg<ReadableStreamDefaultController>(args.thisv());
    MOZ_ASSERT(controller);

    // Step 2: Let stream be this.[[controlledReadableStream]].
    // Step 3: If stream.[[state]] is not "readable", throw a TypeError exception.
    if (!StreamFromController(controller)->readable()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAMCONTROLLER_NOT_READABLE, "error");
        return false;
    }

    // Step 4: Perform ! ReadableStreamDefaultControllerError(this, e).
    if (!ReadableStreamControllerError(cx, controller, args.get(0)))
        return false;
    args.rval().setUndefined();
    return true;
}

static bool
ReadableStreamDefaultController_error(JSContext* cx, unsigned argc, Value* vp)
{
    // Step 1: If ! IsReadableStreamDefaultController(this) is false, throw a
    //         TypeError exception.

    CallArgs args = CallArgsFromVp(argc, vp);
    if (!IsMaybeWrapped<ReadableStreamDefaultController>(args.thisv())) {
        ReportValueError3(cx, JSMSG_INCOMPATIBLE_PROTO, JSDVG_SEARCH_STACK, args.thisv(),
                          nullptr, "ReadableStreamDefaultController", "error");
        return false;
    }
    return ReadableStreamDefaultController_error_impl(cx, args);
}

static const JSPropertySpec ReadableStreamDefaultController_properties[] = {
    JS_PSG("desiredSize", ReadableStreamDefaultController_desiredSize, 0),
    JS_PS_END
};

static const JSFunctionSpec ReadableStreamDefaultController_methods[] = {
    JS_FN("close",      ReadableStreamDefaultController_close,      0, 0),
    JS_FN("enqueue",    ReadableStreamDefaultController_enqueue,    1, 0),
    JS_FN("error",      ReadableStreamDefaultController_error,      1, 0),
    JS_FS_END
};

CLASS_SPEC(ReadableStreamDefaultController, 4, 7, 0, 0, JS_NULL_CLASS_OPS);

/**
 * Unified implementation of ReadableStream controllers' [[CancelSteps]] internal
 * methods.
 * Streams spec, 3.8.5.1. [[CancelSteps]] ( reason )
 * and
 * Streams spec, 3.10.5.1. [[CancelSteps]] ( reason )
 */
static MOZ_MUST_USE JSObject*
ReadableStreamControllerCancelSteps(JSContext* cx, HandleNativeObject controller,
                                    HandleValue reason)
{
    MOZ_ASSERT(IsReadableStreamController(controller));

    // Step 1 of 3.10.5.1: If this.[[pendingPullIntos]] is not empty,
    if (!controller->is<ReadableStreamDefaultController>()) {
        Value val = controller->getFixedSlot(ByteControllerSlot_PendingPullIntos);
        RootedNativeObject pendingPullIntos(cx, &val.toObject().as<NativeObject>());

        if (pendingPullIntos->getDenseInitializedLength() != 0) {
            // Step a: Let firstDescriptor be the first element of
            //         this.[[pendingPullIntos]].
            // Step b: Set firstDescriptor.[[bytesFilled]] to 0.
            Rooted<PullIntoDescriptor*> firstDescriptor(cx);
            firstDescriptor = PeekList<PullIntoDescriptor>(pendingPullIntos);
            firstDescriptor->setBytesFilled(0);
        }
    }

    // Step 1 of 3.8.5.1, step 2 of 3.10.5.1: Perform ! ResetQueue(this).
    if (!ResetQueue(cx, controller))
        return nullptr;

    // Step 2 of 3.8.5.1, step 3 of 3.10.5.1:
    // Return ! PromiseInvokeOrNoop(this.[[underlying(Byte)Source]],
    //                              "cancel", « reason »)
    RootedValue underlyingSource(cx);
    underlyingSource = controller->getFixedSlot(ControllerSlot_UnderlyingSource);

    if (Is<TeeState>(underlyingSource)) {
        Rooted<TeeState*> teeState(cx, &underlyingSource.toObject().as<TeeState>());
        Rooted<ReadableStreamDefaultController*> defaultController(cx);
        defaultController = &controller->as<ReadableStreamDefaultController>();
        return ReadableStreamTee_Cancel(cx, teeState, defaultController, reason);
    }

    if (ControllerFlags(controller) & ControllerFlag_ExternalSource) {
        void* source = underlyingSource.toPrivate();
        Rooted<ReadableStream*> stream(cx, StreamFromController(controller));
        RootedValue rval(cx);
        rval = cx->runtime()->readableStreamCancelCallback(cx, stream, source,
                                                           stream->embeddingFlags(), reason);
        return PromiseObject::unforgeableResolve(cx, rval);
    }

    return PromiseInvokeOrNoop(cx, underlyingSource, cx->names().cancel, reason);
}

inline static MOZ_MUST_USE bool
DequeueValue(JSContext* cx, HandleNativeObject container, MutableHandleValue chunk);

// Streams spec, 3.8.5.2. ReadableStreamDefaultController [[PullSteps]]()
static JSObject*
ReadableStreamDefaultControllerPullSteps(JSContext* cx, HandleNativeObject controller)
{
    MOZ_ASSERT(controller->is<ReadableStreamDefaultController>());

    // Step 1: Let stream be this.[[controlledReadableStream]].
    Rooted<ReadableStream*> stream(cx, StreamFromController(controller));

    // Step 2: If this.[[queue]] is not empty,
    RootedNativeObject queue(cx);
    RootedValue val(cx, controller->getFixedSlot(QueueContainerSlot_Queue));
    if (val.isObject())
        queue = &val.toObject().as<NativeObject>();

    if (queue && queue->getDenseInitializedLength() != 0) {
        // Step a: Let chunk be ! DequeueValue(this.[[queue]]).
        RootedValue chunk(cx);
        if (!DequeueValue(cx, controller, &chunk))
            return nullptr;

        // Step b: If this.[[closeRequested]] is true and this.[[queue]] is empty,
        //         perform ! ReadableStreamClose(stream).
        bool closeRequested = ControllerFlags(controller) & ControllerFlag_CloseRequested;
        if (closeRequested && queue->getDenseInitializedLength() == 0) {
            if (!ReadableStreamCloseInternal(cx, stream))
                return nullptr;
        }

        // Step c: Otherwise, perform ! ReadableStreamDefaultControllerCallPullIfNeeded(this).
        else {
        if (!ReadableStreamControllerCallPullIfNeeded(cx, controller))
            return nullptr;
        }

        // Step d: Return a promise resolved with ! CreateIterResultObject(chunk, false).
        RootedObject iterResultObj(cx, CreateIterResultObject(cx, chunk, false));
        if (!iterResultObj)
          return nullptr;
        RootedValue iterResult(cx, ObjectValue(*iterResultObj));
        return PromiseObject::unforgeableResolve(cx, iterResult);
    }

    // Step 3: Let pendingPromise be ! ReadableStreamAddReadRequest(stream).
    Rooted<PromiseObject*> pendingPromise(cx, ReadableStreamAddReadRequest(cx, stream));
    if (!pendingPromise)
        return nullptr;

    // Step 4: Perform ! ReadableStreamDefaultControllerCallPullIfNeeded(this).
    if (!ReadableStreamControllerCallPullIfNeeded(cx, controller))
        return nullptr;

    // Step 5: Return pendingPromise.
    return pendingPromise;
}

// Streams spec, 3.9.2 and 3.12.3. step 7:
// Upon fulfillment of pullPromise,
static bool
ControllerPullHandler(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedNativeObject controller(cx, TargetFromHandler<NativeObject>(args.callee()));
    uint32_t flags = ControllerFlags(controller);

    // Step a: Set controller.[[pulling]] to false.
    // Step b.i: Set controller.[[pullAgain]] to false.
    RemoveControllerFlags(controller, ControllerFlag_Pulling | ControllerFlag_PullAgain);

    // Step b: If controller.[[pullAgain]] is true,
    if (flags & ControllerFlag_PullAgain) {
        // Step ii: Perform ! ReadableByteStreamControllerCallPullIfNeeded(controller).
        if (!ReadableStreamControllerCallPullIfNeeded(cx, controller))
            return false;
    }

    args.rval().setUndefined();
    return true;
}

// Streams spec, 3.9.2 and 3.12.3. step 8:
// Upon rejection of pullPromise with reason e,
static bool
ControllerPullFailedHandler(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedNativeObject controller(cx, TargetFromHandler<NativeObject>(args.callee()));
    HandleValue e = args.get(0);

    // Step a: If controller.[[controlledReadableStream]].[[state]] is "readable",
    //         perform ! ReadableByteStreamControllerError(controller, e).
    if (StreamFromController(controller)->readable()) {
        if (!ReadableStreamControllerError(cx, controller, e))
            return false;
    }

    args.rval().setUndefined();
    return true;
}

static bool
ReadableStreamControllerShouldCallPull(NativeObject* controller);

static MOZ_MUST_USE double
ReadableStreamControllerGetDesiredSizeUnchecked(NativeObject* controller);

// Streams spec, 3.9.2 ReadableStreamDefaultControllerCallPullIfNeeded ( controller )
// and
// Streams spec, 3.12.3. ReadableByteStreamControllerCallPullIfNeeded ( controller )
inline static MOZ_MUST_USE bool
ReadableStreamControllerCallPullIfNeeded(JSContext* cx, HandleNativeObject controller)
{
    // Step 1: Let shouldPull be
    //         ! ReadableByteStreamControllerShouldCallPull(controller).
    bool shouldPull = ReadableStreamControllerShouldCallPull(controller);

    // Step 2: If shouldPull is false, return.
    if (!shouldPull)
        return true;

    // Step 3: If controller.[[pulling]] is true,
    if (ControllerFlags(controller) & ControllerFlag_Pulling) {
        // Step a: Set controller.[[pullAgain]] to true.
        AddControllerFlags(controller, ControllerFlag_PullAgain);

        // Step b: Return.
        return true;
    }

    // Step 4: Assert: controller.[[pullAgain]] is false.
    MOZ_ASSERT(!(ControllerFlags(controller) & ControllerFlag_PullAgain));

    // Step 5: Set controller.[[pulling]] to true.
    AddControllerFlags(controller, ControllerFlag_Pulling);

    // Step 6: Let pullPromise be
    //         ! PromiseInvokeOrNoop(controller.[[underlyingByteSource]], "pull", controller).
    RootedObject pullPromise(cx);
    RootedValue underlyingSource(cx);
    underlyingSource = controller->getFixedSlot(ControllerSlot_UnderlyingSource);
    RootedValue controllerVal(cx, ObjectValue(*controller));

    if (Is<TeeState>(underlyingSource)) {
        Rooted<TeeState*> teeState(cx, &underlyingSource.toObject().as<TeeState>());
        Rooted<ReadableStream*> stream(cx, StreamFromController(controller));
        pullPromise = ReadableStreamTee_Pull(cx, teeState, stream);
    } else if (ControllerFlags(controller) & ControllerFlag_ExternalSource) {
        void* source = underlyingSource.toPrivate();
        Rooted<ReadableStream*> stream(cx, StreamFromController(controller));
        double desiredSize = ReadableStreamControllerGetDesiredSizeUnchecked(controller);
        cx->runtime()->readableStreamDataRequestCallback(cx, stream, source,
                                                         stream->embeddingFlags(), desiredSize);
        pullPromise = PromiseObject::unforgeableResolve(cx, UndefinedHandleValue);
    } else {
        pullPromise = PromiseInvokeOrNoop(cx, underlyingSource, cx->names().pull, controllerVal);
    }
    if (!pullPromise || !pullPromise->is<PromiseObject>())
        return false;

    RootedObject onPullFulfilled(cx, NewHandler(cx, ControllerPullHandler, controller));
    if (!onPullFulfilled)
        return false;

    RootedObject onPullRejected(cx, NewHandler(cx, ControllerPullFailedHandler, controller));
    if (!onPullRejected)
        return false;

    return JS::AddPromiseReactions(cx, pullPromise, onPullFulfilled, onPullRejected);

    // Steps 7-8 implemented in functions above.
}

// Streams spec, 3.9.3. ReadableStreamDefaultControllerShouldCallPull ( controller )
// and
// Streams spec, 3.12.24. ReadableByteStreamControllerShouldCallPull ( controller )
static bool
ReadableStreamControllerShouldCallPull(NativeObject* controller)
{
    // Step 1: Let stream be controller.[[controlledReadableStream]].
    ReadableStream* stream = StreamFromController(controller);

    // Step 2: If stream.[[state]] is "closed" or stream.[[state]] is "errored",
    //         return false.
    // or, equivalently
    // Step 2: If stream.[[state]] is not "readable", return false.
    if (!stream->readable())
        return false;

    // Step 3: If controller.[[closeRequested]] is true, return false.
    uint32_t flags = ControllerFlags(controller);
    if (flags & ControllerFlag_CloseRequested)
        return false;

    // Step 4: If controller.[[started]] is false, return false.
    if (!(flags & ControllerFlag_Started))
        return false;

    // Step 5: If ! IsReadableStreamLocked(stream) is true and
    //         ! ReadableStreamGetNumReadRequests(stream) > 0, return true.
    // Steps 5-6 of 3.12.24 are equivalent in our implementation.
    if (stream->locked() && ReadableStreamGetNumReadRequests(stream) > 0)
        return true;

    // Step 6: Let desiredSize be ReadableStreamDefaultControllerGetDesiredSize(controller).
    double desiredSize = ReadableStreamControllerGetDesiredSizeUnchecked(controller);

    // Step 7: If desiredSize > 0, return true.
    // Step 8: Return false.
    // Steps 7-8 of 3.12.24 are equivalent in our implementation.
    return desiredSize > 0;
}

// Streams spec, 3.9.4. ReadableStreamDefaultControllerClose ( controller )
static MOZ_MUST_USE bool
ReadableStreamDefaultControllerClose(JSContext* cx,
                                     Handle<ReadableStreamDefaultController*> controller)
{
    // Step 1: Let stream be controller.[[controlledReadableStream]].
    Rooted<ReadableStream*> stream(cx, StreamFromController(controller));

    // Step 2: Assert: controller.[[closeRequested]] is false.
    MOZ_ASSERT(!(ControllerFlags(controller) & ControllerFlag_CloseRequested));

    // Step 3: Assert: stream.[[state]] is "readable".
    MOZ_ASSERT(stream->readable());

    // Step 4: Set controller.[[closeRequested]] to true.
    AddControllerFlags(controller, ControllerFlag_CloseRequested);

    // Step 5: If controller.[[queue]] is empty, perform ! ReadableStreamClose(stream).
    RootedNativeObject queue(cx);
    queue = &controller->getFixedSlot(QueueContainerSlot_Queue).toObject().as<NativeObject>();
    if (queue->getDenseInitializedLength() == 0)
        return ReadableStreamCloseInternal(cx, stream);

    return true;
}

static MOZ_MUST_USE bool
EnqueueValueWithSize(JSContext* cx, HandleNativeObject container, HandleValue value,
                     HandleValue sizeVal);

// Streams spec, 3.9.5. ReadableStreamDefaultControllerEnqueue ( controller, chunk )
static MOZ_MUST_USE bool
ReadableStreamDefaultControllerEnqueue(JSContext* cx,
                                       Handle<ReadableStreamDefaultController*> controller,
                                       HandleValue chunk)
{
    // Step 1: Let stream be controller.[[controlledReadableStream]].
    Rooted<ReadableStream*> stream(cx, StreamFromController(controller));

    // Step 2: Assert: controller.[[closeRequested]] is false.
    MOZ_ASSERT(!(ControllerFlags(controller) & ControllerFlag_CloseRequested));

    // Step 3: Assert: stream.[[state]] is "readable".
    MOZ_ASSERT(stream->readable());

    // Step 4: If ! IsReadableStreamLocked(stream) is true and
    //         ! ReadableStreamGetNumReadRequests(stream) > 0, perform
    //         ! ReadableStreamFulfillReadRequest(stream, chunk, false).
    if (stream->locked() && ReadableStreamGetNumReadRequests(stream) > 0) {
        if (!ReadableStreamFulfillReadOrReadIntoRequest(cx, stream, chunk, false))
            return false;
    } else {
        // Step 5: Otherwise,
        // Step a: Let chunkSize be 1.
        RootedValue chunkSize(cx, NumberValue(1));
        bool success = true;

        // Step b: If controller.[[strategySize]] is not undefined,
        RootedValue strategySize(cx);
        strategySize = controller->getFixedSlot(DefaultControllerSlot_StrategySize);
        if (!strategySize.isUndefined()) {
            // Step i: Set chunkSize to Call(stream.[[strategySize]], undefined, chunk).
            success = Call(cx, strategySize, UndefinedHandleValue, chunk, &chunkSize);
        }

        // Step c: Let enqueueResult be
        //         EnqueueValueWithSize(controller, chunk, chunkSize).
        if (success)
            success = EnqueueValueWithSize(cx, controller, chunk, chunkSize);

        if (!success) {
            // Step b.ii: If chunkSize is an abrupt completion,
            // and
            // Step d: If enqueueResult is an abrupt completion,
            RootedValue exn(cx);
            if (!cx->getPendingException(&exn))
                return false;

            // Step b.ii.1: Perform
            //         ! ReadableStreamDefaultControllerErrorIfNeeded(controller,
            //                                                        chunkSize.[[Value]]).
            if (!ReadableStreamDefaultControllerErrorIfNeeded(cx, controller, exn))
                return false;

            // Step b.ii.2: Return chunkSize.
            return false;
        }
    }

    // Step 6: Perform ! ReadableStreamDefaultControllerCallPullIfNeeded(controller).
    if (!ReadableStreamControllerCallPullIfNeeded(cx, controller))
        return false;

    // Step 7: Return.
    return true;
}

static MOZ_MUST_USE bool
ReadableByteStreamControllerClearPendingPullIntos(JSContext* cx, HandleNativeObject controller);

// Streams spec, 3.9.6. ReadableStreamDefaultControllerError ( controller, e )
// and
// Streams spec, 3.12.10. ReadableByteStreamControllerError ( controller, e )
static MOZ_MUST_USE bool
ReadableStreamControllerError(JSContext* cx, HandleNativeObject controller, HandleValue e)
{
    MOZ_ASSERT(IsReadableStreamController(controller));

    // Step 1: Let stream be controller.[[controlledReadableStream]].
    Rooted<ReadableStream*> stream(cx, StreamFromController(controller));

    // Step 2: Assert: stream.[[state]] is "readable".
    MOZ_ASSERT(stream->readable());

    // Step 3 of 3.12.10:
    // Perform ! ReadableByteStreamControllerClearPendingPullIntos(controller).
    if (controller->is<ReadableByteStreamController>()) {
        Rooted<ReadableByteStreamController*> byteStreamController(cx);
        byteStreamController = &controller->as<ReadableByteStreamController>();
        if (!ReadableByteStreamControllerClearPendingPullIntos(cx, byteStreamController))
            return false;
    }

    // Step 3 (or 4): Perform ! ResetQueue(controller).
    if (!ResetQueue(cx, controller))
        return false;

    // Step 4 (or 5): Perform ! ReadableStreamError(stream, e).
    return ReadableStreamErrorInternal(cx, stream, e);
}

// Streams spec, 3.9.7. ReadableStreamDefaultControllerErrorIfNeeded ( controller, e ) nothrow
static MOZ_MUST_USE bool
ReadableStreamDefaultControllerErrorIfNeeded(JSContext* cx,
                                             Handle<ReadableStreamDefaultController*> controller,
                                             HandleValue e)
{
    // Step 1: If controller.[[controlledReadableStream]].[[state]] is "readable",
    //         perform ! ReadableStreamDefaultControllerError(controller, e).
    Rooted<ReadableStream*> stream(cx, StreamFromController(controller));
    if (stream->readable())
        return ReadableStreamControllerError(cx, controller, e);
    return true;
}

// Streams spec, 3.9.8. ReadableStreamDefaultControllerGetDesiredSize ( controller )
// and
// Streams spec 3.12.13. ReadableByteStreamControllerGetDesiredSize ( controller )
static MOZ_MUST_USE double
ReadableStreamControllerGetDesiredSizeUnchecked(NativeObject* controller)
{
    // Steps 1-4 done at callsites, so only assert that they have been done.
#if DEBUG
    ReadableStream* stream = StreamFromController(controller);
    MOZ_ASSERT(!(stream->errored() || stream->closed()));
#endif // DEBUG

    // Step 5: Return controller.[[strategyHWM]] − controller.[[queueTotalSize]].
    double strategyHWM = controller->getFixedSlot(ControllerSlot_StrategyHWM).toNumber();
    double queueSize = controller->getFixedSlot(QueueContainerSlot_TotalSize).toNumber();
    return strategyHWM - queueSize;
}

// Streams spec, 3.10.3 new ReadableByteStreamController ( stream, underlyingSource,
//                                                         highWaterMark )
// Steps 3 - 16.
static MOZ_MUST_USE ReadableByteStreamController*
CreateReadableByteStreamController(JSContext* cx, Handle<ReadableStream*> stream,
                                   HandleValue underlyingByteSource,
                                   HandleValue highWaterMarkVal)
{
    Rooted<ReadableByteStreamController*> controller(cx);
    controller = NewBuiltinClassInstance<ReadableByteStreamController>(cx);
    if (!controller)
        return nullptr;

    // Step 3: Set this.[[controlledReadableStream]] to stream.
    controller->setFixedSlot(ControllerSlot_Stream, ObjectValue(*stream));

    // Step 4: Set this.[[underlyingByteSource]] to underlyingByteSource.
    controller->setFixedSlot(ControllerSlot_UnderlyingSource, underlyingByteSource);

    // Step 5: Set this.[[pullAgain]], and this.[[pulling]] to false.
    controller->setFixedSlot(ControllerSlot_Flags, Int32Value(0));

    // Step 6: Perform ! ReadableByteStreamControllerClearPendingPullIntos(this).
    if (!ReadableByteStreamControllerClearPendingPullIntos(cx, controller))
        return nullptr;

    // Step 7: Perform ! ResetQueue(this).
    if (!ResetQueue(cx, controller))
        return nullptr;

    // Step 8: Set this.[[started]] and this.[[closeRequested]] to false.
    // These should be false by default, unchanged since step 5.
    MOZ_ASSERT(ControllerFlags(controller) == 0);

    // Step 9: Set this.[[strategyHWM]] to
    //         ? ValidateAndNormalizeHighWaterMark(highWaterMark).
    double highWaterMark;
    if (!ValidateAndNormalizeHighWaterMark(cx, highWaterMarkVal, &highWaterMark))
        return nullptr;
    controller->setFixedSlot(ControllerSlot_StrategyHWM, NumberValue(highWaterMark));

    // Step 10: Let autoAllocateChunkSize be
    //          ? GetV(underlyingByteSource, "autoAllocateChunkSize").
    RootedValue autoAllocateChunkSize(cx);
    if (!GetProperty(cx, underlyingByteSource, cx->names().autoAllocateChunkSize,
                     &autoAllocateChunkSize))
    {
        return nullptr;
    }

    // Step 11: If autoAllocateChunkSize is not undefined,
    if (!autoAllocateChunkSize.isUndefined()) {
        // Step a: If ! IsInteger(autoAllocateChunkSize) is false, or if
        //         autoAllocateChunkSize ≤ 0, throw a RangeError exception.
        if (!IsInteger(autoAllocateChunkSize) || autoAllocateChunkSize.toNumber() <= 0) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                      JSMSG_READABLEBYTESTREAMCONTROLLER_BAD_CHUNKSIZE);
            return nullptr;
    }
    }

    // Step 12: Set this.[[autoAllocateChunkSize]] to autoAllocateChunkSize.
    controller->setFixedSlot(ByteControllerSlot_AutoAllocateSize, autoAllocateChunkSize);

    // Step 13: Set this.[[pendingPullIntos]] to a new empty List.
    if (!SetNewList(cx, controller, ByteControllerSlot_PendingPullIntos))
        return nullptr;

    // Step 14: Let controller be this (implicit).

    // Step 15: Let startResult be
    //          ? InvokeOrNoop(underlyingSource, "start", « this »).
    RootedValue startResult(cx);
    RootedValue controllerVal(cx, ObjectValue(*controller));
    if (!InvokeOrNoop(cx, underlyingByteSource, cx->names().start, controllerVal, &startResult))
        return nullptr;

    // Step 16: Let startPromise be a promise resolved with startResult:
    RootedObject startPromise(cx, PromiseObject::unforgeableResolve(cx, startResult));
    if (!startPromise)
        return nullptr;

    RootedObject onStartFulfilled(cx, NewHandler(cx, ControllerStartHandler, controller));
    if (!onStartFulfilled)
        return nullptr;

    RootedObject onStartRejected(cx, NewHandler(cx, ControllerStartFailedHandler, controller));
    if (!onStartRejected)
        return nullptr;

    if (!JS::AddPromiseReactions(cx, startPromise, onStartFulfilled, onStartRejected))
        return nullptr;

    return controller;
}

bool
ReadableByteStreamController::hasExternalSource() {
    return ControllerFlags(this) & ControllerFlag_ExternalSource;
}

// Streams spec, 3.10.3.
// new ReadableByteStreamController ( stream, underlyingByteSource,
//                                    highWaterMark )
bool
ReadableByteStreamController::constructor(JSContext* cx, unsigned argc, Value* vp)
{
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_NO_CONSTRUCTOR,
                              "ReadableByteStreamController");
    return false;
}

// Version of the ReadableByteStreamConstructor that's specialized for
// handling external, embedding-provided, underlying sources.
static MOZ_MUST_USE ReadableByteStreamController*
CreateReadableByteStreamController(JSContext* cx, Handle<ReadableStream*> stream,
                                   void* underlyingSource)
{
    Rooted<ReadableByteStreamController*> controller(cx);
    controller = NewBuiltinClassInstance<ReadableByteStreamController>(cx);
    if (!controller)
        return nullptr;

    // Step 3: Set this.[[controlledReadableStream]] to stream.
    controller->setFixedSlot(ControllerSlot_Stream, ObjectValue(*stream));

    // Step 4: Set this.[[underlyingByteSource]] to underlyingByteSource.
    controller->setFixedSlot(ControllerSlot_UnderlyingSource, PrivateValue(underlyingSource));

    // Step 5: Set this.[[pullAgain]], and this.[[pulling]] to false.
    controller->setFixedSlot(ControllerSlot_Flags, Int32Value(ControllerFlag_ExternalSource));

    // Step 6: Perform ! ReadableByteStreamControllerClearPendingPullIntos(this).
    // Omitted.

    // Step 7: Perform ! ResetQueue(this).
    controller->setFixedSlot(QueueContainerSlot_TotalSize, Int32Value(0));

    // Step 8: Set this.[[started]] and this.[[closeRequested]] to false.
    // Step 9: Set this.[[strategyHWM]] to
    //         ? ValidateAndNormalizeHighWaterMark(highWaterMark).
    controller->setFixedSlot(ControllerSlot_StrategyHWM, Int32Value(0));

    // Step 10: Let autoAllocateChunkSize be
    //          ? GetV(underlyingByteSource, "autoAllocateChunkSize").
    // Step 11: If autoAllocateChunkSize is not undefined,
    // Step 12: Set this.[[autoAllocateChunkSize]] to autoAllocateChunkSize.
    // Omitted.

    // Step 13: Set this.[[pendingPullIntos]] to a new empty List.
    if (!SetNewList(cx, controller, ByteControllerSlot_PendingPullIntos))
        return nullptr;

    // Step 14: Let controller be this (implicit).
    // Step 15: Let startResult be
    //          ? InvokeOrNoop(underlyingSource, "start", « this »).
    // Omitted.

    // Step 16: Let startPromise be a promise resolved with startResult:
    RootedObject startPromise(cx, PromiseObject::unforgeableResolve(cx, UndefinedHandleValue));
    if (!startPromise)
        return nullptr;

    RootedObject onStartFulfilled(cx, NewHandler(cx, ControllerStartHandler, controller));
    if (!onStartFulfilled)
        return nullptr;

    RootedObject onStartRejected(cx, NewHandler(cx, ControllerStartFailedHandler, controller));
    if (!onStartRejected)
        return nullptr;

    if (!JS::AddPromiseReactions(cx, startPromise, onStartFulfilled, onStartRejected))
        return nullptr;

    return controller;
}

static MOZ_MUST_USE ReadableStreamBYOBRequest*
CreateReadableStreamBYOBRequest(JSContext* cx, Handle<ReadableByteStreamController*> controller,
                                HandleObject view);

// Streams spec, 3.10.4.1. get byobRequest
static MOZ_MUST_USE bool
ReadableByteStreamController_byobRequest_impl(JSContext* cx, const CallArgs& args)
{
    Rooted<ReadableByteStreamController*> controller(cx);
    controller = &args.thisv().toObject().as<ReadableByteStreamController>();

    // Step 2: If this.[[byobRequest]] is undefined and this.[[pendingPullIntos]]
    //         is not empty,
    Value val = controller->getFixedSlot(ByteControllerSlot_BYOBRequest);
    RootedValue byobRequest(cx, val);
    val = controller->getFixedSlot(ByteControllerSlot_PendingPullIntos);
    RootedNativeObject pendingPullIntos(cx, &val.toObject().as<NativeObject>());

    if (byobRequest.isUndefined() && pendingPullIntos->getDenseInitializedLength() != 0) {
        // Step a: Let firstDescriptor be the first element of this.[[pendingPullIntos]].
        Rooted<PullIntoDescriptor*> firstDescriptor(cx);
        firstDescriptor = PeekList<PullIntoDescriptor>(pendingPullIntos);

        // Step b: Let view be ! Construct(%Uint8Array%,
        //  « firstDescriptor.[[buffer]],
        //  firstDescriptor.[[byteOffset]] + firstDescriptor.[[bytesFilled]],
        //  firstDescriptor.[[byteLength]] − firstDescriptor.[[bytesFilled]] »).
        RootedArrayBufferObject buffer(cx, firstDescriptor->buffer());
        uint32_t bytesFilled = firstDescriptor->bytesFilled();
        RootedObject view(cx, JS_NewUint8ArrayWithBuffer(cx, buffer,
                                                         firstDescriptor->byteOffset() + bytesFilled,
                                                         firstDescriptor->byteLength() - bytesFilled));
        if (!view)
            return false;

        // Step c: Set this.[[byobRequest]] to
        //         ! Construct(ReadableStreamBYOBRequest, « this, view »).
        RootedObject request(cx, CreateReadableStreamBYOBRequest(cx, controller, view));
        if (!request)
            return false;
        byobRequest = ObjectValue(*request);
        controller->setFixedSlot(ByteControllerSlot_BYOBRequest, byobRequest);
    }

    // Step 3: Return this.[[byobRequest]].
    args.rval().set(byobRequest);
    return true;
}

static bool
ReadableByteStreamController_byobRequest(JSContext* cx, unsigned argc, Value* vp)
{
    // Step 1: If IsReadableByteStreamController(this) is false, throw a TypeError
    //         exception.
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<Is<ReadableByteStreamController>,
                                ReadableByteStreamController_byobRequest_impl>(cx, args);
}

// Streams spec, 3.10.4.2. get desiredSize
// Combined with 3.8.4.1 above.

static bool
ReadableByteStreamController_desiredSize(JSContext* cx, unsigned argc, Value* vp)
{
    // Step 1: If ! IsReadableByteStreamController(this) is false, throw a
    //         TypeError exception.
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<Is<ReadableByteStreamController>,
                                ReadableStreamController_desiredSize_impl>(cx, args);
}

static MOZ_MUST_USE bool
ReadableByteStreamControllerClose(JSContext* cx, Handle<ReadableByteStreamController*> controller);

// Streams spec, 3.10.4.3. close()
static MOZ_MUST_USE bool
ReadableByteStreamController_close_impl(JSContext* cx, const CallArgs& args)
{
    Rooted<ReadableByteStreamController*> controller(cx);
    controller = &args.thisv().toObject().as<ReadableByteStreamController>();

    // Steps 2-3.
    if (!VerifyControllerStateForClosing(cx, controller))
        return false;

    // Step 4: Perform ? ReadableByteStreamControllerClose(this).
    if (!ReadableByteStreamControllerClose(cx, controller))
        return false;
    args.rval().setUndefined();
    return true;
}

static bool
ReadableByteStreamController_close(JSContext* cx, unsigned argc, Value* vp)
{
    // Step 1: If ! IsReadableByteStreamController(this) is false, throw a
    //         TypeError exception.
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<Is<ReadableByteStreamController>,
                                ReadableByteStreamController_close_impl>(cx, args);
}

static MOZ_MUST_USE bool
ReadableByteStreamControllerEnqueue(JSContext* cx,
                                    Handle<ReadableByteStreamController*> controller,
                                    HandleObject chunk);

// Streams spec, 3.10.4.4. enqueue ( chunk )
static MOZ_MUST_USE bool
ReadableByteStreamController_enqueue_impl(JSContext* cx, const CallArgs& args)
{
    Rooted<ReadableByteStreamController*> controller(cx);
    controller = &args.thisv().toObject().as<ReadableByteStreamController>();
    HandleValue chunkVal = args.get(0);

    // Step 2: If this.[[closeRequested]] is true, throw a TypeError exception.
    if (ControllerFlags(controller) & ControllerFlag_CloseRequested) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAMCONTROLLER_CLOSED, "enqueue");
        return false;
    }

    // Step 3: If this.[[controlledReadableStream]].[[state]] is not "readable",
    //         throw a TypeError exception.
    if (!StreamFromController(controller)->readable()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAMCONTROLLER_NOT_READABLE, "enqueue");
        return false;
    }

    // Step 4: If Type(chunk) is not Object, throw a TypeError exception.
    // Step 5: If chunk does not have a [[ViewedArrayBuffer]] internal slot,
    //         throw a TypeError exception.
    if (!chunkVal.isObject() || !JS_IsArrayBufferViewObject(&chunkVal.toObject())) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLEBYTESTREAMCONTROLLER_BAD_CHUNK,
                                  "ReadableByteStreamController#enqueue");
        return false;
    }
    RootedObject chunk(cx, &chunkVal.toObject());

    // Step 6: Return ! ReadableByteStreamControllerEnqueue(this, chunk).
    if (!ReadableByteStreamControllerEnqueue(cx, controller, chunk))
        return false;
    args.rval().setUndefined();
    return true;
}

static bool
ReadableByteStreamController_enqueue(JSContext* cx, unsigned argc, Value* vp)
{
    // Step 1: If ! IsReadableByteStreamController(this) is false, throw a
    //         TypeError exception.
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<Is<ReadableByteStreamController>,
                                ReadableByteStreamController_enqueue_impl>(cx, args);
}

// Streams spec, 3.10.4.5. error ( e )
static MOZ_MUST_USE bool
ReadableByteStreamController_error_impl(JSContext* cx, const CallArgs& args)
{
    Rooted<ReadableByteStreamController*> controller(cx);
    controller = &args.thisv().toObject().as<ReadableByteStreamController>();
    HandleValue e = args.get(0);

    // Step 2: Let stream be this.[[controlledReadableStream]].
    // Step 3: If stream.[[state]] is not "readable", throw a TypeError exception.
    if (!StreamFromController(controller)->readable()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAMCONTROLLER_NOT_READABLE, "error");
        return false;
    }

    // Step 4: Perform ! ReadableByteStreamControllerError(this, e).
    if (!ReadableStreamControllerError(cx, controller, e))
        return false;
    args.rval().setUndefined();
    return true;
}

static bool
ReadableByteStreamController_error(JSContext* cx, unsigned argc, Value* vp)
{
    // Step 1: If ! IsReadableByteStreamController(this) is false, throw a
    //         TypeError exception.
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<Is<ReadableByteStreamController>,
                                ReadableByteStreamController_error_impl>(cx, args);
}

static const JSPropertySpec ReadableByteStreamController_properties[] = {
    JS_PSG("byobRequest", ReadableByteStreamController_byobRequest, 0),
    JS_PSG("desiredSize", ReadableByteStreamController_desiredSize, 0),
    JS_PS_END
};

static const JSFunctionSpec ReadableByteStreamController_methods[] = {
    JS_FN("close",      ReadableByteStreamController_close,     0, 0),
    JS_FN("enqueue",    ReadableByteStreamController_enqueue,   1, 0),
    JS_FN("error",      ReadableByteStreamController_error,     1, 0),
    JS_FS_END
};

static void
ReadableByteStreamControllerFinalize(FreeOp* fop, JSObject* obj)
{
    ReadableByteStreamController& controller = obj->as<ReadableByteStreamController>();

    if (controller.getFixedSlot(ControllerSlot_Flags).isUndefined())
        return;

    uint32_t flags = ControllerFlags(&controller);
    if (!(flags & ControllerFlag_ExternalSource))
        return;

    uint8_t embeddingFlags = flags >> ControllerEmbeddingFlagsOffset;

    void* underlyingSource = controller.getFixedSlot(ControllerSlot_UnderlyingSource).toPrivate();
    obj->runtimeFromAnyThread()->readableStreamFinalizeCallback(underlyingSource, embeddingFlags);
}

static const ClassOps ReadableByteStreamControllerClassOps = {
    nullptr,        /* addProperty */
    nullptr,        /* delProperty */
    nullptr,        /* getProperty */
    nullptr,        /* setProperty */
    nullptr,        /* enumerate */
    nullptr,        /* resolve */
    nullptr,        /* mayResolve */
    ReadableByteStreamControllerFinalize,
    nullptr,        /* call        */
    nullptr,        /* hasInstance */
    nullptr,        /* construct   */
    nullptr,        /* trace   */
};

CLASS_SPEC(ReadableByteStreamController, 3, 9, 0, JSCLASS_BACKGROUND_FINALIZE,
           &ReadableByteStreamControllerClassOps);

// Streams spec, 3.10.5.1. [[PullSteps]] ()
// Unified with 3.8.5.1 above.

static MOZ_MUST_USE bool
ReadableByteStreamControllerHandleQueueDrain(JSContext* cx, HandleNativeObject controller);

// Streams spec, 3.10.5.2. [[PullSteps]] ()
static JSObject*
ReadableByteStreamControllerPullSteps(JSContext* cx, HandleNativeObject controller)
{
    // Step 1: Let stream be this.[[controlledReadableStream]].
    Rooted<ReadableStream*> stream(cx, StreamFromController(controller));

    // Step 2: MOZ_ASSERT: ! ReadableStreamHasDefaultReader(stream) is true.
    MOZ_ASSERT(ReadableStreamHasDefaultReader(stream));

    RootedValue val(cx);
    // Step 3: If this.[[queueTotalSize]] > 0,
    double queueTotalSize = controller->getFixedSlot(QueueContainerSlot_TotalSize).toNumber();
    if (queueTotalSize > 0) {
        // Step 3.a: MOZ_ASSERT: ! ReadableStreamGetNumReadRequests(_stream_) is 0.
        MOZ_ASSERT(ReadableStreamGetNumReadRequests(stream) == 0);

        RootedObject view(cx);

        if (stream->mode() == JS::ReadableStreamMode::ExternalSource) {
            val = controller->getFixedSlot(ControllerSlot_UnderlyingSource);
            void* underlyingSource = val.toPrivate();

            view = JS_NewUint8Array(cx, queueTotalSize);
            if (!view)
                return nullptr;

            size_t bytesWritten;
            {
                JS::AutoCheckCannotGC noGC(cx);
                bool dummy;
                void* buffer = JS_GetArrayBufferViewData(view, &dummy, noGC);
                auto cb = cx->runtime()->readableStreamWriteIntoReadRequestCallback;
                MOZ_ASSERT(cb);
                // TODO: use bytesWritten to correctly update the request's state.
                cb(cx, stream, underlyingSource, stream->embeddingFlags(), buffer,
                   queueTotalSize, &bytesWritten);
            }

            queueTotalSize = queueTotalSize - bytesWritten;
        } else {
            // Step 3.b: Let entry be the first element of this.[[queue]].
            // Step 3.c: Remove entry from this.[[queue]], shifting all other elements
            //           downward (so that the second becomes the first, and so on).
            val = controller->getFixedSlot(QueueContainerSlot_Queue);
            RootedNativeObject queue(cx, &val.toObject().as<NativeObject>());
            Rooted<ByteStreamChunk*> entry(cx, ShiftFromList<ByteStreamChunk>(cx, queue));
            MOZ_ASSERT(entry);

            queueTotalSize = queueTotalSize - entry->byteLength();

            // Step 3.f: Let view be ! Construct(%Uint8Array%, « entry.[[buffer]],
            //                                   entry.[[byteOffset]], entry.[[byteLength]] »).
            // (reordered)
            RootedObject buffer(cx, entry->buffer());

            uint32_t byteOffset = entry->byteOffset();
            view = JS_NewUint8ArrayWithBuffer(cx, buffer, byteOffset, entry->byteLength());
            if (!view)
                return nullptr;
        }

        // Step 3.d: Set this.[[queueTotalSize]] to
        //           this.[[queueTotalSize]] − entry.[[byteLength]].
        // (reordered)
        controller->setFixedSlot(QueueContainerSlot_TotalSize, Int32Value(queueTotalSize));

        // Step 3.e: Perform ! ReadableByteStreamControllerHandleQueueDrain(this).
        // (reordered)
        if (!ReadableByteStreamControllerHandleQueueDrain(cx, controller))
            return nullptr;

        // Step 3.g: Return a promise resolved with ! CreateIterResultObject(view, false).
        val.setObject(*view);
        RootedObject iterResult(cx, CreateIterResultObject(cx, val, false));
        if (!iterResult)
            return nullptr;
        val.setObject(*iterResult);

        return PromiseObject::unforgeableResolve(cx, val);
    }

    // Step 4: Let autoAllocateChunkSize be this.[[autoAllocateChunkSize]].
    val = controller->getFixedSlot(ByteControllerSlot_AutoAllocateSize);

    // Step 5: If autoAllocateChunkSize is not undefined,
    if (!val.isUndefined()) {
        double autoAllocateChunkSize = val.toNumber();

        // Step 5.a: Let buffer be Construct(%ArrayBuffer%, « autoAllocateChunkSize »).
        RootedObject bufferObj(cx, JS_NewArrayBuffer(cx, autoAllocateChunkSize));

        // Step 5.b: If buffer is an abrupt completion,
        //           return a promise rejected with buffer.[[Value]].
        if (!bufferObj)
            return PromiseRejectedWithPendingError(cx);

        RootedArrayBufferObject buffer(cx, &bufferObj->as<ArrayBufferObject>());

        // Step 5.c: Let pullIntoDescriptor be Record {[[buffer]]: buffer.[[Value]],
        //                                             [[byteOffset]]: 0,
        //                                             [[byteLength]]: autoAllocateChunkSize,
        //                                             [[bytesFilled]]: 0, [[elementSize]]: 1,
        //                                             [[ctor]]: %Uint8Array%,
        //                                             [[readerType]]: `"default"`}.
        RootedObject pullIntoDescriptor(cx);
        pullIntoDescriptor = PullIntoDescriptor::create(cx, buffer, 0,
                                                        autoAllocateChunkSize, 0, 1,
                                                        nullptr,
                                                        ReaderType_Default);
        if (!pullIntoDescriptor)
            return PromiseRejectedWithPendingError(cx);

        // Step 5.d: Append pullIntoDescriptor as the last element of this.[[pendingPullIntos]].
        val = controller->getFixedSlot(ByteControllerSlot_PendingPullIntos);
        RootedNativeObject pendingPullIntos(cx, &val.toObject().as<NativeObject>());
        val = ObjectValue(*pullIntoDescriptor);
        if (!AppendToList(cx, pendingPullIntos, val))
            return nullptr;
    }

    // Step 6: Let promise be ! ReadableStreamAddReadRequest(stream).
    Rooted<PromiseObject*> promise(cx, ReadableStreamAddReadRequest(cx, stream));
    if (!promise)
        return nullptr;

    // Step 7: Perform ! ReadableByteStreamControllerCallPullIfNeeded(this).
    if (!ReadableStreamControllerCallPullIfNeeded(cx, controller))
        return nullptr;

    // Step 8: Return promise.
    return promise;
}

/**
 * Unified implementation of ReadableStream controllers' [[PullSteps]] internal
 * methods.
 * Streams spec, 3.8.5.2. [[PullSteps]] ()
 * and
 * Streams spec, 3.10.5.2. [[PullSteps]] ()
 */
static MOZ_MUST_USE JSObject*
ReadableStreamControllerPullSteps(JSContext* cx, HandleNativeObject controller)
{
    MOZ_ASSERT(IsReadableStreamController(controller));

    if (controller->is<ReadableStreamDefaultController>())
        return ReadableStreamDefaultControllerPullSteps(cx, controller);

    return ReadableByteStreamControllerPullSteps(cx, controller);
}


static MOZ_MUST_USE ReadableStreamBYOBRequest*
CreateReadableStreamBYOBRequest(JSContext* cx, Handle<ReadableByteStreamController*> controller,
                                HandleObject view)
{
    MOZ_ASSERT(controller);
    MOZ_ASSERT(JS_IsArrayBufferViewObject(view));

    Rooted<ReadableStreamBYOBRequest*> request(cx);
    request = NewBuiltinClassInstance<ReadableStreamBYOBRequest>(cx);
    if (!request)
        return nullptr;

  // Step 1: Set this.[[associatedReadableByteStreamController]] to controller.
  request->setFixedSlot(BYOBRequestSlot_Controller, ObjectValue(*controller));

  // Step 2: Set this.[[view]] to view.
  request->setFixedSlot(BYOBRequestSlot_View, ObjectValue(*view));

  return request;
}

// Streams spec, 3.11.3. new ReadableStreamBYOBRequest ( controller, view )
bool
ReadableStreamBYOBRequest::constructor(JSContext* cx, unsigned argc, Value* vp)
{
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_NO_CONSTRUCTOR,
                              "ReadableStreamBYOBRequest");
    return false;
}

// Streams spec, 3.11.4.1 get view
static MOZ_MUST_USE bool
ReadableStreamBYOBRequest_view_impl(JSContext* cx, const CallArgs& args)
{
    // Step 2: Return this.[[view]].
    NativeObject* request = &args.thisv().toObject().as<NativeObject>();
    args.rval().set(request->getFixedSlot(BYOBRequestSlot_View));
    return true;
}

static bool
ReadableStreamBYOBRequest_view(JSContext* cx, unsigned argc, Value* vp)
{
    // Step 1: If ! IsReadableStreamBYOBRequest(this) is false, throw a TypeError
    //         exception.
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<Is<ReadableStreamBYOBRequest>,
                                ReadableStreamBYOBRequest_view_impl>(cx, args);
}

static MOZ_MUST_USE bool
ReadableByteStreamControllerRespond(JSContext* cx,
                                    Handle<ReadableByteStreamController*> controller,
                                    HandleValue bytesWrittenVal);

// Streams spec, 3.11.4.2. respond ( bytesWritten )
static MOZ_MUST_USE bool
ReadableStreamBYOBRequest_respond_impl(JSContext* cx, const CallArgs& args)
{
    Rooted<ReadableStreamBYOBRequest*> request(cx);
    request = &args.thisv().toObject().as<ReadableStreamBYOBRequest>();
    HandleValue bytesWritten = args.get(0);

    // Step 2: If this.[[associatedReadableByteStreamController]] is undefined,
    //         throw a TypeError exception.
    RootedValue controllerVal(cx, request->getFixedSlot(BYOBRequestSlot_Controller));
    if (controllerVal.isUndefined()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAMBYOBREQUEST_NO_CONTROLLER, "respond");
        return false;
    }

    // Step 3: Return ?
    // ReadableByteStreamControllerRespond(this.[[associatedReadableByteStreamController]],
    //                                     bytesWritten).
    Rooted<ReadableByteStreamController*> controller(cx);
    controller = &controllerVal.toObject().as<ReadableByteStreamController>();

    if (!ReadableByteStreamControllerRespond(cx, controller, bytesWritten))
        return false;

    args.rval().setUndefined();
    return true;
}

static bool
ReadableStreamBYOBRequest_respond(JSContext* cx, unsigned argc, Value* vp)
{
    // Step 1: If ! IsReadableStreamBYOBRequest(this) is false, throw a TypeError
    //         exception.
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<Is<ReadableStreamBYOBRequest>,
                                ReadableStreamBYOBRequest_respond_impl>(cx, args);
}

static MOZ_MUST_USE bool
ReadableByteStreamControllerRespondWithNewView(JSContext* cx,
                                               Handle<ReadableByteStreamController*> controller,
                                               HandleObject view);

// Streams spec, 3.11.4.3. respondWithNewView ( view )
static MOZ_MUST_USE bool
ReadableStreamBYOBRequest_respondWithNewView_impl(JSContext* cx, const CallArgs& args)
{
    Rooted<ReadableStreamBYOBRequest*> request(cx);
    request = &args.thisv().toObject().as<ReadableStreamBYOBRequest>();
    HandleValue viewVal = args.get(0);

    // Step 2: If this.[[associatedReadableByteStreamController]] is undefined,
    //         throw a TypeError exception.
    RootedValue controllerVal(cx, request->getFixedSlot(BYOBRequestSlot_Controller));
    if (controllerVal.isUndefined()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAMBYOBREQUEST_NO_CONTROLLER, "respondWithNewView");
        return false;
    }

    // Step 3: If Type(chunk) is not Object, throw a TypeError exception.
    // Step 4: If view does not have a [[ViewedArrayBuffer]] internal slot, throw
    //         a TypeError exception.
    if (!viewVal.isObject() || !JS_IsArrayBufferViewObject(&viewVal.toObject())) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLEBYTESTREAMCONTROLLER_BAD_CHUNK,
                                  "ReadableStreamBYOBRequest#respondWithNewView");
        return false;
    }

    // Step 5: Return ?
    // ReadableByteStreamControllerRespondWithNewView(this.[[associatedReadableByteStreamController]],
    //                                                view).
    Rooted<ReadableByteStreamController*> controller(cx);
    controller = &controllerVal.toObject().as<ReadableByteStreamController>();
    RootedObject view(cx, &viewVal.toObject());

    if (!ReadableByteStreamControllerRespondWithNewView(cx, controller, view))
        return false;

    args.rval().setUndefined();
    return true;
}

static bool
ReadableStreamBYOBRequest_respondWithNewView(JSContext* cx, unsigned argc, Value* vp)
{
    // Step 1: If ! IsReadableStreamBYOBRequest(this) is false, throw a TypeError
    //         exception.
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<Is<ReadableStreamBYOBRequest>,
                                ReadableStreamBYOBRequest_respondWithNewView_impl>(cx, args);
}

static const JSPropertySpec ReadableStreamBYOBRequest_properties[] = {
    JS_PSG("view", ReadableStreamBYOBRequest_view, 0),
    JS_PS_END
};

static const JSFunctionSpec ReadableStreamBYOBRequest_methods[] = {
    JS_FN("respond",            ReadableStreamBYOBRequest_respond,            1, 0),
    JS_FN("respondWithNewView", ReadableStreamBYOBRequest_respondWithNewView, 1, 0),
    JS_FS_END
};

CLASS_SPEC(ReadableStreamBYOBRequest, 3, 2, 0, 0, JS_NULL_CLASS_OPS);

// Streams spec, 3.12.1. IsReadableStreamBYOBRequest ( x )
// Implemented via is<ReadableStreamBYOBRequest>()

// Streams spec, 3.12.2. IsReadableByteStreamController ( x )
// Implemented via is<ReadableByteStreamController>()

// Streams spec, 3.12.3. ReadableByteStreamControllerCallPullIfNeeded ( controller )
// Unified with 3.9.2 above.

static void
ReadableByteStreamControllerInvalidateBYOBRequest(NativeObject* controller);

// Streams spec, 3.12.4. ReadableByteStreamControllerClearPendingPullIntos ( controller )
static MOZ_MUST_USE bool
ReadableByteStreamControllerClearPendingPullIntos(JSContext* cx, HandleNativeObject controller)
{
    MOZ_ASSERT(controller->is<ReadableByteStreamController>());

    // Step 1: Perform ! ReadableByteStreamControllerInvalidateBYOBRequest(controller).
    ReadableByteStreamControllerInvalidateBYOBRequest(controller);

    // Step 2: Set controller.[[pendingPullIntos]] to a new empty List.
    return SetNewList(cx, controller, ByteControllerSlot_PendingPullIntos);
}

// Streams spec, 3.12.5. ReadableByteStreamControllerClose ( controller )
static MOZ_MUST_USE bool
ReadableByteStreamControllerClose(JSContext* cx, Handle<ReadableByteStreamController*> controller)
{
    // Step 1: Let stream be controller.[[controlledReadableStream]].
    Rooted<ReadableStream*> stream(cx, StreamFromController(controller));

    // Step 2: Assert: controller.[[closeRequested]] is false.
    MOZ_ASSERT(!(ControllerFlags(controller) & ControllerFlag_CloseRequested));

    // Step 3: Assert: stream.[[state]] is "readable".
    MOZ_ASSERT(stream->readable());

    // Step 4: If controller.[[queueTotalSize]] > 0,
    double queueTotalSize = controller->getFixedSlot(QueueContainerSlot_TotalSize).toNumber();
    if (queueTotalSize > 0) {
        // Step a: Set controller.[[closeRequested]] to true.
        AddControllerFlags(controller, ControllerFlag_CloseRequested);

        // Step b: Return
        return true;
    }

    // Step 5: If controller.[[pendingPullIntos]] is not empty,
    RootedValue val(cx, controller->getFixedSlot(ByteControllerSlot_PendingPullIntos));
    RootedNativeObject pendingPullIntos(cx, &val.toObject().as<NativeObject>());
    if (pendingPullIntos->getDenseInitializedLength() != 0) {
        // Step a: Let firstPendingPullInto be the first element of
        //         controller.[[pendingPullIntos]].
        Rooted<PullIntoDescriptor*> firstPendingPullInto(cx);
        firstPendingPullInto = PeekList<PullIntoDescriptor>(pendingPullIntos);

        // Step b: If firstPendingPullInto.[[bytesFilled]] > 0,
        if (firstPendingPullInto->bytesFilled() > 0) {
            // Step i: Let e be a new TypeError exception. {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                      JSMSG_READABLEBYTESTREAMCONTROLLER_CLOSE_PENDING_PULL);
            RootedValue e(cx);
            // Not much we can do about uncatchable exceptions, just bail.
            if (!cx->getPendingException(&e))
                return false;
            // Step ii: Perform ! ReadableByteStreamControllerError(controller, e).
            if (!ReadableStreamControllerError(cx, controller, e))
                return false;

            // Step iii: Throw e.
            return false;
        }
    }

    // Step 6: Perform ! ReadableStreamClose(stream).
    return ReadableStreamCloseInternal(cx, stream);
}

static MOZ_MUST_USE JSObject*
ReadableByteStreamControllerConvertPullIntoDescriptor(JSContext* cx,
                                                      Handle<PullIntoDescriptor*> pullIntoDescriptor);

// Streams spec, 3.12.6. ReadableByteStreamControllerCommitPullIntoDescriptor ( stream, pullIntoDescriptor )
static MOZ_MUST_USE bool
ReadableByteStreamControllerCommitPullIntoDescriptor(JSContext* cx, Handle<ReadableStream*> stream,
                                                     Handle<PullIntoDescriptor*> pullIntoDescriptor)
{
    // Step 1: MOZ_ASSERT: stream.[[state]] is not "errored".
    MOZ_ASSERT(!stream->errored());

    // Step 2: Let done be false.
    bool done = false;

    // Step 3: If stream.[[state]] is "closed",
    if (stream->closed()) {
        // Step a: MOZ_ASSERT: pullIntoDescriptor.[[bytesFilled]] is 0.
        MOZ_ASSERT(pullIntoDescriptor->bytesFilled() == 0);

        // Step b: Set done to true.
        done = true;
    }

    // Step 4: Let filledView be
    //         ! ReadableByteStreamControllerConvertPullIntoDescriptor(pullIntoDescriptor).
    RootedObject filledView(cx);
    filledView = ReadableByteStreamControllerConvertPullIntoDescriptor(cx, pullIntoDescriptor);
    if (!filledView)
        return false;

    // Step 5: If pullIntoDescriptor.[[readerType]] is "default",
    uint32_t readerType = pullIntoDescriptor->readerType();
    RootedValue filledViewVal(cx, ObjectValue(*filledView));
    if (readerType == ReaderType_Default) {
        // Step a: Perform ! ReadableStreamFulfillReadRequest(stream, filledView, done).
        if (!ReadableStreamFulfillReadOrReadIntoRequest(cx, stream, filledViewVal, done))
            return false;
    } else {
        // Step 6: Otherwise,
        // Step a: MOZ_ASSERT: pullIntoDescriptor.[[readerType]] is "byob".
        MOZ_ASSERT(readerType == ReaderType_BYOB);

        // Step b: Perform ! ReadableStreamFulfillReadIntoRequest(stream, filledView, done).
        if (!ReadableStreamFulfillReadOrReadIntoRequest(cx, stream, filledViewVal, done))
            return false;
    }

    return true;
}

// Streams spec, 3.12.7. ReadableByteStreamControllerConvertPullIntoDescriptor ( pullIntoDescriptor )
static MOZ_MUST_USE JSObject*
ReadableByteStreamControllerConvertPullIntoDescriptor(JSContext* cx,
                                                      Handle<PullIntoDescriptor*> pullIntoDescriptor)
{
    // Step 1: Let bytesFilled be pullIntoDescriptor.[[bytesFilled]].
    uint32_t bytesFilled = pullIntoDescriptor->bytesFilled();

    // Step 2: Let elementSize be pullIntoDescriptor.[[elementSize]].
    uint32_t elementSize = pullIntoDescriptor->elementSize();

    // Step 3: Assert: bytesFilled <= pullIntoDescriptor.[[byteLength]].
    MOZ_ASSERT(bytesFilled <= pullIntoDescriptor->byteLength());

    // Step 4: Assert: bytesFilled mod elementSize is 0.
    MOZ_ASSERT(bytesFilled % elementSize == 0);

    // Step 5: Return ! Construct(pullIntoDescriptor.[[ctor]],
    //                            pullIntoDescriptor.[[buffer]],
    //                            pullIntoDescriptor.[[byteOffset]],
    //                            bytesFilled / elementSize).
    RootedObject ctor(cx, pullIntoDescriptor->ctor());
    if (!ctor) {
        if (!GetBuiltinConstructor(cx, JSProto_Uint8Array, &ctor))
            return nullptr;
    }
    RootedObject buffer(cx, pullIntoDescriptor->buffer());
    uint32_t byteOffset = pullIntoDescriptor->byteOffset();
    FixedConstructArgs<3> args(cx);
    args[0].setObject(*buffer);
    args[1].setInt32(byteOffset);
    args[2].setInt32(bytesFilled / elementSize);
    return JS_New(cx, ctor, args);
}

static MOZ_MUST_USE bool
ReadableByteStreamControllerEnqueueChunkToQueue(JSContext* cx,
                                                Handle<ReadableByteStreamController*> controller,
                                                HandleObject buffer, uint32_t byteOffset,
                                                uint32_t byteLength);

static MOZ_MUST_USE ArrayBufferObject*
TransferArrayBuffer(JSContext* cx, HandleObject buffer);

static MOZ_MUST_USE bool
ReadableByteStreamControllerProcessPullIntoDescriptorsUsingQueue(JSContext* cx,
                                                                 Handle<ReadableByteStreamController*> controller);

// Streams spec, 3.12.8. ReadableByteStreamControllerEnqueue ( controller, chunk )
static MOZ_MUST_USE bool
ReadableByteStreamControllerEnqueue(JSContext* cx,
                                    Handle<ReadableByteStreamController*> controller,
                                    HandleObject chunk)
{
    // Step 1: Let stream be controller.[[controlledReadableStream]].
    Rooted<ReadableStream*> stream(cx, StreamFromController(controller));

    // Step 2: Assert: controller.[[closeRequested]] is false.
    MOZ_ASSERT(!(ControllerFlags(controller) & ControllerFlag_CloseRequested));

    // Step 3: Assert: stream.[[state]] is "readable".
    MOZ_ASSERT(stream->readable());

    // To make enqueuing chunks via JSAPI nicer, we want to be able to deal
    // with ArrayBuffer objects in addition to ArrayBuffer views here.
    // This cannot happen when enqueuing happens via
    // ReadableByteStreamController_enqueue because that throws if invoked
    // with anything but an ArrayBuffer view.

    Rooted<ArrayBufferObject*> buffer(cx);
    uint32_t byteOffset;
    uint32_t byteLength;

    if (chunk->is<ArrayBufferObject>()) {
        // Steps 4-6 for ArrayBuffer objects.
        buffer = &chunk->as<ArrayBufferObject>();
        byteOffset = 0;
        byteLength = buffer->byteLength();
    } else {
        // Step 4: Let buffer be chunk.[[ViewedArrayBuffer]].
        bool dummy;
        JSObject* bufferObj = JS_GetArrayBufferViewBuffer(cx, chunk, &dummy);
        if (!bufferObj)
            return false;
        buffer = &bufferObj->as<ArrayBufferObject>();

        // Step 5: Let byteOffset be chunk.[[ByteOffset]].
        byteOffset = JS_GetArrayBufferViewByteOffset(chunk);

        // Step 6: Let byteLength be chunk.[[ByteLength]].
        byteLength = JS_GetArrayBufferViewByteLength(chunk);
    }

    // Step 7: Let transferredBuffer be ! TransferArrayBuffer(buffer).
    RootedArrayBufferObject transferredBuffer(cx, TransferArrayBuffer(cx, buffer));
    if (!transferredBuffer)
        return false;

    // Step 8: If ! ReadableStreamHasDefaultReader(stream) is true
    if (ReadableStreamHasDefaultReader(stream)) {
        // Step a: If ! ReadableStreamGetNumReadRequests(stream) is 0,
        if (ReadableStreamGetNumReadRequests(stream) == 0) {
            // Step i: Perform
            //         ! ReadableByteStreamControllerEnqueueChunkToQueue(controller,
            //                                                           transferredBuffer,
            //                                                           byteOffset,
            //                                                           byteLength).
            if (!ReadableByteStreamControllerEnqueueChunkToQueue(cx, controller, transferredBuffer,
                                                                 byteOffset, byteLength))
            {
                return false;
            }
        } else {
            // Step b: Otherwise,
            // Step i: Assert: controller.[[queue]] is empty.
#if DEBUG
            RootedValue val(cx, controller->getFixedSlot(QueueContainerSlot_Queue));
            RootedNativeObject queue(cx, &val.toObject().as<NativeObject>());
            MOZ_ASSERT(queue->getDenseInitializedLength() == 0);
#endif // DEBUG

            // Step ii: Let transferredView be
            //          ! Construct(%Uint8Array%, transferredBuffer, byteOffset, byteLength).
            RootedObject transferredView(cx, JS_NewUint8ArrayWithBuffer(cx, transferredBuffer,
                                                                        byteOffset, byteLength));
            if (!transferredView)
                return false;

            // Step iii: Perform ! ReadableStreamFulfillReadRequest(stream, transferredView, false).
            RootedValue chunk(cx, ObjectValue(*transferredView));
            if (!ReadableStreamFulfillReadOrReadIntoRequest(cx, stream, chunk, false))
                return false;
        }
    } else if (ReadableStreamHasBYOBReader(stream)) {
        // Step 9: Otherwise,
        // Step a: If ! ReadableStreamHasBYOBReader(stream) is true,
        // Step i: Perform
        //         ! ReadableByteStreamControllerEnqueueChunkToQueue(controller,
        //                                                           transferredBuffer,
        //                                                           byteOffset,
        //                                                           byteLength).
        if (!ReadableByteStreamControllerEnqueueChunkToQueue(cx, controller, transferredBuffer,
                                                             byteOffset, byteLength))
        {
            return false;
        }

        // Step ii: Perform ! ReadableByteStreamControllerProcessPullIntoDescriptorsUsingQueue(controller).
        if (!ReadableByteStreamControllerProcessPullIntoDescriptorsUsingQueue(cx, controller))
            return false;
    } else {
        // Step b: Otherwise,
        // Step i: Assert: ! IsReadableStreamLocked(stream) is false.
        MOZ_ASSERT(!stream->locked());

        // Step ii: Perform
        //          ! ReadableByteStreamControllerEnqueueChunkToQueue(controller,
        //                                                            transferredBuffer,
        //                                                            byteOffset,
        //                                                            byteLength).
        if (!ReadableByteStreamControllerEnqueueChunkToQueue(cx, controller, transferredBuffer,
                                                            byteOffset, byteLength))
        {
            return false;
        }
    }

    return true;
}

// Streams spec, 3.12.9.
// ReadableByteStreamControllerEnqueueChunkToQueue ( controller, buffer,
//                                                   byteOffset, byteLength )
static MOZ_MUST_USE bool
ReadableByteStreamControllerEnqueueChunkToQueue(JSContext* cx,
                                                Handle<ReadableByteStreamController*> controller,
                                                HandleObject buffer, uint32_t byteOffset,
                                                uint32_t byteLength)
{
    MOZ_ASSERT(controller->is<ReadableByteStreamController>(), "must operate on ReadableByteStreamController");

    // Step 1: Append Record {[[buffer]]: buffer,
    //                        [[byteOffset]]: byteOffset,
    //                        [[byteLength]]: byteLength}
    //         as the last element of controller.[[queue]].
    RootedValue val(cx, controller->getFixedSlot(QueueContainerSlot_Queue));
    RootedNativeObject queue(cx, &val.toObject().as<NativeObject>());

    Rooted<ByteStreamChunk*> chunk(cx);
    chunk = ByteStreamChunk::create(cx, buffer, byteOffset, byteLength);
    if (!chunk)
        return false;

    RootedValue chunkVal(cx, ObjectValue(*chunk));
    if (!AppendToList(cx, queue, chunkVal))
        return false;

    // Step 2: Add byteLength to controller.[[queueTotalSize]].
    double queueTotalSize = controller->getFixedSlot(QueueContainerSlot_TotalSize).toNumber();
    controller->setFixedSlot(QueueContainerSlot_TotalSize,
                             NumberValue(queueTotalSize + byteLength));

    return true;
}

// Streams spec, 3.12.10. ReadableByteStreamControllerError ( controller, e )
// Unified with 3.9.6 above.

// Streams spec, 3.12.11. ReadableByteStreamControllerFillHeadPullIntoDescriptor ( controler, size, pullIntoDescriptor )
static void
ReadableByteStreamControllerFillHeadPullIntoDescriptor(ReadableByteStreamController* controller, uint32_t size,
                                                       Handle<PullIntoDescriptor*> pullIntoDescriptor)
{
    // Step 1: Assert: either controller.[[pendingPullIntos]] is empty, or the
    //         first element of controller.[[pendingPullIntos]] is pullIntoDescriptor.
#if DEBUG
    Value val = controller->getFixedSlot(ByteControllerSlot_PendingPullIntos);
    NativeObject* pendingPullIntos = &val.toObject().as<NativeObject>();
    MOZ_ASSERT(pendingPullIntos->getDenseInitializedLength() == 0 ||
               &pendingPullIntos->getDenseElement(0).toObject() == pullIntoDescriptor);
#endif // DEBUG

    // Step 2: Perform ! ReadableByteStreamControllerInvalidateBYOBRequest(controller).
    ReadableByteStreamControllerInvalidateBYOBRequest(controller);

    // Step 3: Set pullIntoDescriptor.[[bytesFilled]] to pullIntoDescriptor.[[bytesFilled]] + size.
    pullIntoDescriptor->setBytesFilled(pullIntoDescriptor->bytesFilled() + size);
}

// Streams spec, 3.12.12. ReadableByteStreamControllerFillPullIntoDescriptorFromQueue ( controller, pullIntoDescriptor )
static MOZ_MUST_USE bool
ReadableByteStreamControllerFillPullIntoDescriptorFromQueue(JSContext* cx,
                                                            Handle<ReadableByteStreamController*> controller,
                                                            Handle<PullIntoDescriptor*> pullIntoDescriptor,
                                                            bool* ready)
{
    *ready = false;

    // Step 1: Let elementSize be pullIntoDescriptor.[[elementSize]].
    uint32_t elementSize = pullIntoDescriptor->elementSize();

    // Step 2: Let currentAlignedBytes be pullIntoDescriptor.[[bytesFilled]] −
    //         (pullIntoDescriptor.[[bytesFilled]] mod elementSize).
    uint32_t bytesFilled = pullIntoDescriptor->bytesFilled();
    uint32_t currentAlignedBytes = bytesFilled - (bytesFilled % elementSize);

    // Step 3: Let maxBytesToCopy be min(controller.[[queueTotalSize]],
    //         pullIntoDescriptor.[[byteLength]] − pullIntoDescriptor.[[bytesFilled]]).
    uint32_t byteLength = pullIntoDescriptor->byteLength();

    // The queue size could be negative or overflow uint32_t. We cannot
    // validly have a maxBytesToCopy value that'd overflow uint32_t, though,
    // so just clamp to that.
    Value sizeVal = controller->getFixedSlot(QueueContainerSlot_TotalSize);
    uint32_t queueTotalSize = JS::ToUint32(sizeVal.toNumber());
    uint32_t maxBytesToCopy = std::min(queueTotalSize, byteLength - bytesFilled);

    // Step 4: Let maxBytesFilled be pullIntoDescriptor.[[bytesFilled]] + maxBytesToCopy.
    uint32_t maxBytesFilled = bytesFilled + maxBytesToCopy;

    // Step 5: Let maxAlignedBytes be maxBytesFilled − (maxBytesFilled mod elementSize).
    uint32_t maxAlignedBytes = maxBytesFilled - (maxBytesFilled % elementSize);

    // Step 6: Let totalBytesToCopyRemaining be maxBytesToCopy.
    uint32_t totalBytesToCopyRemaining = maxBytesToCopy;

    // Step 7: Let ready be false (implicit).

    // Step 8: If maxAlignedBytes > currentAlignedBytes,
    if (maxAlignedBytes > currentAlignedBytes) {
        // Step a: Set totalBytesToCopyRemaining to maxAlignedBytes −
        //         pullIntoDescriptor.[[bytesFilled]].
        totalBytesToCopyRemaining = maxAlignedBytes - bytesFilled;

        // Step b: Let ready be true.
        *ready = true;
    }

    if (ControllerFlags(controller) & ControllerFlag_ExternalSource) {
        // TODO: it probably makes sense to eagerly drain the underlying source.
        // We have a buffer lying around anyway, whereas the source might be
        // able to free or reuse buffers once their content is copied into
        // our buffer.
        if (!ready)
            return true;

        Value val = controller->getFixedSlot(ControllerSlot_UnderlyingSource);
        void* underlyingSource = val.toPrivate();

        RootedArrayBufferObject targetBuffer(cx, pullIntoDescriptor->buffer());
        Rooted<ReadableStream*> stream(cx, StreamFromController(controller));

        size_t bytesWritten;
        {
            JS::AutoCheckCannotGC noGC(cx);
            bool dummy;
            uint8_t* buffer = JS_GetArrayBufferData(targetBuffer, &dummy, noGC);
            buffer += bytesFilled;
            auto cb = cx->runtime()->readableStreamWriteIntoReadRequestCallback;
            MOZ_ASSERT(cb);
            cb(cx, stream, underlyingSource, stream->embeddingFlags(), buffer,
               totalBytesToCopyRemaining, &bytesWritten);
            pullIntoDescriptor->setBytesFilled(bytesFilled + bytesWritten);
        }

        queueTotalSize -= bytesWritten;
        controller->setFixedSlot(QueueContainerSlot_TotalSize, Int32Value(queueTotalSize));

        return true;
    }

    // Step 9: Let queue be controller.[[queue]].
    RootedValue val(cx, controller->getFixedSlot(QueueContainerSlot_Queue));
    RootedNativeObject queue(cx, &val.toObject().as<NativeObject>());

    // Step 10: Repeat the following steps while totalBytesToCopyRemaining > 0,
    Rooted<ByteStreamChunk*> headOfQueue(cx);
    while (totalBytesToCopyRemaining > 0) {
        MOZ_ASSERT(queue->getDenseInitializedLength() != 0);

        // Step a: Let headOfQueue be the first element of queue.
        headOfQueue = PeekList<ByteStreamChunk>(queue);

        // Step b: Let bytesToCopy be min(totalBytesToCopyRemaining,
        //                                headOfQueue.[[byteLength]]).
        uint32_t byteLength = headOfQueue->byteLength();
        uint32_t bytesToCopy = std::min(totalBytesToCopyRemaining, byteLength);

        // Step c: Let destStart be pullIntoDescriptor.[[byteOffset]] +
        //         pullIntoDescriptor.[[bytesFilled]].
        uint32_t destStart = pullIntoDescriptor->byteOffset() + bytesFilled;

        // Step d: Perform ! CopyDataBlockBytes(pullIntoDescriptor.[[buffer]].[[ArrayBufferData]],
        //                                      destStart,
        //                                      headOfQueue.[[buffer]].[[ArrayBufferData]],
        //                                      headOfQueue.[[byteOffset]],
        //                                      bytesToCopy).
        RootedArrayBufferObject sourceBuffer(cx, headOfQueue->buffer());
        uint32_t sourceOffset = headOfQueue->byteOffset();
        RootedArrayBufferObject targetBuffer(cx, pullIntoDescriptor->buffer());
        ArrayBufferObject::copyData(targetBuffer, destStart, sourceBuffer, sourceOffset,
                                    bytesToCopy);

        // Step e: If headOfQueue.[[byteLength]] is bytesToCopy,
        if (byteLength == bytesToCopy) {
            // Step i: Remove the first element of queue, shifting all other elements
            //         downward (so that the second becomes the first, and so on).
            headOfQueue = ShiftFromList<ByteStreamChunk>(cx, queue);
            MOZ_ASSERT(headOfQueue);
        } else {
            // Step f: Otherwise,
            // Step i: Set headOfQueue.[[byteOffset]] to headOfQueue.[[byteOffset]] +
            //         bytesToCopy.
            headOfQueue->SetByteOffset(sourceOffset + bytesToCopy);

            // Step ii: Set headOfQueue.[[byteLength]] to headOfQueue.[[byteLength]] −
            //          bytesToCopy.
            headOfQueue->SetByteLength(byteLength - bytesToCopy);
        }

        // Step g: Set controller.[[queueTotalSize]] to
        //         controller.[[queueTotalSize]] − bytesToCopy.
        queueTotalSize = controller->getFixedSlot(QueueContainerSlot_TotalSize).toNumber();
        queueTotalSize -= bytesToCopy;
        controller->setFixedSlot(QueueContainerSlot_TotalSize, NumberValue(queueTotalSize));

        // Step h: Perform ! ReadableByteStreamControllerFillHeadPullIntoDescriptor(controller,
        //                                                                          bytesToCopy,
        //                                                                          pullIntoDescriptor).
        ReadableByteStreamControllerFillHeadPullIntoDescriptor(controller, bytesToCopy,
                                                               pullIntoDescriptor);
        bytesFilled += bytesToCopy;
        MOZ_ASSERT(bytesFilled == pullIntoDescriptor->bytesFilled());

        // Step i: Set totalBytesToCopyRemaining to totalBytesToCopyRemaining − bytesToCopy.
        totalBytesToCopyRemaining -= bytesToCopy;
    }

    // Step 11: If ready is false,
    if (!*ready) {
        // Step a: Assert: controller.[[queueTotalSize]] is 0.
        MOZ_ASSERT(controller->getFixedSlot(QueueContainerSlot_TotalSize).toNumber() == 0);

        // Step b: Assert: pullIntoDescriptor.[[bytesFilled]] > 0.
        MOZ_ASSERT(bytesFilled > 0, "should have filled some bytes");

        // Step c: Assert: pullIntoDescriptor.[[bytesFilled]] <
        //         pullIntoDescriptor.[[elementSize]].
        MOZ_ASSERT(bytesFilled < elementSize);
    }

    // Step 12: Return ready.
    return true;
}

// Streams spec 3.12.13. ReadableByteStreamControllerGetDesiredSize ( controller )
// Unified with 3.9.8 above.

// Streams spec, 3.12.14. ReadableByteStreamControllerHandleQueueDrain ( controller )
static MOZ_MUST_USE bool
ReadableByteStreamControllerHandleQueueDrain(JSContext* cx, HandleNativeObject controller)
{
    MOZ_ASSERT(controller->is<ReadableByteStreamController>());

    // Step 1: Assert: controller.[[controlledReadableStream]].[[state]] is "readable".
    Rooted<ReadableStream*> stream(cx, StreamFromController(controller));
    MOZ_ASSERT(stream->readable());

    // Step 2: If controller.[[queueTotalSize]] is 0 and
    //         controller.[[closeRequested]] is true,
    double totalSize = controller->getFixedSlot(QueueContainerSlot_TotalSize).toNumber();
    bool closeRequested = ControllerFlags(controller) & ControllerFlag_CloseRequested;
    if (totalSize == 0 && closeRequested) {
      // Step a: Perform ! ReadableStreamClose(controller.[[controlledReadableStream]]).
      return ReadableStreamCloseInternal(cx, stream);
    }

    // Step 3: Otherwise,
    // Step a: Perform ! ReadableByteStreamControllerCallPullIfNeeded(controller).
    return ReadableStreamControllerCallPullIfNeeded(cx, controller);
}

// Streams spec 3.12.15. ReadableByteStreamControllerInvalidateBYOBRequest ( controller )
static void
ReadableByteStreamControllerInvalidateBYOBRequest(NativeObject* controller)
{
    MOZ_ASSERT(controller->is<ReadableByteStreamController>());

    // Step 1: If controller.[[byobRequest]] is undefined, return.
    Value byobRequestVal = controller->getFixedSlot(ByteControllerSlot_BYOBRequest);
    if (byobRequestVal.isUndefined())
        return;

    NativeObject* byobRequest = &byobRequestVal.toObject().as<NativeObject>();
    // Step 2: Set controller.[[byobRequest]].[[associatedReadableByteStreamController]]
    //         to undefined.
    byobRequest->setFixedSlot(BYOBRequestSlot_Controller, UndefinedValue());

    // Step 3: Set controller.[[byobRequest]].[[view]] to undefined.
    byobRequest->setFixedSlot(BYOBRequestSlot_View, UndefinedValue());

    // Step 4: Set controller.[[byobRequest]] to undefined.
    controller->setFixedSlot(ByteControllerSlot_BYOBRequest, UndefinedValue());
}

static MOZ_MUST_USE PullIntoDescriptor*
ReadableByteStreamControllerShiftPendingPullInto(JSContext* cx, HandleNativeObject controller);

// Streams spec 3.12.16. ReadableByteStreamControllerProcessPullIntoDescriptorsUsingQueue ( controller )
static MOZ_MUST_USE bool
ReadableByteStreamControllerProcessPullIntoDescriptorsUsingQueue(JSContext* cx,
                                                                 Handle<ReadableByteStreamController*> controller)
{
    Rooted<ReadableStream*> stream(cx, StreamFromController(controller));

    // Step 1: Assert: controller.[[closeRequested]] is false.
    MOZ_ASSERT(!(ControllerFlags(controller) & ControllerFlag_CloseRequested));

    // Step 2: Repeat the following steps while controller.[[pendingPullIntos]]
    //         is not empty,
    RootedValue val(cx, controller->getFixedSlot(ByteControllerSlot_PendingPullIntos));
    RootedNativeObject pendingPullIntos(cx, &val.toObject().as<NativeObject>());
    Rooted<PullIntoDescriptor*> pullIntoDescriptor(cx);
    while (pendingPullIntos->getDenseInitializedLength() != 0) {
        // Step a: If controller.[[queueTotalSize]] is 0, return.
        double queueTotalSize = controller->getFixedSlot(QueueContainerSlot_TotalSize).toNumber();
        if (queueTotalSize == 0)
            return true;

        // Step b: Let pullIntoDescriptor be the first element of
        //         controller.[[pendingPullIntos]].
        pullIntoDescriptor = PeekList<PullIntoDescriptor>(pendingPullIntos);

        // Step c: If ! ReadableByteStreamControllerFillPullIntoDescriptorFromQueue(controller, pullIntoDescriptor)
        //         is true,
        bool ready;
        if (!ReadableByteStreamControllerFillPullIntoDescriptorFromQueue(cx, controller,
                                                                         pullIntoDescriptor,
                                                                         &ready))
        {
            return false;
        }
        if (ready) {
            // Step i: Perform ! ReadableByteStreamControllerShiftPendingPullInto(controller).
            if (!ReadableByteStreamControllerShiftPendingPullInto(cx, controller))
                return false;

            // Step ii: Perform ! ReadableByteStreamControllerCommitPullIntoDescriptor(controller.[[controlledReadableStream]],
            //                                                                         pullIntoDescriptor).
            if (!ReadableByteStreamControllerCommitPullIntoDescriptor(cx, stream,
                                                                      pullIntoDescriptor))
            {
                return false;
            }
        }
    }

    return true;
}

// Streams spec, 3.12.17. ReadableByteStreamControllerPullInto ( controller, view )
static MOZ_MUST_USE JSObject*
ReadableByteStreamControllerPullInto(JSContext* cx,
                                     Handle<ReadableByteStreamController*> controller,
                                     Handle<ArrayBufferViewObject*> view)
{
    MOZ_ASSERT(controller->is<ReadableByteStreamController>());

    // Step 1: Let stream be controller.[[controlledReadableStream]].
    Rooted<ReadableStream*> stream(cx, StreamFromController(controller));

    // Step 2: Let elementSize be 1.
    uint32_t elementSize = 1;

    RootedObject ctor(cx);
    // Step 4: If view has a [[TypedArrayName]] internal slot (i.e., it is not a
    //         DataView),
    if (view->is<TypedArrayObject>()) {
        JSProtoKey protoKey = StandardProtoKeyOrNull(view);
        MOZ_ASSERT(protoKey);

        if (!GetBuiltinConstructor(cx, protoKey, &ctor))
            return nullptr;
        elementSize = 1 << TypedArrayShift(view->as<TypedArrayObject>().type());
    } else {
        // Step 3: Let ctor be %DataView% (reordered).
        if (!GetBuiltinConstructor(cx, JSProto_DataView, &ctor))
            return nullptr;
    }

    // Step 5: Let pullIntoDescriptor be Record {[[buffer]]: view.[[ViewedArrayBuffer]],
    //                                           [[byteOffset]]: view.[[ByteOffset]],
    //                                           [[byteLength]]: view.[[ByteLength]],
    //                                           [[bytesFilled]]: 0,
    //                                           [[elementSize]]: elementSize,
    //                                           [[ctor]]: ctor,
    //                                           [[readerType]]: "byob"}.
    bool dummy;
    RootedArrayBufferObject buffer(cx, &JS_GetArrayBufferViewBuffer(cx, view, &dummy)
                                       ->as<ArrayBufferObject>());
    if (!buffer)
        return nullptr;

    uint32_t byteOffset = JS_GetArrayBufferViewByteOffset(view);
    uint32_t byteLength = JS_GetArrayBufferViewByteLength(view);
    Rooted<PullIntoDescriptor*> pullIntoDescriptor(cx);
    pullIntoDescriptor = PullIntoDescriptor::create(cx, buffer, byteOffset, byteLength, 0,
                                                    elementSize, ctor,
                                                    ReaderType_BYOB);
    if (!pullIntoDescriptor)
        return nullptr;

    // Step 6: If controller.[[pendingPullIntos]] is not empty,
    RootedValue val(cx, controller->getFixedSlot(ByteControllerSlot_PendingPullIntos));
    RootedNativeObject pendingPullIntos(cx, &val.toObject().as<NativeObject>());
    if (pendingPullIntos->getDenseInitializedLength() != 0) {
        // Step a: Set pullIntoDescriptor.[[buffer]] to
        //         ! TransferArrayBuffer(pullIntoDescriptor.[[buffer]]).
        RootedArrayBufferObject transferredBuffer(cx, TransferArrayBuffer(cx, buffer));
        if (!transferredBuffer)
            return nullptr;
        pullIntoDescriptor->setBuffer(transferredBuffer);

        // Step b: Append pullIntoDescriptor as the last element of
        //         controller.[[pendingPullIntos]].
        val = ObjectValue(*pullIntoDescriptor);
        if (!AppendToList(cx, pendingPullIntos, val))
            return nullptr;

        // Step c: Return ! ReadableStreamAddReadIntoRequest(stream).
        return ReadableStreamAddReadIntoRequest(cx, stream);
    }

    // Step 7: If stream.[[state]] is "closed",
    if (stream->closed()) {
        // Step a: Let emptyView be ! Construct(ctor, pullIntoDescriptor.[[buffer]],
        //                                            pullIntoDescriptor.[[byteOffset]], 0).
        FixedConstructArgs<3> args(cx);
        args[0].setObject(*buffer);
        args[1].setInt32(byteOffset);
        args[2].setInt32(0);
        RootedObject emptyView(cx, JS_New(cx, ctor, args));
        if (!emptyView)
            return nullptr;

        // Step b: Return a promise resolved with
        //         ! CreateIterResultObject(emptyView, true).
        RootedValue val(cx, ObjectValue(*emptyView));
        RootedObject iterResult(cx, CreateIterResultObject(cx, val, true));
        if (!iterResult)
            return nullptr;
        val = ObjectValue(*iterResult);
        return PromiseObject::unforgeableResolve(cx, val);
    }

    // Step 8: If controller.[[queueTotalSize]] > 0,
    double queueTotalSize = controller->getFixedSlot(QueueContainerSlot_TotalSize).toNumber();
    if (queueTotalSize > 0) {
        // Step a: If ! ReadableByteStreamControllerFillPullIntoDescriptorFromQueue(controller,
        //                                                                          pullIntoDescriptor)
        //         is true,
        bool ready;
        if (!ReadableByteStreamControllerFillPullIntoDescriptorFromQueue(cx, controller,
                                                                         pullIntoDescriptor, &ready))
        {
            return nullptr;
        }

        if (ready) {
            // Step i: Let filledView be
            //         ! ReadableByteStreamControllerConvertPullIntoDescriptor(pullIntoDescriptor).
            RootedObject filledView(cx);
            filledView = ReadableByteStreamControllerConvertPullIntoDescriptor(cx,
                                                                               pullIntoDescriptor);
            if (!filledView)
                return nullptr;

            // Step ii: Perform ! ReadableByteStreamControllerHandleQueueDrain(controller).
            if (!ReadableByteStreamControllerHandleQueueDrain(cx, controller))
                return nullptr;

            // Step iii: Return a promise resolved with
            //           ! CreateIterResultObject(filledView, false).
            val = ObjectValue(*filledView);
            RootedObject iterResult(cx, CreateIterResultObject(cx, val, false));
            if (!iterResult)
                return nullptr;
            val = ObjectValue(*iterResult);
            return PromiseObject::unforgeableResolve(cx, val);
        }

        // Step b: If controller.[[closeRequested]] is true,
        if (ControllerFlags(controller) & ControllerFlag_CloseRequested) {
            // Step i: Let e be a TypeError exception.
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                      JSMSG_READABLESTREAMCONTROLLER_CLOSED, "read");

            // Not much we can do about uncatchable exceptions, just bail.
            RootedValue e(cx);
            if (!GetAndClearException(cx, &e))
                return nullptr;

            // Step ii: Perform ! ReadableByteStreamControllerError(controller, e).
            if (!ReadableStreamControllerError(cx, controller, e))
                return nullptr;

            // Step iii: Return a promise rejected with e.
            return PromiseObject::unforgeableReject(cx, e);
        }
    }

    // Step 9: Set pullIntoDescriptor.[[buffer]] to
    //         ! TransferArrayBuffer(pullIntoDescriptor.[[buffer]]).
    RootedArrayBufferObject transferredBuffer(cx, TransferArrayBuffer(cx, buffer));
    if (!transferredBuffer)
        return nullptr;
    pullIntoDescriptor->setBuffer(transferredBuffer);

    // Step 10: Append pullIntoDescriptor as the last element of
    //          controller.[[pendingPullIntos]].
    val = ObjectValue(*pullIntoDescriptor);
    if (!AppendToList(cx, pendingPullIntos, val))
        return nullptr;

    // Step 11: Let promise be ! ReadableStreamAddReadIntoRequest(stream).
    Rooted<PromiseObject*> promise(cx, ReadableStreamAddReadIntoRequest(cx, stream));
    if (!promise)
        return nullptr;

    // Step 12: Perform ! ReadableByteStreamControllerCallPullIfNeeded(controller).
    if (!ReadableStreamControllerCallPullIfNeeded(cx, controller))
        return nullptr;

    // Step 13: Return promise.
    return promise;
}

static MOZ_MUST_USE bool
ReadableByteStreamControllerRespondInternal(JSContext* cx,
                                            Handle<ReadableByteStreamController*> controller,
                                            double bytesWritten);

// Streams spec 3.12.18. ReadableByteStreamControllerRespond( controller, bytesWritten )
static MOZ_MUST_USE bool
ReadableByteStreamControllerRespond(JSContext* cx,
                                    Handle<ReadableByteStreamController*> controller,
                                    HandleValue bytesWrittenVal)
{
    MOZ_ASSERT(controller->is<ReadableByteStreamController>());

    // Step 1: Let bytesWritten be ? ToNumber(bytesWritten).
    double bytesWritten;
    if (!ToNumber(cx, bytesWrittenVal, &bytesWritten))
        return false;

    // Step 2: If ! IsFiniteNonNegativeNumber(bytesWritten) is false,
    if (bytesWritten < 0 || mozilla::IsNaN(bytesWritten) || mozilla::IsInfinite(bytesWritten)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_NUMBER_MUST_BE_FINITE_NON_NEGATIVE, "bytesWritten");
        return false;
    }

    // Step 3: Assert: controller.[[pendingPullIntos]] is not empty.
#if DEBUG
    Value val = controller->getFixedSlot(ByteControllerSlot_PendingPullIntos);
    RootedNativeObject pendingPullIntos(cx, &val.toObject().as<NativeObject>());
    MOZ_ASSERT(pendingPullIntos->getDenseInitializedLength() != 0);
#endif // DEBUG

    // Step 4: Perform ? ReadableByteStreamControllerRespondInternal(controller, bytesWritten).
    return ReadableByteStreamControllerRespondInternal(cx, controller, bytesWritten);
}

// Streams spec 3.12.19. ReadableByteStreamControllerRespondInClosedState( controller, firstDescriptor )
static MOZ_MUST_USE bool
ReadableByteStreamControllerRespondInClosedState(JSContext* cx,
                                                   Handle<ReadableByteStreamController*> controller,
                                                   Handle<PullIntoDescriptor*> firstDescriptor)
{
    // Step 1: Set firstDescriptor.[[buffer]] to
    //         ! TransferArrayBuffer(firstDescriptor.[[buffer]]).
    RootedArrayBufferObject buffer(cx, firstDescriptor->buffer());
    RootedArrayBufferObject transferredBuffer(cx, TransferArrayBuffer(cx, buffer));
    if (!transferredBuffer)
        return false;
    firstDescriptor->setBuffer(transferredBuffer);

    // Step 2: Assert: firstDescriptor.[[bytesFilled]] is 0.
    MOZ_ASSERT(firstDescriptor->bytesFilled() == 0);

    // Step 3: Let stream be controller.[[controlledReadableStream]].
    Rooted<ReadableStream*> stream(cx, StreamFromController(controller));

    // Step 4: If ReadableStreamHasBYOBReader(stream) is true,
    if (ReadableStreamHasBYOBReader(stream)) {
        // Step a: Repeat the following steps while
        //         ! ReadableStreamGetNumReadIntoRequests(stream) > 0,
        Rooted<PullIntoDescriptor*> descriptor(cx);
        while (ReadableStreamGetNumReadRequests(stream) > 0) {
            // Step i: Let pullIntoDescriptor be
            //         ! ReadableByteStreamControllerShiftPendingPullInto(controller).
            descriptor = ReadableByteStreamControllerShiftPendingPullInto(cx, controller);
            if (!descriptor)
                return false;

            // Step ii: Perform !
            //          ReadableByteStreamControllerCommitPullIntoDescriptor(stream,
            //                                                               pullIntoDescriptor).
            if (!ReadableByteStreamControllerCommitPullIntoDescriptor(cx, stream, descriptor))
                return false;
        }
    }

    return true;
}

// Streams spec 3.12.20.
// ReadableByteStreamControllerRespondInReadableState( controller, bytesWritten, pullIntoDescriptor )
static MOZ_MUST_USE bool
ReadableByteStreamControllerRespondInReadableState(JSContext* cx,
                                                   Handle<ReadableByteStreamController*> controller,
                                                   uint32_t bytesWritten,
                                                   Handle<PullIntoDescriptor*> pullIntoDescriptor)
{
    // Step 1: If pullIntoDescriptor.[[bytesFilled]] + bytesWritten > pullIntoDescriptor.[[byteLength]],
    //         throw a RangeError exception.
    uint32_t bytesFilled = pullIntoDescriptor->bytesFilled();
    uint32_t byteLength = pullIntoDescriptor->byteLength();
    if (bytesFilled + bytesWritten > byteLength) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLEBYTESTREAMCONTROLLER_INVALID_BYTESWRITTEN);
        return false;
    }

    // Step 2: Perform ! ReadableByteStreamControllerFillHeadPullIntoDescriptor(controller,
    //                                                                          bytesWritten,
    //                                                                          pullIntoDescriptor).
    ReadableByteStreamControllerFillHeadPullIntoDescriptor(controller, bytesWritten,
                                                           pullIntoDescriptor);
    bytesFilled += bytesWritten;

    // Step 3: If pullIntoDescriptor.[[bytesFilled]] <
    //         pullIntoDescriptor.[[elementSize]], return.
    uint32_t elementSize = pullIntoDescriptor->elementSize();
    if (bytesFilled < elementSize)
        return true;

    // Step 4: Perform ! ReadableByteStreamControllerShiftPendingPullInto(controller).
    if (!ReadableByteStreamControllerShiftPendingPullInto(cx, controller))
        return false;

    // Step 5: Let remainderSize be pullIntoDescriptor.[[bytesFilled]] mod
    //         pullIntoDescriptor.[[elementSize]].
    uint32_t remainderSize = bytesFilled % elementSize;

    // Step 6: If remainderSize > 0,
    RootedArrayBufferObject buffer(cx, pullIntoDescriptor->buffer());
    if (remainderSize > 0) {
        // Step a: Let end be pullIntoDescriptor.[[byteOffset]] +
        //         pullIntoDescriptor.[[bytesFilled]].
        uint32_t end = pullIntoDescriptor->byteOffset() + bytesFilled;

        // Step b: Let remainder be ? CloneArrayBuffer(pullIntoDescriptor.[[buffer]],
        //                                             end − remainderSize,
        //                                             remainderSize, %ArrayBuffer%).
        // TODO: this really, really should just use a slot to store the remainder.
        RootedObject remainderObj(cx, JS_NewArrayBuffer(cx, remainderSize));
        if (!remainderObj)
            return false;
        RootedArrayBufferObject remainder(cx, &remainderObj->as<ArrayBufferObject>());
        ArrayBufferObject::copyData(remainder, 0, buffer, end - remainderSize, remainderSize);

        // Step c: Perform ! ReadableByteStreamControllerEnqueueChunkToQueue(controller,
        //                                                                   remainder, 0,
        //                                                                   remainder.[[ByteLength]]).
        // Note: `remainderSize` is equivalent to remainder.[[ByteLength]].
        if (!ReadableByteStreamControllerEnqueueChunkToQueue(cx, controller, remainder, 0,
                                                             remainderSize))
        {
            return false;
        }
    }

    // Step 7: Set pullIntoDescriptor.[[buffer]] to
    //         ! TransferArrayBuffer(pullIntoDescriptor.[[buffer]]).
    RootedArrayBufferObject transferredBuffer(cx, TransferArrayBuffer(cx, buffer));
    if (!transferredBuffer)
        return false;
    pullIntoDescriptor->setBuffer(transferredBuffer);

    // Step 8: Set pullIntoDescriptor.[[bytesFilled]] to pullIntoDescriptor.[[bytesFilled]] −
    //         remainderSize.
    pullIntoDescriptor->setBytesFilled(bytesFilled - remainderSize);

    // Step 9: Perform ! ReadableByteStreamControllerCommitPullIntoDescriptor(controller.[[controlledReadableStream]],
    //                                                                        pullIntoDescriptor).
    Rooted<ReadableStream*> stream(cx, StreamFromController(controller));
    if (!ReadableByteStreamControllerCommitPullIntoDescriptor(cx, stream, pullIntoDescriptor))
        return false;

    // Step 10: Perform ! ReadableByteStreamControllerProcessPullIntoDescriptorsUsingQueue(controller).
    return ReadableByteStreamControllerProcessPullIntoDescriptorsUsingQueue(cx, controller);
}

// Streams spec, 3.12.21. ReadableByteStreamControllerRespondInternal ( controller, bytesWritten )
static MOZ_MUST_USE bool
ReadableByteStreamControllerRespondInternal(JSContext* cx,
                                            Handle<ReadableByteStreamController*> controller,
                                            double bytesWritten)
{
    // Step 1: Let firstDescriptor be the first element of controller.[[pendingPullIntos]].
    RootedValue val(cx, controller->getFixedSlot(ByteControllerSlot_PendingPullIntos));
    RootedNativeObject pendingPullIntos(cx, &val.toObject().as<NativeObject>());
    Rooted<PullIntoDescriptor*> firstDescriptor(cx);
    firstDescriptor = PeekList<PullIntoDescriptor>(pendingPullIntos);

    // Step 2: Let stream be controller.[[controlledReadableStream]].
    Rooted<ReadableStream*> stream(cx, StreamFromController(controller));

    // Step 3: If stream.[[state]] is "closed",
    if (stream->closed()) {
        // Step a: If bytesWritten is not 0, throw a TypeError exception.
        if (bytesWritten != 0) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                      JSMSG_READABLESTREAMBYOBREQUEST_RESPOND_CLOSED);
            return false;
        }

        // Step b: Perform
        //         ! ReadableByteStreamControllerRespondInClosedState(controller, firstDescriptor).
        return ReadableByteStreamControllerRespondInClosedState(cx, controller, firstDescriptor);
    }

    // Step 4: Otherwise,
    // Step a: Assert: stream.[[state]] is "readable".
    MOZ_ASSERT(stream->readable());

    // Step b: Perform ? ReadableByteStreamControllerRespondInReadableState(controller,
    //                                                                      bytesWritten,
    //                                                                      firstDescriptor).
    return ReadableByteStreamControllerRespondInReadableState(cx, controller, bytesWritten,
                                                              firstDescriptor);
}

// Streams spec, 3.12.22. ReadableByteStreamControllerRespondWithNewView ( controller, view )
static MOZ_MUST_USE bool
ReadableByteStreamControllerRespondWithNewView(JSContext* cx,
                                               Handle<ReadableByteStreamController*> controller,
                                               HandleObject view)
{
    // Step 1: Assert: controller.[[pendingPullIntos]] is not empty.
    RootedValue val(cx, controller->getFixedSlot(ByteControllerSlot_PendingPullIntos));
    RootedNativeObject pendingPullIntos(cx, &val.toObject().as<NativeObject>());
    MOZ_ASSERT(pendingPullIntos->getDenseInitializedLength() != 0);

    // Step 2: Let firstDescriptor be the first element of controller.[[pendingPullIntos]].
    Rooted<PullIntoDescriptor*> firstDescriptor(cx);
    firstDescriptor = PeekList<PullIntoDescriptor>(pendingPullIntos);

    // Step 3: If firstDescriptor.[[byteOffset]] + firstDescriptor.[[bytesFilled]]
    //         is not view.[[ByteOffset]], throw a RangeError exception.
    uint32_t byteOffset = uint32_t(JS_GetArrayBufferViewByteOffset(view));
    if (firstDescriptor->byteOffset() + firstDescriptor->bytesFilled() != byteOffset) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLEBYTESTREAMCONTROLLER_INVALID_VIEW_OFFSET);
        return false;
    }

    // Step 4: If firstDescriptor.[[byteLength]] is not view.[[ByteLength]],
    //         throw a RangeError exception.
    uint32_t byteLength = JS_GetArrayBufferViewByteLength(view);
    if (firstDescriptor->byteLength() != byteLength) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLEBYTESTREAMCONTROLLER_INVALID_VIEW_SIZE);
        return false;
    }

    // Step 5: Set firstDescriptor.[[buffer]] to view.[[ViewedArrayBuffer]].
    bool dummy;
    RootedArrayBufferObject buffer(cx,
                                   &AsArrayBuffer(JS_GetArrayBufferViewBuffer(cx, view, &dummy)));
    if (!buffer)
        return false;
    firstDescriptor->setBuffer(buffer);

    // Step 6: Perform ? ReadableByteStreamControllerRespondInternal(controller,
    //                                                               view.[[ByteLength]]).
    return ReadableByteStreamControllerRespondInternal(cx, controller, byteLength);
}

// Streams spec, 3.12.23. ReadableByteStreamControllerShiftPendingPullInto ( controller )
static MOZ_MUST_USE PullIntoDescriptor*
ReadableByteStreamControllerShiftPendingPullInto(JSContext* cx, HandleNativeObject controller)
{
    MOZ_ASSERT(controller->is<ReadableByteStreamController>());

    // Step 1: Let descriptor be the first element of controller.[[pendingPullIntos]].
    // Step 2: Remove descriptor from controller.[[pendingPullIntos]], shifting
    //         all other elements downward (so that the second becomes the first,
    //         and so on).
    RootedValue val(cx, controller->getFixedSlot(ByteControllerSlot_PendingPullIntos));
    RootedNativeObject pendingPullIntos(cx, &val.toObject().as<NativeObject>());
    Rooted<PullIntoDescriptor*> descriptor(cx);
    descriptor = ShiftFromList<PullIntoDescriptor>(cx, pendingPullIntos);
    MOZ_ASSERT(descriptor);

    // Step 3: Perform ! ReadableByteStreamControllerInvalidateBYOBRequest(controller).
    ReadableByteStreamControllerInvalidateBYOBRequest(controller);

    // Step 4: Return descriptor.
    return descriptor;
}

// Streams spec, 3.12.24. ReadableByteStreamControllerShouldCallPull ( controller )
// Unified with 3.9.3 above.

// Streams spec, 6.1.2. new ByteLengthQueuingStrategy({ highWaterMark })
bool
js::ByteLengthQueuingStrategy::constructor(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!ThrowIfNotConstructing(cx, args, "ByteLengthQueuingStrategy"))
        return false;

    RootedObject proto(cx);
    RootedObject newTarget(cx, &args.newTarget().toObject());
    if (!GetPrototypeFromConstructor(cx, newTarget, &proto))
        return false;

    RootedObject strategy(cx, NewObjectWithClassProto<ByteLengthQueuingStrategy>(cx, proto));
    if (!strategy)
        return false;

    RootedObject argObj(cx, ToObject(cx, args.get(0)));
    if (!argObj)
      return false;

    RootedValue highWaterMark(cx);
    if (!GetProperty(cx, argObj, argObj, cx->names().highWaterMark, &highWaterMark))
      return false;

    if (!SetProperty(cx, strategy, cx->names().highWaterMark, highWaterMark))
      return false;

    args.rval().setObject(*strategy);
    return true;
}

// Streams spec 6.1.3.1. size ( chunk )
bool
ByteLengthQueuingStrategy_size(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Step 1: Return ? GetV(chunk, "byteLength").
    return GetProperty(cx, args.get(0), cx->names().byteLength, args.rval());
}

static const JSPropertySpec ByteLengthQueuingStrategy_properties[] = {
    JS_PS_END
};

static const JSFunctionSpec ByteLengthQueuingStrategy_methods[] = {
    JS_FN("size", ByteLengthQueuingStrategy_size, 1, 0),
    JS_FS_END
};

CLASS_SPEC(ByteLengthQueuingStrategy, 1, 0, 0, 0, JS_NULL_CLASS_OPS);

// Streams spec, 6.2.2. new CountQueuingStrategy({ highWaterMark })
bool
js::CountQueuingStrategy::constructor(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!ThrowIfNotConstructing(cx, args, "CountQueuingStrategy"))
        return false;

    RootedObject proto(cx);
    RootedObject newTarget(cx, &args.newTarget().toObject());
    if (!GetPrototypeFromConstructor(cx, newTarget, &proto))
        return false;

    Rooted<CountQueuingStrategy*> strategy(cx);
    strategy = NewObjectWithClassProto<CountQueuingStrategy>(cx, proto);
    if (!strategy)
        return false;

    RootedObject argObj(cx, ToObject(cx, args.get(0)));
    if (!argObj)
      return false;

    RootedValue highWaterMark(cx);
    if (!GetProperty(cx, argObj, argObj, cx->names().highWaterMark, &highWaterMark))
      return false;

    if (!SetProperty(cx, strategy, cx->names().highWaterMark, highWaterMark))
      return false;

    args.rval().setObject(*strategy);
    return true;
}

// Streams spec 6.2.3.1. size ( chunk )
bool
CountQueuingStrategy_size(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Step 1: Return 1.
    args.rval().setInt32(1);
    return true;
}

static const JSPropertySpec CountQueuingStrategy_properties[] = {
    JS_PS_END
};

static const JSFunctionSpec CountQueuingStrategy_methods[] = {
    JS_FN("size", CountQueuingStrategy_size, 0, 0),
    JS_FS_END
};

CLASS_SPEC(CountQueuingStrategy, 1, 0, 0, 0, JS_NULL_CLASS_OPS);

#undef CLASS_SPEC

// Streams spec, 6.3.1. DequeueValue ( container ) nothrow
inline static MOZ_MUST_USE bool
DequeueValue(JSContext* cx, HandleNativeObject container, MutableHandleValue chunk)
{
    // Step 1: Assert: container has [[queue]] and [[queueTotalSize]] internal
    //         slots.
    MOZ_ASSERT(IsReadableStreamController(container));

    // Step 2: Assert: queue is not empty.
    RootedValue val(cx, container->getFixedSlot(QueueContainerSlot_Queue));
    RootedNativeObject queue(cx);
    queue = UnwrapInternalSlotObject<NativeObject>(&val.toObject());
    MOZ_ASSERT(queue->getDenseInitializedLength() > 0);

    // Step 3. Let pair be the first element of queue.
    // Step 4. Remove pair from queue, shifting all other elements downward
    //         (so that the second becomes the first, and so on).
    Rooted<QueueEntry*> pair(cx, ShiftFromList<QueueEntry>(cx, queue));
    MOZ_ASSERT(pair);

    // Step 5: Set container.[[queueTotalSize]] to
    //         container.[[queueTotalSize]] − pair.[[size]].
    // Step 6: If container.[[queueTotalSize]] < 0, set
    //         container.[[queueTotalSize]] to 0.
    //         (This can occur due to rounding errors.)
    double totalSize = container->getFixedSlot(QueueContainerSlot_TotalSize).toNumber();

    totalSize -= pair->size();
    if (totalSize < 0)
        totalSize = 0;
    container->setFixedSlot(QueueContainerSlot_TotalSize, NumberValue(totalSize));

    // Step 7: Return pair.[[value]].
    RootedValue chunkVal(cx, pair->value());
    if (!JS_WrapValue(cx, &chunkVal))
        return false;
    chunk.set(chunkVal);
    return true;
}

// Streams spec, 6.3.2. EnqueueValueWithSize ( container, value, size ) throws
static MOZ_MUST_USE bool
EnqueueValueWithSize(JSContext* cx, HandleNativeObject container, HandleValue value,
                     HandleValue sizeVal)
{
    // Step 1: Assert: container has [[queue]] and [[queueTotalSize]] internal
    //         slots.
    MOZ_ASSERT(IsReadableStreamController(container));

    // Step 2: Let size be ? ToNumber(size).
    double size;
    if (!ToNumber(cx, sizeVal, &size))
        return false;

    // Step 3: If ! IsFiniteNonNegativeNumber(size) is false, throw a RangeError
    //         exception.
    if (size < 0 || mozilla::IsNaN(size) || mozilla::IsInfinite(size)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_NUMBER_MUST_BE_FINITE_NON_NEGATIVE, "size");
        return false;
    }

    // Step 4: Append Record {[[value]]: value, [[size]]: size} as the last element
    //         of container.[[queue]].
    RootedValue val(cx, container->getFixedSlot(QueueContainerSlot_Queue));
    RootedNativeObject queue(cx);
    queue = UnwrapInternalSlotObject<NativeObject>(&val.toObject());

    {
        JSAutoCompartment ac(cx, queue);
        RootedValue wrappedValue(cx, value);
        if (!JS_WrapValue(cx, &wrappedValue))
            return false;

        QueueEntry* entry = QueueEntry::create(cx, wrappedValue, size);
        if (!entry)
            return false;
        val = ObjectValue(*entry);
        if (!AppendToList(cx, queue, val))
            return false;

        // Step 5: Set container.[[queueTotalSize]] to
        //         container.[[queueTotalSize]] + size.
        double totalSize = container->getFixedSlot(QueueContainerSlot_TotalSize).toNumber();
        container->setFixedSlot(QueueContainerSlot_TotalSize, NumberValue(totalSize + size));
    }

    return true;
}

// Streams spec, 6.3.3. PeekQueueValue ( container ) nothrow
// Used by WritableStream.
// static MOZ_MUST_USE Value
// PeekQueueValue(NativeObject* container)
// {
//     // Step 1: Assert: container has [[queue]] and [[queueTotalSize]] internal
//     //         slots.
//     MOZ_ASSERT(IsReadableStreamController(container));

//     // Step 2: Assert: queue is not empty.
//     Value val = container->getFixedSlot(QueueContainerSlot_Queue);
//     NativeObject* queue = &val.toObject().as<NativeObject>();
//     MOZ_ASSERT(queue->getDenseInitializedLength() > 0);

//     // Step 3: Let pair be the first element of container.[[queue]].
//     QueueEntry* pair = PeekList<QueueEntry>(queue);

//     // Step 4: Return pair.[[value]].
//     return pair->value();
// }

/**
 * Streams spec, 6.3.4. ResetQueue ( container ) nothrow
 */
inline static MOZ_MUST_USE bool
ResetQueue(JSContext* cx, HandleNativeObject container)
{
    // Step 1: Assert: container has [[queue]] and [[queueTotalSize]] internal
    //         slots.
    MOZ_ASSERT(IsReadableStreamController(container));

    // Step 2: Set container.[[queue]] to a new empty List.
    if (!SetNewList(cx, container, QueueContainerSlot_Queue))
        return false;

    // Step 3: Set container.[[queueTotalSize]] to 0.
    container->setFixedSlot(QueueContainerSlot_TotalSize, NumberValue(0));

    return true;
}


/**
 * Streams spec, 6.4.1. InvokeOrNoop ( O, P, args )
 */
inline static MOZ_MUST_USE bool
InvokeOrNoop(JSContext* cx, HandleValue O, HandlePropertyName P, HandleValue arg,
             MutableHandleValue rval)
{
    // Step 1: Assert: P is a valid property key (omitted).
    // Step 2: If args was not passed, let args be a new empty List (omitted).
    // Step 3: Let method be ? GetV(O, P).
    RootedValue method(cx);
    if (!GetProperty(cx, O, P, &method))
        return false;

    // Step 4: If method is undefined, return.
    if (method.isUndefined())
        return true;

    // Step 5: Return ? Call(method, O, args).
    return Call(cx, method, O, arg, rval);
}

/**
 * Streams spec, 6.4.3. PromiseInvokeOrNoop ( O, P, args )
 * Specialized to one arg, because that's what all stream related callers use.
 */
static MOZ_MUST_USE JSObject*
PromiseInvokeOrNoop(JSContext* cx, HandleValue O, HandlePropertyName P, HandleValue arg)
{
    // Step 1: Assert: O is not undefined.
    MOZ_ASSERT(!O.isUndefined());

    // Step 2: Assert: ! IsPropertyKey(P) is true (implicit).
    // Step 3: Assert: args is a List (omitted).

    // Step 4: Let returnValue be InvokeOrNoop(O, P, args).
    // Step 5: If returnValue is an abrupt completion, return a promise
    //         rejected with returnValue.[[Value]].
    RootedValue returnValue(cx);
    if (!InvokeOrNoop(cx, O, P, arg, &returnValue))
        return PromiseRejectedWithPendingError(cx);

    // Step 6: Otherwise, return a promise resolved with returnValue.[[Value]].
    return PromiseObject::unforgeableResolve(cx, returnValue);
}

/**
 * Streams spec, 6.4.4 TransferArrayBuffer ( O )
 */
static MOZ_MUST_USE ArrayBufferObject*
TransferArrayBuffer(JSContext* cx, HandleObject buffer)
{
    // Step 1 (implicit).

    // Step 2.
    MOZ_ASSERT(buffer->is<ArrayBufferObject>());

    // Step 3.
    MOZ_ASSERT(!JS_IsDetachedArrayBufferObject(buffer));

    // Step 5 (reordered).
    uint32_t size = buffer->as<ArrayBufferObject>().byteLength();

    // Steps 4, 6.
    void* contents = JS_StealArrayBufferContents(cx, buffer);
    if (!contents)
        return nullptr;
    MOZ_ASSERT(JS_IsDetachedArrayBufferObject(buffer));

    // Step 7.
    RootedObject transferredBuffer(cx, JS_NewArrayBufferWithContents(cx, size, contents));
    if (!transferredBuffer)
        return nullptr;
    return &transferredBuffer->as<ArrayBufferObject>();
}

// Streams spec, 6.4.5. ValidateAndNormalizeHighWaterMark ( highWaterMark )
static MOZ_MUST_USE bool
ValidateAndNormalizeHighWaterMark(JSContext* cx, HandleValue highWaterMarkVal, double* highWaterMark)
{
    // Step 1: Set highWaterMark to ? ToNumber(highWaterMark).
    if (!ToNumber(cx, highWaterMarkVal, highWaterMark))
        return false;

    // Step 2: If highWaterMark is NaN, throw a TypeError exception.
    // Step 3: If highWaterMark < 0, throw a RangeError exception.
    if (mozilla::IsNaN(*highWaterMark) || *highWaterMark < 0) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_STREAM_INVALID_HIGHWATERMARK);
        return false;
    }

    // Step 4: Return highWaterMark.
    return true;
}

// Streams spec, 6.4.6. ValidateAndNormalizeQueuingStrategy ( size, highWaterMark )
static MOZ_MUST_USE bool
ValidateAndNormalizeQueuingStrategy(JSContext* cx, HandleValue size,
                                    HandleValue highWaterMarkVal, double* highWaterMark)
{
    // Step 1: If size is not undefined and ! IsCallable(size) is false, throw a
    //         TypeError exception.
    if (!size.isUndefined() && !IsCallable(size)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_NOT_FUNCTION,
                                  "ReadableStream argument options.size");
        return false;
    }

    // Step 2: Let highWaterMark be ? ValidateAndNormalizeHighWaterMark(highWaterMark).
    if (!ValidateAndNormalizeHighWaterMark(cx, highWaterMarkVal, highWaterMark))
        return false;

    // Step 3: Return Record {[[size]]: size, [[highWaterMark]]: highWaterMark}.
    return true;
}

MOZ_MUST_USE bool
js::ReadableStreamReaderCancel(JSContext* cx, HandleObject readerObj, HandleValue reason)
{
    MOZ_ASSERT(IsReadableStreamReader(readerObj));
    RootedNativeObject reader(cx, &readerObj->as<NativeObject>());
    MOZ_ASSERT(StreamFromReader(reader));
    return ReadableStreamReaderGenericCancel(cx, reader, reason);
}

MOZ_MUST_USE bool
js::ReadableStreamReaderReleaseLock(JSContext* cx, HandleObject readerObj)
{
    MOZ_ASSERT(IsReadableStreamReader(readerObj));
    RootedNativeObject reader(cx, &readerObj->as<NativeObject>());
    MOZ_ASSERT(ReadableStreamGetNumReadRequests(StreamFromReader(reader)) == 0);
    return ReadableStreamReaderGenericRelease(cx, reader);
}

MOZ_MUST_USE bool
ReadableStream::enqueue(JSContext* cx, Handle<ReadableStream*> stream, HandleValue chunk)
{
    Rooted<ReadableStreamDefaultController*> controller(cx);
    controller = &ControllerFromStream(stream)->as<ReadableStreamDefaultController>();

    MOZ_ASSERT(!(ControllerFlags(controller) & ControllerFlag_CloseRequested));
    MOZ_ASSERT(stream->readable());

    return ReadableStreamDefaultControllerEnqueue(cx, controller, chunk);
}

MOZ_MUST_USE bool
ReadableStream::enqueueBuffer(JSContext* cx, Handle<ReadableStream*> stream,
                              Handle<ArrayBufferObject*> chunk)
{
    Rooted<ReadableByteStreamController*> controller(cx);
    controller = &ControllerFromStream(stream)->as<ReadableByteStreamController>();

    MOZ_ASSERT(!(ControllerFlags(controller) & ControllerFlag_CloseRequested));
    MOZ_ASSERT(stream->readable());

    return ReadableByteStreamControllerEnqueue(cx, controller, chunk);
}

void
ReadableStream::desiredSize(bool* hasSize, double* size) const
{
    if (errored()) {
        *hasSize = false;
        return;
    }

    *hasSize = true;

    if (closed()) {
        *size = 0;
        return;
    }

    NativeObject* controller = ControllerFromStream(this);
    *size = ReadableStreamControllerGetDesiredSizeUnchecked(controller);
}

/*static */ bool
ReadableStream::getExternalSource(JSContext* cx, Handle<ReadableStream*> stream, void** source)
{
    MOZ_ASSERT(stream->mode() == JS::ReadableStreamMode::ExternalSource);
    if (stream->locked()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_READABLESTREAM_LOCKED);
        return false;
    }
    if (!stream->readable()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAMCONTROLLER_NOT_READABLE,
                                  "ReadableStreamGetExternalUnderlyingSource");
        return false;
    }

    auto controller = &ControllerFromStream(stream)->as<ReadableByteStreamController>();
    AddControllerFlags(controller, ControllerFlag_SourceLocked);
    *source = controller->getFixedSlot(ControllerSlot_UnderlyingSource).toPrivate();
    return true;
}

void
ReadableStream::releaseExternalSource()
{
    MOZ_ASSERT(mode() == JS::ReadableStreamMode::ExternalSource);
    MOZ_ASSERT(locked());
    auto controller = ControllerFromStream(this);
    MOZ_ASSERT(ControllerFlags(controller) & ControllerFlag_SourceLocked);
    RemoveControllerFlags(controller, ControllerFlag_SourceLocked);
}

uint8_t
ReadableStream::embeddingFlags() const
{
    uint8_t flags = ControllerFlags(ControllerFromStream(this)) >> ControllerEmbeddingFlagsOffset;
    MOZ_ASSERT_IF(flags, mode() == JS::ReadableStreamMode::ExternalSource);
    return flags;
}

// Streams spec, 3.10.4.4. steps 1-3
// and
// Streams spec, 3.12.8. steps 8-9
//
// Adapted to handling updates signaled by the embedding for streams with
// external underlying sources.
//
// The remaining steps of those two functions perform checks and asserts that
// don't apply to streams with external underlying sources.
MOZ_MUST_USE bool
ReadableStream::updateDataAvailableFromSource(JSContext* cx, Handle<ReadableStream*> stream,
                                              uint32_t availableData)
{
    Rooted<ReadableByteStreamController*> controller(cx);
    controller = &ControllerFromStream(stream)->as<ReadableByteStreamController>();

    // Step 2: If this.[[closeRequested]] is true, throw a TypeError exception.
    if (ControllerFlags(controller) & ControllerFlag_CloseRequested) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAMCONTROLLER_CLOSED, "enqueue");
        return false;
    }

    // Step 3: If this.[[controlledReadableStream]].[[state]] is not "readable",
    //         throw a TypeError exception.
    if (!StreamFromController(controller)->readable()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAMCONTROLLER_NOT_READABLE, "enqueue");
        return false;
    }

    RemoveControllerFlags(controller, ControllerFlag_Pulling | ControllerFlag_PullAgain);

#if DEBUG
    uint32_t oldAvailableData = controller->getFixedSlot(QueueContainerSlot_TotalSize).toInt32();
#endif // DEBUG
    controller->setFixedSlot(QueueContainerSlot_TotalSize, Int32Value(availableData));

    // Step 8.a: If ! ReadableStreamGetNumReadRequests(stream) is 0,
    // Reordered because for externally-sourced streams it applies regardless
    // of reader type.
    if (ReadableStreamGetNumReadRequests(stream) == 0)
        return true;

    // Step 8: If ! ReadableStreamHasDefaultReader(stream) is true
    if (ReadableStreamHasDefaultReader(stream)) {
        // Step b: Otherwise,
        // Step i: Assert: controller.[[queue]] is empty.
        MOZ_ASSERT(oldAvailableData == 0);

        // Step ii: Let transferredView be
        //          ! Construct(%Uint8Array%, transferredBuffer, byteOffset, byteLength).
        JSObject* viewObj = JS_NewUint8Array(cx, availableData);
        Rooted<ArrayBufferViewObject*> transferredView(cx, &viewObj->as<ArrayBufferViewObject>());
        if (!transferredView)
            return false;

        Value val = controller->getFixedSlot(ControllerSlot_UnderlyingSource);
        void* underlyingSource = val.toPrivate();

        size_t bytesWritten;
        {
            JS::AutoCheckCannotGC noGC(cx);
            bool dummy;
            void* buffer = JS_GetArrayBufferViewData(transferredView, &dummy, noGC);
            auto cb = cx->runtime()->readableStreamWriteIntoReadRequestCallback;
            MOZ_ASSERT(cb);
            // TODO: use bytesWritten to correctly update the request's state.
            cb(cx, stream, underlyingSource, stream->embeddingFlags(), buffer,
               availableData, &bytesWritten);
        }

        // Step iii: Perform ! ReadableStreamFulfillReadRequest(stream, transferredView, false).
        RootedValue chunk(cx, ObjectValue(*transferredView));
        if (!ReadableStreamFulfillReadOrReadIntoRequest(cx, stream, chunk, false))
            return false;

        controller->setFixedSlot(QueueContainerSlot_TotalSize,
                                 Int32Value(availableData - bytesWritten));
    } else if (ReadableStreamHasBYOBReader(stream)) {
        // Step 9: Otherwise,
        // Step a: If ! ReadableStreamHasBYOBReader(stream) is true,
        // Step i: Perform
        // (Not needed for external underlying sources.)

        // Step ii: Perform ! ReadableByteStreamControllerProcessPullIntoDescriptorsUsingQueue(controller).
        if (!ReadableByteStreamControllerProcessPullIntoDescriptorsUsingQueue(cx, controller))
            return false;
    } else {
        // Step b: Otherwise,
        // Step i: Assert: ! IsReadableStreamLocked(stream) is false.
        MOZ_ASSERT(!stream->locked());

        // Step ii: Perform
        //          ! ReadableByteStreamControllerEnqueueChunkToQueue(controller,
        //                                                            transferredBuffer,
        //                                                            byteOffset,
        //                                                            byteLength).
        // (Not needed for external underlying sources.)
    }

    return true;
}

MOZ_MUST_USE bool
ReadableStream::close(JSContext* cx, Handle<ReadableStream*> stream)
{
    RootedNativeObject controllerObj(cx, ControllerFromStream(stream));
    if (!VerifyControllerStateForClosing(cx, controllerObj))
        return false;

    if (controllerObj->is<ReadableStreamDefaultController>()) {
        Rooted<ReadableStreamDefaultController*> controller(cx);
        controller = &controllerObj->as<ReadableStreamDefaultController>();
        return ReadableStreamDefaultControllerClose(cx, controller);
    }

    Rooted<ReadableByteStreamController*> controller(cx);
    controller = &controllerObj->as<ReadableByteStreamController>();
    return ReadableByteStreamControllerClose(cx, controller);
}

MOZ_MUST_USE bool
ReadableStream::error(JSContext* cx, Handle<ReadableStream*> stream, HandleValue reason)
{
    // Step 3: If stream.[[state]] is not "readable", throw a TypeError exception.
    if (!stream->readable()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAMCONTROLLER_NOT_READABLE, "error");
        return false;
    }

    // Step 4: Perform ! ReadableStreamDefaultControllerError(this, e).
    RootedNativeObject controller(cx, ControllerFromStream(stream));
    return ReadableStreamControllerError(cx, controller, reason);
}

MOZ_MUST_USE bool
ReadableStream::tee(JSContext* cx, Handle<ReadableStream*> stream, bool cloneForBranch2,
                    MutableHandle<ReadableStream*> branch1Stream,
                    MutableHandle<ReadableStream*> branch2Stream)
{
    return ReadableStreamTee(cx, stream, false, branch1Stream, branch2Stream);
}

MOZ_MUST_USE NativeObject*
ReadableStream::getReader(JSContext* cx, Handle<ReadableStream*> stream,
                          JS::ReadableStreamReaderMode mode)
{
    if (mode == JS::ReadableStreamReaderMode::Default)
        return CreateReadableStreamDefaultReader(cx, stream);
    return CreateReadableStreamBYOBReader(cx, stream);
}

JS_FRIEND_API(JSObject*)
js::UnwrapReadableStream(JSObject* obj)
{
    if (JSObject* unwrapped = CheckedUnwrap(obj))
        return unwrapped->is<ReadableStream>() ? unwrapped : nullptr;
    return nullptr;
}

namespace {

enum class StreamCompressionFormat {
    Deflate,
    DeflateRaw,
    Gzip
};

struct StreamCompressionContext
{
    z_stream mStream;
    StreamCompressionFormat mFormat;
    bool mCompress;
    bool mFinished;
    bool mInitialized;

    StreamCompressionContext(StreamCompressionFormat format, bool compress)
      : mFormat(format),
        mCompress(compress),
        mFinished(false),
        mInitialized(false)
    {
        memset(&mStream, 0, sizeof(mStream));
    }

    ~StreamCompressionContext()
    {
        if (!mInitialized)
            return;
        if (mCompress)
            deflateEnd(&mStream);
        else
            inflateEnd(&mStream);
    }
};

static int
WindowBitsForFormat(StreamCompressionFormat format)
{
    switch (format) {
      case StreamCompressionFormat::Deflate:
        return MAX_WBITS;
      case StreamCompressionFormat::DeflateRaw:
        return -MAX_WBITS;
      case StreamCompressionFormat::Gzip:
        return MAX_WBITS + 16;
    }
    MOZ_CRASH("Unknown compression stream format");
}

static bool
ParseCompressionFormat(JSContext* cx, HandleValue value,
                       StreamCompressionFormat* format)
{
    RootedString str(cx, ToString<CanGC>(cx, value));
    if (!str)
        return false;

    bool match = false;
    if (!JS_StringEqualsAscii(cx, str, "deflate", &match))
        return false;
    if (match) {
        *format = StreamCompressionFormat::Deflate;
        return true;
    }

    if (!JS_StringEqualsAscii(cx, str, "deflate-raw", &match))
        return false;
    if (match) {
        *format = StreamCompressionFormat::DeflateRaw;
        return true;
    }

    if (!JS_StringEqualsAscii(cx, str, "gzip", &match))
        return false;
    if (match) {
        *format = StreamCompressionFormat::Gzip;
        return true;
    }

    if (!JS_StringEqualsAscii(cx, str, "brotli", &match))
        return false;

    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_NOT_EXPECTED_TYPE,
                              "CompressionStream", "supported format",
                              match ? "unsupported brotli format" : "unknown format");
    return false;
}

static void
StreamCompressionContextFinalize(FreeOp* fop, JSObject* obj)
{
    auto* context = static_cast<StreamCompressionContext*>(JS_GetPrivate(obj));
    if (context)
        js_delete(context);
}

static const ClassOps StreamCompressionContextClassOps = {
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* getProperty */
    nullptr, /* setProperty */
    nullptr, /* enumerate */
    nullptr, /* resolve */
    nullptr, /* mayResolve */
    StreamCompressionContextFinalize,
    nullptr, /* call */
    nullptr, /* hasInstance */
    nullptr, /* construct */
    nullptr  /* trace */
};

static const Class StreamCompressionContextClass = {
    "StreamCompressionContext",
    JSCLASS_HAS_PRIVATE,
    &StreamCompressionContextClassOps
};

static bool
CreateStreamCompressionContext(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 2) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_MORE_ARGS_NEEDED,
                                  "__uxpCreateCompressionContext", "1", "s");
        return false;
    }

    StreamCompressionFormat format;
    if (!ParseCompressionFormat(cx, args.get(0), &format))
        return false;

    bool compress = ToBoolean(args.get(1));
    auto* context = js_new<StreamCompressionContext>(format, compress);
    if (!context) {
        ReportOutOfMemory(cx);
        return false;
    }

    int windowBits = WindowBitsForFormat(format);
    int zerr = compress
               ? deflateInit2(&context->mStream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                              windowBits, 8, Z_DEFAULT_STRATEGY)
               : inflateInit2(&context->mStream, windowBits);
    if (zerr != Z_OK) {
        js_delete(context);
        JS_ReportErrorASCII(cx, "Failed to initialize compression stream");
        return false;
    }
    context->mInitialized = true;

    RootedObject obj(cx, NewObjectWithGivenProto(cx, &StreamCompressionContextClass, nullptr));
    if (!obj) {
        js_delete(context);
        return false;
    }

    JS_SetPrivate(obj, context);
    args.rval().setObject(*obj);
    return true;
}

static bool
GetBufferSourceBytes(JSContext* cx, HandleValue value, Vector<uint8_t>* bytes)
{
    if (!value.isObject()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_NOT_EXPECTED_TYPE,
                                  "CompressionStream", "BufferSource",
                                  InformalValueTypeName(value));
        return false;
    }

    RootedObject obj(cx, &value.toObject());
    if (JS_IsArrayBufferViewObject(obj)) {
        uint32_t length = JS_GetArrayBufferViewByteLength(obj);
        if (length == 0)
            return true;

        JS::AutoCheckCannotGC nogc(cx);
        bool isShared;
        uint8_t* data = static_cast<uint8_t*>(JS_GetArrayBufferViewData(obj, &isShared, nogc));
        return bytes->append(data, length);
    }

    if (JS_IsArrayBufferObject(obj)) {
        uint32_t length = JS_GetArrayBufferByteLength(obj);
        if (length == 0)
            return true;

        JS::AutoCheckCannotGC nogc(cx);
        bool isShared;
        uint8_t* data = JS_GetArrayBufferData(obj, &isShared, nogc);
        return bytes->append(data, length);
    }

    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_NOT_EXPECTED_TYPE,
                              "CompressionStream", "BufferSource",
                              obj->getClass()->name);
    return false;
}

static bool
AppendUint8Array(JSContext* cx, HandleObject array, uint32_t* index,
                 const uint8_t* data, size_t length)
{
    if (length == 0)
        return true;

    if (length > UINT32_MAX) {
        ReportAllocationOverflow(cx);
        return false;
    }

    RootedObject view(cx, JS_NewUint8Array(cx, length));
    if (!view)
        return false;

    {
        JS::AutoCheckCannotGC nogc(cx);
        bool isShared;
        uint8_t* viewData = static_cast<uint8_t*>(JS_GetArrayBufferViewData(view, &isShared, nogc));
        memcpy(viewData, data, length);
    }

    RootedValue viewVal(cx, ObjectValue(*view));
    return JS_SetElement(cx, array, (*index)++, viewVal);
}

static bool
StreamCompressionProcess(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 3 || !args[0].isObject() ||
        !args[0].toObject().hasClass(&StreamCompressionContextClass)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_NOT_EXPECTED_TYPE,
                                  "__uxpCompressionProcess",
                                  "StreamCompressionContext", "other");
        return false;
    }

    auto* context =
        static_cast<StreamCompressionContext*>(JS_GetPrivate(&args[0].toObject()));
    if (!context) {
        JS_ReportErrorASCII(cx, "Compression stream is already finished");
        return false;
    }

    bool finish = ToBoolean(args.get(2));

    if (context->mFinished) {
        if (finish && args.get(1).isUndefined()) {
            RootedObject emptyChunks(cx, JS_NewArrayObject(cx, 0));
            if (!emptyChunks)
                return false;
            args.rval().setObject(*emptyChunks);
            return true;
        }
        JS_ReportErrorASCII(cx, "Compression stream is already finished");
        return false;
    }

    Vector<uint8_t> input(cx);
    if (!args.get(1).isUndefined() && !GetBufferSourceBytes(cx, args.get(1), &input))
        return false;

    RootedObject chunks(cx, JS_NewArrayObject(cx, 0));
    if (!chunks)
        return false;

    z_stream& stream = context->mStream;
    stream.next_in = input.empty() ? nullptr : input.begin();
    stream.avail_in = input.length();

    uint32_t chunkIndex = 0;
    const int flush = finish ? Z_FINISH : Z_NO_FLUSH;
    bool madeProgress;

    do {
        uint8_t output[16384];
        stream.next_out = output;
        stream.avail_out = sizeof(output);

        uint32_t beforeIn = stream.avail_in;
        uint32_t beforeOut = stream.avail_out;
        int zerr = context->mCompress ? deflate(&stream, flush) : inflate(&stream, flush);

        size_t produced = sizeof(output) - stream.avail_out;
        if (!AppendUint8Array(cx, chunks, &chunkIndex, output, produced))
            return false;

        madeProgress = produced > 0 || stream.avail_in != beforeIn ||
                       stream.avail_out != beforeOut;

        if (zerr == Z_STREAM_END) {
            context->mFinished = true;
            if (!context->mCompress && stream.avail_in != 0) {
                JS_ReportErrorASCII(cx, "Compressed input has trailing data");
                return false;
            }
            break;
        }

        if (zerr == Z_BUF_ERROR && !madeProgress)
            break;

        if (zerr != Z_OK && zerr != Z_BUF_ERROR) {
            JS_ReportErrorASCII(cx, stream.msg ? stream.msg : "Compression stream error");
            return false;
        }
    } while (stream.avail_out == 0 || stream.avail_in != 0 ||
             (finish && !context->mFinished && madeProgress));

    if (finish && !context->mFinished) {
        JS_ReportErrorASCII(cx, "Compressed input ended before the end of stream");
        return false;
    }

    args.rval().setObject(*chunks);
    return true;
}

bool
ReleaseReadableStreamReaderSilently(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setBoolean(true);

    if (!args.get(0).isObject())
        return true;

    RootedObject readerObj(cx, &args[0].toObject());
    if (!readerObj->is<ReadableStreamDefaultReader>() &&
        !readerObj->is<ReadableStreamBYOBReader>())
        return true;

    RootedNativeObject reader(cx, &readerObj->as<NativeObject>());
    if (!ReaderHasStream(reader))
        return true;

    Value requestsVal = reader->getFixedSlot(ReaderSlot_Requests);
    if (!requestsVal.isUndefined()) {
        NativeObject* requests = &requestsVal.toObject().as<NativeObject>();
        if (requests->getDenseInitializedLength() != 0) {
            args.rval().setBoolean(false);
            return true;
        }
    }

    Rooted<ReadableStream*> stream(cx, StreamFromReader(reader));
    MOZ_ASSERT(&stream->getFixedSlot(StreamSlot_Reader).toObject() == reader);

    RootedObject closedPromise(cx, PromiseObject::unforgeableResolve(cx, UndefinedHandleValue));
    if (!closedPromise)
        return false;
    reader->setFixedSlot(ReaderSlot_ClosedPromise, ObjectValue(*closedPromise));

    stream->setFixedSlot(StreamSlot_Reader, UndefinedValue());
    reader->setFixedSlot(ReaderSlot_Stream, UndefinedValue());
    return true;
}

} // anonymous namespace

bool
js::InitStreamExtras(JSContext* cx, HandleObject global)
{
    if (!JS_DefineFunction(cx, global, "__uxpCreateCompressionContext",
                           CreateStreamCompressionContext, 2, 0))
        return false;

    if (!JS_DefineFunction(cx, global, "__uxpCompressionProcess",
                           StreamCompressionProcess, 3, 0))
        return false;

    if (!JS_DefineFunction(cx, global, "__uxpReleaseReadableStreamReaderSilently",
                           ReleaseReadableStreamReaderSilently, 1, 0))
        return false;

    static const char source[] =
  R"JS(
(function(global) {
  "use strict";

  if (global.__uxpStreamsInitialized)
    return;
  Object.defineProperty(global, "__uxpStreamsInitialized",
                        { value: true, configurable: true });

  var objectToString = Object.prototype.toString;
  var hasOwn = Object.prototype.hasOwnProperty;

  function isObject(value) {
    return (typeof value === "object" && value !== null) ||
           typeof value === "function";
  }

  function requiredObject(value, name) {
    if (!isObject(value))
      throw new TypeError(name + " is not an object");
    return value;
  }

  function newPromiseCapability() {
    var resolve, reject;
    var promise = new Promise(function(res, rej) {
      resolve = res;
      reject = rej;
    });
    return { promise: promise, resolve: resolve, reject: reject };
  }

  function promiseCall(fn, receiver, args) {
    try {
      return Promise.resolve(fn.apply(receiver, args || []));
    } catch (e) {
      return Promise.reject(e);
    }
  }

  function getMethod(value, name) {
    var method = value[name];
    if (method === undefined || method === null)
      return undefined;
    if (typeof method !== "function")
      throw new TypeError(name + " is not callable");
    return method;
  }

  function makeDOMException(message, name) {
    if (typeof global.DOMException === "function")
      return new global.DOMException(message, name);
    var error = new Error(message);
    error.name = name;
    return error;
  }

  function define(name, value) {
    if (hasOwn.call(global, name) && global[name] !== undefined)
      return;
    Object.defineProperty(global, name, {
      value: value,
      writable: true,
      configurable: true
    });
  }

  var writableState = new WeakMap();
  var writerState = new WeakMap();
  var writableControllerState = new WeakMap();

  function isWritableStream(stream) {
    return writableState.has(stream);
  }

  function isWritableStreamDefaultWriter(writer) {
    return writerState.has(writer);
  }

  function rejectPromise(error) {
    return Promise.reject(error);
  }

  function silenceRejection(promise) {
    try {
      if (promise && typeof promise.catch === "function")
        promise.catch(function() {});
    } catch (e) {}
  }

  function releaseReaderLock(reader) {
    try {
      if (global.__uxpReleaseReadableStreamReaderSilently(reader))
        return;
    } catch (e) {}
    Promise.resolve().then(function() {
      try {
        if (global.__uxpReleaseReadableStreamReaderSilently(reader))
          return;
      } catch (e) {}
      try { silenceRejection(reader.closed); } catch (e) {}
      try { reader.releaseLock(); } catch (e) {}
    });
  }

  function releaseWriterLock(writer) {
    try { silenceRejection(writer.closed); } catch (e) {}
    try { silenceRejection(writer.ready); } catch (e) {}
    try { writer.releaseLock(); } catch (e) {}
  }

  function syncWritableWriterWithStream(writer, state) {
    var ws = writerState.get(writer);
    if (!ws || ws.stream === undefined)
      return;

    ws.readyPromise = state.readyPromise;
    if (state.state === "closed") {
      ws.closedPromise = Promise.resolve(undefined);
    } else if (state.state === "errored") {
      ws.closedPromise = Promise.reject(state.storedError);
      ws.closedPromise.catch(function() {});
    } else {
      ws.closedPromise = state.closedPromise;
    }
  }

  function writableError(stream, error) {
    var state = writableState.get(stream);
    if (!state || state.state === "closed" || state.state === "errored")
      return;
    state.state = "errored";
    state.storedError = error;
    state.ready.reject(error);
    state.closed.reject(error);
    state.readyPromise.catch(function() {});
    state.closedPromise.catch(function() {});
    if (state.writer)
      syncWritableWriterWithStream(state.writer, state);
  }

  function writableClose(stream) {
    var state = writableState.get(stream);
    if (!state || state.state === "closed")
      return;
    state.state = "closed";
    state.ready.resolve(undefined);
    state.closed.resolve(undefined);
    if (state.writer)
      syncWritableWriterWithStream(state.writer, state);
  }

  function writableStreamWrite(stream, chunk) {
    var state = writableState.get(stream);
    if (!state)
      return rejectPromise(new TypeError("WritableStream expected"));
    if (state.state === "closed")
      return rejectPromise(new TypeError("WritableStream is closed"));
    if (state.state === "errored")
      return rejectPromise(state.storedError);
    if (state.state === "closing")
      return rejectPromise(new TypeError("WritableStream is closing"));

    var size = 1;
    try {
      size = state.sizeAlgorithm(chunk);
      if (!Number.isFinite(size) || size < 0)
        throw new RangeError("Invalid chunk size");
    } catch (e) {
      writableError(stream, e);
      return rejectPromise(e);
    }

    state.queueTotalSize += size;
    if (state.queueTotalSize > state.highWaterMark && state.backpressure === false) {
      state.backpressure = true;
      state.ready = newPromiseCapability();
      state.readyPromise = state.ready.promise;
      if (state.writer)
        syncWritableWriterWithStream(state.writer, state);
    }

    var write = state.sink.write;
    var controller = state.controller;
    var writePromise = state.writeChain.then(function() {
      if (state.state === "errored")
        throw state.storedError;
      if (write)
        return write.call(state.sink, chunk, controller);
      return undefined;
    });
    state.writeChain = writePromise.then(function(value) {
      state.queueTotalSize -= size;
      if (state.queueTotalSize <= state.highWaterMark && state.backpressure) {
        state.backpressure = false;
        state.ready.resolve(undefined);
        state.ready = newPromiseCapability();
        state.ready.resolve(undefined);
        state.readyPromise = state.ready.promise;
        if (state.writer)
          syncWritableWriterWithStream(state.writer, state);
      }
      return value;
    }, function(error) {
      writableError(stream, error);
      throw error;
    });
    state.writeChain.catch(function() {});
    return writePromise;
  }

  function writableStreamClose(stream) {
    var state = writableState.get(stream);
    if (!state)
      return rejectPromise(new TypeError("WritableStream expected"));
    if (state.state === "closed")
      return rejectPromise(new TypeError("WritableStream is already closed"));
    if (state.state === "errored")
      return rejectPromise(state.storedError);
    if (state.state === "closing")
      return rejectPromise(new TypeError("WritableStream is already closing"));

    state.state = "closing";
    var close = state.sink.close;
    var closePromise = state.writeChain.then(function() {
      if (close)
        return close.call(state.sink);
      return undefined;
    }).then(function(value) {
      writableClose(stream);
      return value;
    }, function(error) {
      writableError(stream, error);
      throw error;
    });
    state.writeChain = closePromise.catch(function() {});
    return closePromise;
  }

  function writableStreamAbort(stream, reason) {
    var state = writableState.get(stream);
    if (!state)
      return rejectPromise(new TypeError("WritableStream expected"));
    if (state.state === "closed")
      return Promise.resolve(undefined);
    if (state.state === "errored")
      return rejectPromise(state.storedError);

    var abort = state.sink.abort;
    state.state = "errored";
    state.storedError = reason;
    if (state.abortController) {
      try { state.abortController.abort(); } catch (e) {}
    }
    state.ready.reject(reason);
    state.closed.reject(reason);
    state.readyPromise.catch(function() {});
    state.closedPromise.catch(function() {});
    if (state.writer)
      syncWritableWriterWithStream(state.writer, state);

    var abortPromise = abort ? promiseCall(abort, state.sink, [reason])
                             : Promise.resolve(undefined);
    return abortPromise;
  }

  function WritableStreamDefaultController() {
    throw new TypeError("Illegal constructor");
  }

  WritableStreamDefaultController.prototype.error = function(error) {
    var state = writableControllerState.get(this);
    if (!state)
      throw new TypeError("WritableStreamDefaultController expected");
    writableError(state.stream, error);
  };

  try {
    Object.defineProperty(WritableStreamDefaultController.prototype, "signal", {
      configurable: true,
      get: function() {
        var state = writableControllerState.get(this);
        if (!state)
          throw new TypeError("WritableStreamDefaultController expected");
        return state.abortController ? state.abortController.signal : undefined;
      }
    });
  } catch (e) {}

  Object.defineProperty(WritableStreamDefaultController.prototype,
                        Symbol.toStringTag,
                        { value: "WritableStreamDefaultController",
                          configurable: true });

  function WritableStream(underlyingSink, strategy) {
    if (!(this instanceof WritableStream))
      throw new TypeError("WritableStream must be constructed with new");

    underlyingSink = underlyingSink === undefined ? {} : requiredObject(underlyingSink, "underlyingSink");
    strategy = strategy === undefined ? {} : requiredObject(strategy, "strategy");

    if (underlyingSink.type !== undefined)
      throw new RangeError("Invalid writable stream type");

    var highWaterMark = strategy.highWaterMark === undefined ? 1 : Number(strategy.highWaterMark);
    if (Number.isNaN(highWaterMark) || highWaterMark < 0)
      throw new RangeError("Invalid highWaterMark");

    var sizeAlgorithm = strategy.size;
    if (sizeAlgorithm === undefined)
      sizeAlgorithm = function() { return 1; };
    if (typeof sizeAlgorithm !== "function")
      throw new TypeError("strategy.size is not callable");

    var controller = Object.create(WritableStreamDefaultController.prototype);
    var abortController = typeof global.AbortController === "function"
      ? new global.AbortController()
      : undefined;
    var ready = newPromiseCapability();
    ready.resolve(undefined);
    var closed = newPromiseCapability();
    var state = {
      sink: underlyingSink,
      controller: controller,
      signal: undefined,
      state: "writable",
      storedError: undefined,
      writer: undefined,
      abortController: abortController,
      writeChain: Promise.resolve(undefined),
      ready: ready,
      readyPromise: ready.promise,
      closed: closed,
      closedPromise: closed.promise,
      highWaterMark: highWaterMark,
      sizeAlgorithm: sizeAlgorithm,
      queueTotalSize: 0,
      backpressure: false
    };
    writableState.set(this, state);
    writableControllerState.set(controller, {
      stream: this,
      abortController: abortController
    });

    var start = underlyingSink.start;
    if (start !== undefined) {
      if (typeof start !== "function")
        throw new TypeError("underlyingSink.start is not callable");
      state.writeChain = promiseCall(start, underlyingSink, [controller]).catch(function(error) {
        writableError(this, error);
        throw error;
      }.bind(this));
      state.writeChain.catch(function() {});
    }
  }

  Object.defineProperty(WritableStream.prototype, "locked", {
    configurable: true,
    get: function() {
      var state = writableState.get(this);
      if (!state)
        throw new TypeError("WritableStream expected");
      return state.writer !== undefined;
    }
  });

  WritableStream.prototype.abort = function(reason) {
    if (!isWritableStream(this))
      return rejectPromise(new TypeError("WritableStream expected"));
    if (writableState.get(this).writer !== undefined)
      return rejectPromise(new TypeError("WritableStream is locked"));
    return writableStreamAbort(this, reason);
  };

  WritableStream.prototype.close = function() {
    if (!isWritableStream(this))
      return rejectPromise(new TypeError("WritableStream expected"));
    if (writableState.get(this).writer !== undefined)
      return rejectPromise(new TypeError("WritableStream is locked"));
    return writableStreamClose(this);
  };

  WritableStream.prototype.getWriter = function() {
    if (!isWritableStream(this))
      throw new TypeError("WritableStream expected");
    return new WritableStreamDefaultWriter(this);
  };

  Object.defineProperty(WritableStream.prototype, Symbol.toStringTag,
                        { value: "WritableStream", configurable: true });

  function WritableStreamDefaultWriter(stream) {
    if (!(this instanceof WritableStreamDefaultWriter))
      throw new TypeError("WritableStreamDefaultWriter must be constructed with new");
    if (!isWritableStream(stream))
      throw new TypeError("WritableStream expected");

    var state = writableState.get(stream);
    if (state.writer !== undefined)
      throw new TypeError("WritableStream is locked");

    state.writer = this;
    writerState.set(this, {
      stream: stream,
      readyPromise: state.readyPromise,
      closedPromise: state.closedPromise
    });
  }

  Object.defineProperty(WritableStreamDefaultWriter.prototype, "closed", {
    configurable: true,
    get: function() {
      var state = writerState.get(this);
      if (!state)
        return rejectPromise(new TypeError("WritableStreamDefaultWriter expected"));
      return state.closedPromise;
    }
  });

  Object.defineProperty(WritableStreamDefaultWriter.prototype, "desiredSize", {
    configurable: true,
    get: function() {
      var state = writerState.get(this);
      if (!state)
        throw new TypeError("WritableStreamDefaultWriter expected");
      if (state.stream === undefined)
        throw new TypeError("Writer lock is released");
      var streamState = writableState.get(state.stream);
      if (streamState.state === "errored")
        return null;
      if (streamState.state === "closed")
        return 0;
      return streamState.highWaterMark - streamState.queueTotalSize;
    }
  });

  Object.defineProperty(WritableStreamDefaultWriter.prototype, "ready", {
    configurable: true,
    get: function() {
      var state = writerState.get(this);
      if (!state)
        return rejectPromise(new TypeError("WritableStreamDefaultWriter expected"));
      return state.readyPromise;
    }
  });

  WritableStreamDefaultWriter.prototype.abort = function(reason) {
    var state = writerState.get(this);
    if (!state || state.stream === undefined)
      return rejectPromise(new TypeError("Writer lock is released"));
    return writableStreamAbort(state.stream, reason);
  };

  WritableStreamDefaultWriter.prototype.close = function() {
    var state = writerState.get(this);
    if (!state || state.stream === undefined)
      return rejectPromise(new TypeError("Writer lock is released"));
    return writableStreamClose(state.stream);
  };

  WritableStreamDefaultWriter.prototype.releaseLock = function() {
    var state = writerState.get(this);
    if (!state)
      throw new TypeError("WritableStreamDefaultWriter expected");
    if (state.stream === undefined)
      return;
    var streamState = writableState.get(state.stream);
    if (streamState.writer === this)
      streamState.writer = undefined;
    state.stream = undefined;
    state.readyPromise = rejectPromise(new TypeError("Writer lock is released"));
    state.closedPromise = rejectPromise(new TypeError("Writer lock is released"));
    state.readyPromise.catch(function() {});
    state.closedPromise.catch(function() {});
  };

  WritableStreamDefaultWriter.prototype.write = function(chunk) {
    var state = writerState.get(this);
    if (!state || state.stream === undefined)
      return rejectPromise(new TypeError("Writer lock is released"));
    return writableStreamWrite(state.stream, chunk);
  };

)JS"
R"JS(

  Object.defineProperty(WritableStreamDefaultWriter.prototype, Symbol.toStringTag,
                        { value: "WritableStreamDefaultWriter",
                          configurable: true });

  var transformState = new WeakMap();
  var transformControllerState = new WeakMap();

  function TransformStreamDefaultController() {
    throw new TypeError("Illegal constructor");
  }

  Object.defineProperty(TransformStreamDefaultController.prototype, "desiredSize", {
    configurable: true,
    get: function() {
      var state = transformControllerState.get(this);
      if (!state)
        throw new TypeError("TransformStreamDefaultController expected");
      return state.readableController.desiredSize;
    }
  });

  TransformStreamDefaultController.prototype.enqueue = function(chunk) {
    var state = transformControllerState.get(this);
    if (!state)
      throw new TypeError("TransformStreamDefaultController expected");
    state.readableController.enqueue(chunk);
  };

  TransformStreamDefaultController.prototype.error = function(reason) {
    var state = transformControllerState.get(this);
    if (!state)
      throw new TypeError("TransformStreamDefaultController expected");
    state.readableController.error(reason);
    writableError(state.stream.writable, reason);
  };

  TransformStreamDefaultController.prototype.terminate = function() {
    var state = transformControllerState.get(this);
    if (!state)
      throw new TypeError("TransformStreamDefaultController expected");
    try {
      state.readableController.close();
    } catch (e) {}
    writableError(state.stream.writable, new TypeError("TransformStream terminated"));
  };

  Object.defineProperty(TransformStreamDefaultController.prototype,
                        Symbol.toStringTag,
                        { value: "TransformStreamDefaultController",
                          configurable: true });

  function errorReadableController(controller, error) {
    try {
      controller.error(error);
    } catch (e) {}
  }

  function TransformStream(transformer, writableStrategy, readableStrategy) {
    if (!(this instanceof TransformStream))
      throw new TypeError("TransformStream must be constructed with new");

    transformer = transformer === undefined ? {} : requiredObject(transformer, "transformer");
    writableStrategy = writableStrategy === undefined ? {} : requiredObject(writableStrategy, "writableStrategy");
    readableStrategy = readableStrategy === undefined ? {} : requiredObject(readableStrategy, "readableStrategy");

    if (transformer.readableType !== undefined)
      throw new RangeError("Invalid readableType");
    if (transformer.writableType !== undefined)
      throw new RangeError("Invalid writableType");

    var controller = Object.create(TransformStreamDefaultController.prototype);
    var readableController;
    var stream = this;
    var startAlgorithm = getMethod(transformer, "start");
    var transformAlgorithm = getMethod(transformer, "transform");
    var flushAlgorithm = getMethod(transformer, "flush");
    var cancelAlgorithm = getMethod(transformer, "cancel");

    var readable = new ReadableStream({
      start: function(c) {
        readableController = c;
        transformControllerState.set(controller, {
          stream: stream,
          readableController: c
        });
      },
      cancel: function(reason) {
        if (cancelAlgorithm)
          return cancelAlgorithm.call(transformer, reason);
        return undefined;
      }
    }, readableStrategy);

    var startPromise = startAlgorithm
      ? promiseCall(startAlgorithm, transformer, [controller])
      : Promise.resolve(undefined);
    startPromise = startPromise.then(undefined, function(error) {
      if (readableController)
        errorReadableController(readableController, error);
      throw error;
    });

    var writable = new WritableStream({
      start: function() {
        return startPromise;
      },
      write: function(chunk) {
        var result;
        try {
          if (transformAlgorithm) {
            result = transformAlgorithm.call(transformer, chunk, controller);
          } else {
            controller.enqueue(chunk);
            result = undefined;
          }
        } catch (error) {
          errorReadableController(readableController, error);
          throw error;
        }
        return Promise.resolve(result).then(undefined, function(error) {
          errorReadableController(readableController, error);
          throw error;
        });
      },
      close: function() {
        var result = flushAlgorithm
          ? promiseCall(flushAlgorithm, transformer, [controller])
          : Promise.resolve(undefined);
        return result.then(function() {
          readableController.close();
        }, function(error) {
          errorReadableController(readableController, error);
          throw error;
        });
      },
      abort: function(reason) {
        readableController.error(reason);
      }
    }, writableStrategy);

    transformState.set(this, {
      readable: readable,
      writable: writable,
      controller: controller
    });
  }

  Object.defineProperty(TransformStream.prototype, "readable", {
    configurable: true,
    get: function() {
      var state = transformState.get(this);
      if (!state)
        throw new TypeError("TransformStream expected");
      return state.readable;
    }
  });

  Object.defineProperty(TransformStream.prototype, "writable", {
    configurable: true,
    get: function() {
      var state = transformState.get(this);
      if (!state)
        throw new TypeError("TransformStream expected");
      return state.writable;
    }
  });

  Object.defineProperty(TransformStream.prototype, Symbol.toStringTag,
                        { value: "TransformStream", configurable: true });

  function isReadableStreamObject(value) {
    if (!isObject(value) || !global.ReadableStream)
      return false;
    try {
      return value instanceof global.ReadableStream;
    } catch (e) {
      return false;
    }
  }

  function isUncloneableStreamChunk(value) {
    return isObject(value) &&
           (isWritableStream(value) ||
            isReadableStreamObject(value) ||
            transformState.has(value));
  }

  function makeDataCloneError() {
    return makeDOMException("The object could not be cloned.", "DataCloneError");
  }

  function cloneTransferReasonError(value) {
    var constructors = [
      global.EvalError,
      global.RangeError,
      global.ReferenceError,
      global.SyntaxError,
      global.TypeError,
      global.URIError,
      global.Error
    ];

    for (var i = 0; i < constructors.length; ++i) {
      var C = constructors[i];
      if (typeof C !== "function")
        continue;
      try {
        if (!(value instanceof C))
          continue;
      } catch (e) {
        continue;
      }

      var message = "";
      var descriptor;
      try {
        descriptor = Object.getOwnPropertyDescriptor(value, "message");
      } catch (e) {
        descriptor = undefined;
      }
      if (descriptor && hasOwn.call(descriptor, "value"))
        message = String(descriptor.value);
      return new C(message);
    }

    return undefined;
  }

  function cloneTransferReasonValue(value, memory) {
    if (typeof value === "function" || typeof value === "symbol")
      throw makeDataCloneError();
    if (!isObject(value))
      return value;
    if (isUncloneableStreamChunk(value))
      throw makeDataCloneError();

    memory = memory || new WeakMap();
    if (memory.has(value))
      return memory.get(value);

    if (typeof global.DOMException === "function") {
      try {
        if (value instanceof global.DOMException) {
          return new global.DOMException(String(value.message),
                                         String(value.name));
        }
      } catch (e) {}
    }

    var errorClone = cloneTransferReasonError(value);
    if (errorClone !== undefined)
      return errorClone;

    if (typeof global.ArrayBuffer === "function") {
      try {
        if (value instanceof global.ArrayBuffer) {
          var bufferClone = value.slice(0);
          memory.set(value, bufferClone);
          return bufferClone;
        }
      } catch (e) {}
    }

    if (global.ArrayBuffer && global.ArrayBuffer.isView &&
        global.ArrayBuffer.isView(value)) {
      var clonedBuffer = cloneTransferReasonValue(value.buffer, memory);
      var viewClone;
      if (objectToString.call(value) === "[object DataView]") {
        viewClone = new global.DataView(clonedBuffer, value.byteOffset,
                                        value.byteLength);
      } else {
        viewClone = new value.constructor(clonedBuffer, value.byteOffset,
                                          value.length);
      }
      memory.set(value, viewClone);
      return viewClone;
    }

    if (typeof global.Date === "function") {
      try {
        if (value instanceof global.Date) {
          var dateClone = new global.Date(value.getTime());
          memory.set(value, dateClone);
          return dateClone;
        }
      } catch (e) {}
    }

    if (typeof global.RegExp === "function") {
      try {
        if (value instanceof global.RegExp) {
          var flags = "";
          flags += value.global ? "g" : "";
          flags += value.ignoreCase ? "i" : "";
          flags += value.multiline ? "m" : "";
          flags += value.unicode ? "u" : "";
          flags += value.sticky ? "y" : "";
          var regexpClone = new global.RegExp(value.source, flags);
          regexpClone.lastIndex = value.lastIndex;
          memory.set(value, regexpClone);
          return regexpClone;
        }
      } catch (e) {}
    }

    if (typeof global.Map === "function") {
      try {
        if (value instanceof global.Map) {
          var mapClone = new global.Map();
          memory.set(value, mapClone);
          value.forEach(function(v, k) {
            mapClone.set(cloneTransferReasonValue(k, memory),
                         cloneTransferReasonValue(v, memory));
          });
          return mapClone;
        }
      } catch (e) {
        throw e;
      }
    }

    if (typeof global.Set === "function") {
      try {
        if (value instanceof global.Set) {
          var setClone = new global.Set();
          memory.set(value, setClone);
          value.forEach(function(v) {
            setClone.add(cloneTransferReasonValue(v, memory));
          });
          return setClone;
        }
      } catch (e) {
        throw e;
      }
    }

    var clone = Array.isArray(value) ? [] : {};
    memory.set(value, clone);

    var keys = Object.keys(value);
    for (var i = 0; i < keys.length; ++i) {
      var key = keys[i];
      clone[key] = cloneTransferReasonValue(value[key], memory);
    }

    return clone;
  }

  function abortTransferredWritable(record, reason) {
    try {
      return record.writer.abort(reason);
    } catch (e) {
      return Promise.reject(e);
    }
  }

  function rejectTransferredWritableWrite(record, error) {
    var abortPromise = abortTransferredWritable(record, error);
    silenceRejection(abortPromise);
    return Promise.reject(error);
  }

  function forwardTransferredWritableWrite(record, chunk) {
    if (isUncloneableStreamChunk(chunk))
      return rejectTransferredWritableWrite(record, makeDataCloneError());

    var prior = record.tail || Promise.resolve(undefined);
    var firstWrite = !record.started;
    record.started = true;

    var forwarding = prior.then(function() {
      var writePromise;
      try {
        writePromise = Promise.resolve(record.writer.write(chunk));
      } catch (e) {
        writePromise = Promise.reject(e);
      }
      record.inFlight = writePromise;
      silenceRejection(writePromise);
      return writePromise;
    });

    record.tail = forwarding;
    silenceRejection(forwarding);

    if (firstWrite)
      return undefined;

    return prior.then(function() { return undefined; });
  }

  function closeTransferredWritable(record) {
    var tail = record.tail || Promise.resolve(undefined);
    var closePromise = tail.then(function() {
      return record.writer.close();
    });
    record.tail = closePromise;
    silenceRejection(closePromise);
    return closePromise;
  }

  function __uxpTransferWritableStream(stream) {
    if (!isWritableStream(stream))
      return undefined;
    if (stream.locked)
      return null;
    try {
      return {
        writer: stream.getWriter(),
        started: false,
        inFlight: undefined,
        tail: undefined
      };
    } catch (e) {
      return null;
    }
  }

  function __uxpReceiveWritableStreamTransfer(record) {
    return new WritableStream({
      write: function(chunk) {
        return forwardTransferredWritableWrite(record, chunk);
      },
      close: function() {
        return closeTransferredWritable(record);
      },
      abort: function(reason) {
        return abortTransferredWritable(record, reason);
      }
    });
  }

  function __uxpTransferReadableStream(stream) {
    if (!isReadableStreamObject(stream))
      return undefined;
    if (stream.locked)
      return null;
    try {
      var record = {
        reader: stream.getReader(),
        pendingRead: undefined,
        pumping: undefined,
        closed: false,
        errored: false,
        canceled: false
      };
      ensureReadableTransferRead(record);
      return record;
    } catch (e) {
      return null;
    }
  }

  function ensureReadableTransferRead(record) {
    if (!record.pendingRead && !record.closed && !record.errored &&
        !record.canceled) {
      try {
        record.pendingRead = Promise.resolve(record.reader.read());
      } catch (e) {
        record.pendingRead = Promise.reject(e);
      }
      silenceRejection(record.pendingRead);
    }
    return record.pendingRead;
  }

  function errorReadableTransfer(record, controller, error) {
    if (record.canceled || record.closed || record.errored)
      return;
    record.errored = true;
    try { controller.error(error); } catch (e) {}
  }

  function pumpReadableTransfer(record, controller) {
    if (record.pumping || record.closed || record.errored || record.canceled)
      return;

    var pendingRead = ensureReadableTransferRead(record);
    if (!pendingRead)
      return;

    record.pumping = pendingRead.then(function(result) {
      record.pendingRead = undefined;
      record.pumping = undefined;

      if (record.canceled || record.errored || record.closed)
        return;

      if (result.done) {
        record.closed = true;
        try { controller.close(); } catch (e) {}
        return;
      }

      if (isUncloneableStreamChunk(result.value)) {
        var cloneError = makeDataCloneError();
        silenceRejection(record.reader.cancel(cloneError));
        errorReadableTransfer(record, controller, cloneError);
        return;
      }

      try {
        controller.enqueue(result.value);
      } catch (e) {
        silenceRejection(record.reader.cancel(e));
        errorReadableTransfer(record, controller, e);
        return;
      }

      if (controller.desiredSize > 0)
        pumpReadableTransfer(record, controller);
    }, function(error) {
      record.pendingRead = undefined;
      record.pumping = undefined;
      errorReadableTransfer(record, controller, error);
    });
    silenceRejection(record.pumping);
  }

  function cancelReadableTransfer(record, reason) {
    record.canceled = true;
    var clonedReason;
    try {
      clonedReason = cloneTransferReasonValue(reason);
    } catch (e) {
      return Promise.reject(e);
    }

    Promise.resolve().then(function() {
      try {
        silenceRejection(record.reader.cancel(clonedReason));
      } catch (e) {}
    });
    return undefined;
  }

  function closeReadableTransferPort(port) {
    try { port.close(); } catch (e) {}
  }

  function startReadableTransferPort(port) {
    try {
      if (port.start)
        port.start();
    } catch (e) {}
  }

  function tryPostReadableTransferPort(port, message) {
    try {
      port.postMessage(message);
      return true;
    } catch (e) {
      return false;
    }
  }

  function postReadableTransferPortError(record, error) {
    var posted = tryPostReadableTransferPort(record.port,
                                             { type: "error", value: error });
    if (!posted) {
      posted = tryPostReadableTransferPort(record.port, {
        type: "error",
        name: error && error.name,
        message: error && error.message
      });
    }
    record.closed = true;
    if (!posted)
      closeReadableTransferPort(record.port);
  }

)JS"
R"JS(

  function requestReadableTransferPortChunk(state) {
    if (state.closed || state.errored || state.canceled)
      return;
    if (!tryPostReadableTransferPort(state.port, { type: "pull" })) {
      state.errored = true;
      if (state.controller)
        try { state.controller.error(makeDataCloneError()); } catch (e) {}
      closeReadableTransferPort(state.port);
    }
  }

  function errorReadableTransferPortState(state, error) {
    if (state.errored)
      return;
    state.errored = true;
    try {
      if (state.controller)
        state.controller.error(error);
    } catch (e) {}
    closeReadableTransferPort(state.port);
  }

  function pumpReadableTransferPort(record) {
    if (record.reading || record.closed || record.canceled ||
        record.credits <= 0)
      return;

    --record.credits;
    record.reading = true;

    var readPromise;
    try {
      readPromise = Promise.resolve(record.reader.read());
    } catch (e) {
      readPromise = Promise.reject(e);
    }
    silenceRejection(readPromise);

    readPromise.then(function(result) {
      record.reading = false;

      if (record.canceled || record.closed)
        return;

      if (result.done) {
        record.closed = true;
        if (!tryPostReadableTransferPort(record.port, { type: "close" }))
          closeReadableTransferPort(record.port);
        return;
      }

      if (isUncloneableStreamChunk(result.value)) {
        var cloneError = makeDataCloneError();
        silenceRejection(record.reader.cancel(cloneError));
        postReadableTransferPortError(record, cloneError);
        return;
      }

      if (!tryPostReadableTransferPort(record.port,
                                       { type: "chunk", value: result.value })) {
        record.closed = true;
        closeReadableTransferPort(record.port);
        return;
      }

      pumpReadableTransferPort(record);
    }, function(error) {
      record.reading = false;
      if (record.canceled || record.closed)
        return;
      postReadableTransferPortError(record, error);
    });
  }

  function __uxpTransferReadableStreamPort(stream) {

)JS"
R"JS(
    if (!isReadableStreamObject(stream))
      return undefined;
    if (stream.locked || typeof global.MessageChannel !== "function")
      return null;

    var channel;
    try {
      channel = new global.MessageChannel();
      var record = {
        reader: stream.getReader(),
        port: channel.port1,
        reading: false,
        credits: 1,
        closed: false,
        canceled: false
      };

      record.port.onmessage = function(event) {
        var message = event.data;
        if (!message || record.canceled)
          return;

        if (message.type === "closed") {
          record.closed = true;
          closeReadableTransferPort(record.port);
          return;
        }

        if (record.closed)
          return;

        if (message.type === "pull") {
          ++record.credits;
          pumpReadableTransferPort(record);
          return;
        }

        if (message.type === "cancel") {
          record.canceled = true;
          Promise.resolve().then(function() {
            try {
              silenceRejection(record.reader.cancel(message.reason));
            } catch (e) {}
          });
          closeReadableTransferPort(record.port);
        }
      };

      startReadableTransferPort(record.port);
      pumpReadableTransferPort(record);
      return channel.port2;
    } catch (e) {
      if (channel) {
        closeReadableTransferPort(channel.port1);
        closeReadableTransferPort(channel.port2);
      }
      return null;
    }
  }

  function errorFromReadableTransferPortMessage(message) {
    if ("value" in message)
      return message.value;
    return makeDOMException(message.message || "ReadableStream transfer failed",
                            message.name || "DataCloneError");
  }

  function __uxpReceiveReadableStreamTransferFromPort(port) {
    var state = {
      port: port,
      controller: undefined,
      closed: false,
      errored: false,
      canceled: false
    };

    port.onmessage = function(event) {
      var message = event.data;
      if (!message || state.closed || state.errored || state.canceled)
        return;

      if (message.type === "chunk") {
        if (isUncloneableStreamChunk(message.value)) {
          errorReadableTransferPortState(state, makeDataCloneError());
          return;
        }

        try {
          state.controller.enqueue(message.value);
        } catch (e) {
          errorReadableTransferPortState(state, e);
          return;
        }

        if (state.controller.desiredSize > 0)
          requestReadableTransferPortChunk(state);
        return;
      }

      if (message.type === "close") {
        state.closed = true;
        try { state.controller.close(); } catch (e) {}
        tryPostReadableTransferPort(port, { type: "closed" });
        closeReadableTransferPort(port);
        return;
      }

      if (message.type === "error") {
        if (!state.errored) {
          state.errored = true;
          try {
            if (state.controller)
              state.controller.error(
                errorFromReadableTransferPortMessage(message));
          } catch (e) {}
        }
        tryPostReadableTransferPort(port, { type: "closed" });
        closeReadableTransferPort(port);
      }
    };

    return new ReadableStream({
      start: function(controller) {
        state.controller = controller;
        startReadableTransferPort(port);
        requestReadableTransferPortChunk(state);
      },
      pull: function() {
        requestReadableTransferPortChunk(state);
      },
      cancel: function(reason) {
        state.canceled = true;
        try {
          port.postMessage({ type: "cancel", reason: reason });
        } catch (e) {
          closeReadableTransferPort(port);
          return Promise.reject(e);
        }
        closeReadableTransferPort(port);
        return undefined;
      }
    });
  }

  function __uxpTransferTransformStream(stream) {
    if (!isObject(stream) || !transformState.has(stream))
      return undefined;

    var state = transformState.get(stream);
    if (state.readable.locked || state.writable.locked)
      return null;

    var readableRecord = __uxpTransferReadableStream(state.readable);
    if (!readableRecord)
      return null;

    var writableRecord = __uxpTransferWritableStream(state.writable);
    if (!writableRecord)
      return null;

    return {
      readable: readableRecord,
      writable: writableRecord
    };
  }

  function __uxpReceiveTransformStreamTransfer(record) {
    var stream = Object.create(global.TransformStream.prototype);
    transformState.set(stream, {
      readable: __uxpReceiveReadableStreamTransfer(record.readable),
      writable: __uxpReceiveWritableStreamTransfer(record.writable),
      controller: undefined
    });
    return stream;
  }

  function __uxpReceiveReadableStreamTransfer(record) {
    return new ReadableStream({
      start: function(controller) {
        pumpReadableTransfer(record, controller);
      },
      pull: function(controller) {
        pumpReadableTransfer(record, controller);
      },
      cancel: function(reason) {
        return cancelReadableTransfer(record, reason);
      }
    });
  }

  function pipeTo(readable, destination, options) {
    options = options === undefined ? {} : requiredObject(options, "options");
    var signal = options.signal;
    var preventClose = Boolean(options.preventClose);
    var preventAbort = Boolean(options.preventAbort);
    var preventCancel = Boolean(options.preventCancel);
    var reader;
    var writer;

    try {
      destination = requiredObject(destination, "destination");
      reader = readable.getReader();
      writer = destination.getWriter();
    } catch (e) {
      return rejectPromise(e);
    }

    var closed = false;
    function cleanup() {
      if (closed)
        return;
      closed = true;
      releaseReaderLock(reader);
      releaseWriterLock(writer);
      if (signal && signal.removeEventListener)
        signal.removeEventListener("abort", onAbort);
    }

    function abortReason() {
      return signal && "reason" in signal
             ? signal.reason
             : makeDOMException("The operation was aborted.", "AbortError");
    }

    function shutdownWithAction(action, originalError) {
      return action().then(function() {
        cleanup();
        throw originalError;
      }, function(actionError) {
        cleanup();
        throw actionError;
      });
    }

    function onAbort() {}

    var pipePromise = new Promise(function(resolve, reject) {
      onAbort = function() {
        var reason = abortReason();
        var actions = [];
        if (!preventAbort)
          actions.push(writer.abort(reason));
        if (!preventCancel)
          actions.push(reader.cancel(reason));
        Promise.all(actions).then(function() {
          cleanup();
          reject(reason);
        }, function(error) {
          cleanup();
          reject(error);
        });
      };

      if (signal && signal.aborted) {
        onAbort();
        return;
      }
      if (signal && signal.addEventListener)
        signal.addEventListener("abort", onAbort);

      function pump() {
        if (closed)
          return;
        reader.read().then(function(result) {
          if (closed)
            return;
          if (result.done) {
            var closePromise = preventClose ? Promise.resolve(undefined)
                                           : writer.close();
            closePromise.then(function() {
              cleanup();
              resolve(undefined);
            }, function(error) {
              cleanup();
              reject(error);
            });
            return;
          }

          writer.write(result.value).then(pump, function(error) {
            if (closed)
              return;
            var cancel = preventCancel
              ? Promise.resolve(undefined)
              : reader.cancel(error);
            cancel.then(function() {
              cleanup();
              reject(error);
            }, function(cancelError) {
              cleanup();
              reject(cancelError);
            });
          });
        }, function(error) {
          if (closed)
            return;
          var abort = preventAbort
            ? Promise.resolve(undefined)
            : writer.abort(error);
          abort.then(function() {
            cleanup();
            reject(error);
          }, function(abortError) {
            cleanup();
            reject(abortError);
          });
        });
      }

      pump();
    });

    return pipePromise;
  }

  function pipeThrough(readable, pair, options) {
    pair = requiredObject(pair, "transform");
    var writable = pair.writable;
    var output = pair.readable;
    if (readable.locked)
      throw new TypeError("ReadableStream is locked");
    if (!isWritableStream(writable))
      throw new TypeError("transform.writable must be a WritableStream");
    if (!(output instanceof global.ReadableStream))
      throw new TypeError("transform.readable must be a ReadableStream");
    if (writable.locked)
      throw new TypeError("WritableStream is locked");
    readable.pipeTo(writable, options).catch(function() {});
    return output;
  }

  Object.defineProperty(global, "__uxpStreamsPipeTo",
                        { value: pipeTo, configurable: true });
  Object.defineProperty(global, "__uxpStreamsPipeThrough",
                        { value: pipeThrough, configurable: true });
  Object.defineProperty(global, "__uxpTransferWritableStream",
                        { value: __uxpTransferWritableStream,
                          configurable: true });
  Object.defineProperty(global, "__uxpReceiveWritableStreamTransfer",
                        { value: __uxpReceiveWritableStreamTransfer,
                          configurable: true });
  Object.defineProperty(global, "__uxpTransferReadableStream",
                        { value: __uxpTransferReadableStream,
                          configurable: true });
  Object.defineProperty(global, "__uxpReceiveReadableStreamTransfer",
                        { value: __uxpReceiveReadableStreamTransfer,
                          configurable: true });
  Object.defineProperty(global, "__uxpTransferReadableStreamPort",
                        { value: __uxpTransferReadableStreamPort,
                          configurable: true });
  Object.defineProperty(global, "__uxpReceiveReadableStreamTransferFromPort",
                        { value: __uxpReceiveReadableStreamTransferFromPort,
                          configurable: true });
  Object.defineProperty(global, "__uxpTransferTransformStream",
                        { value: __uxpTransferTransformStream,
                          configurable: true });
  Object.defineProperty(global, "__uxpReceiveTransformStreamTransfer",
                        { value: __uxpReceiveTransformStreamTransfer,
                          configurable: true });

  if (global.ReadableStream && !global.ReadableStream.from) {
    Object.defineProperty(global.ReadableStream, "from", {
      value: function from(asyncIterable) {
        if (asyncIterable == null)
          throw new TypeError("ReadableStream.from requires an iterable");
        var method = asyncIterable[Symbol.asyncIterator] ||
                     asyncIterable[Symbol.iterator];
        if (typeof method !== "function")
          throw new TypeError("ReadableStream.from requires an iterable");
        var iterator = method.call(asyncIterable);
        return new ReadableStream({
          pull: function(controller) {
            return Promise.resolve(iterator.next()).then(function(result) {
              if (!isObject(result))
                throw new TypeError("Iterator result is not an object");
              if (result.done) {
                controller.close();
              } else {
                controller.enqueue(result.value);
              }
            });
          },
          cancel: function(reason) {
            var returnMethod = iterator.return;
            if (returnMethod)
              return returnMethod.call(iterator, reason);
            return undefined;
          }
        });
      },
      writable: true,
      configurable: true
    });
  }

  if (global.ReadableStream && typeof Symbol === "function" &&
      Symbol.asyncIterator && !global.ReadableStream.prototype.values) {
    function ReadableStreamAsyncIterator(stream, preventCancel) {
      this._reader = stream.getReader();
      this._preventCancel = preventCancel;
      this._done = false;
    }

    ReadableStreamAsyncIterator.prototype.next = function() {
      if (this._done)
        return Promise.resolve({ value: undefined, done: true });
      var self = this;
      return this._reader.read().then(function(result) {
        if (result.done) {
          self._done = true;
          releaseReaderLock(self._reader);
        }
        return result;
      }, function(error) {
        self._done = true;
        releaseReaderLock(self._reader);
        throw error;
      });
    };

    ReadableStreamAsyncIterator.prototype.return = function(value) {
      if (this._done)
        return Promise.resolve({ value: value, done: true });
      this._done = true;
      var reader = this._reader;
      if (this._preventCancel) {
        releaseReaderLock(reader);
        return Promise.resolve({ value: value, done: true });
      }
      return reader.cancel(value).then(function() {
        releaseReaderLock(reader);
        return { value: value, done: true };
      }, function(error) {
        releaseReaderLock(reader);
        throw error;
      });
    };

    ReadableStreamAsyncIterator.prototype[Symbol.asyncIterator] = function() {
      return this;
    };

    Object.defineProperty(global.ReadableStream.prototype, "values", {
      value: function values(options) {
        var preventCancel = Boolean(options && options.preventCancel);
        return new ReadableStreamAsyncIterator(this, preventCancel);
      },
      writable: true,
      configurable: true
    });

    Object.defineProperty(global.ReadableStream.prototype, Symbol.asyncIterator, {
      value: global.ReadableStream.prototype.values,
      writable: true,
      configurable: true
    });
  }

  )JS"
  R"JS(

  var textEncoderStreamState = new WeakMap();
  var textDecoderStreamState = new WeakMap();

  function enqueueEncoded(controller, encoder, string) {
    if (string.length === 0)
      return;
    var chunk = encoder.encode(string);
    if (chunk.byteLength)
      controller.enqueue(chunk);
  }

  function TextEncoderStream() {
    if (!(this instanceof TextEncoderStream))
      throw new TypeError("TextEncoderStream must be constructed with new");

    var encoder = new TextEncoder();
    var leadingSurrogate = null;
    var transform = new TransformStream({
      transform: function(chunk, controller) {
        var input = String(chunk);
        var output = "";
        var i = 0;

        if (leadingSurrogate !== null) {
          if (input.length > 0) {
            var first = input.charCodeAt(0);
            if (first >= 0xDC00 && first <= 0xDFFF) {
              output += String.fromCharCode(leadingSurrogate) + input.charAt(0);
              i = 1;
            } else {
              output += "\uFFFD";
            }
          } else {
            return;
          }
          leadingSurrogate = null;
        }

        for (; i < input.length; ++i) {
          var code = input.charCodeAt(i);
          if (code >= 0xD800 && code <= 0xDBFF) {
            if (i + 1 < input.length) {
              var next = input.charCodeAt(i + 1);
              if (next >= 0xDC00 && next <= 0xDFFF) {
                output += input.charAt(i) + input.charAt(i + 1);
                ++i;
              } else {
                output += "\uFFFD";
              }
            } else {
              leadingSurrogate = code;
              break;
            }
          } else if (code >= 0xDC00 && code <= 0xDFFF) {
            output += "\uFFFD";
          } else {
            output += input.charAt(i);
          }
        }

        enqueueEncoded(controller, encoder, output);
      },
      flush: function(controller) {
        if (leadingSurrogate !== null) {
          leadingSurrogate = null;
          enqueueEncoded(controller, encoder, "\uFFFD");
        }
      }
    });

    textEncoderStreamState.set(this, { transform: transform });
  }

  Object.defineProperty(TextEncoderStream.prototype, "encoding", {
    configurable: true,
    get: function() {
      if (!textEncoderStreamState.has(this))
        throw new TypeError("TextEncoderStream expected");
      return "utf-8";
    }
  });

  Object.defineProperty(TextEncoderStream.prototype, "readable", {
    configurable: true,
    get: function() {
      var state = textEncoderStreamState.get(this);
      if (!state)
        throw new TypeError("TextEncoderStream expected");
      return state.transform.readable;
    }
  });

  Object.defineProperty(TextEncoderStream.prototype, "writable", {
    configurable: true,
    get: function() {
      var state = textEncoderStreamState.get(this);
      if (!state)
        throw new TypeError("TextEncoderStream expected");
      return state.transform.writable;
    }
  });

  Object.defineProperty(TextEncoderStream.prototype, Symbol.toStringTag,
                        { value: "TextEncoderStream", configurable: true });

  function TextDecoderStream(label, options) {
    if (!(this instanceof TextDecoderStream))
      throw new TypeError("TextDecoderStream must be constructed with new");
    label = label === undefined ? "utf-8" : label;
    options = options === undefined ? {} : options;

    var decoder = new TextDecoder(label, options);
    var transform = new TransformStream({
      transform: function(chunk, controller) {
        var output = decoder.decode(chunk, { stream: true });
        if (output.length)
          controller.enqueue(output);
      },
      flush: function(controller) {
        var output = decoder.decode();
        if (output.length)
          controller.enqueue(output);
      }
    });

    textDecoderStreamState.set(this, {
      transform: transform,
      encoding: decoder.encoding,
      fatal: decoder.fatal,
      ignoreBOM: Boolean(options && options.ignoreBOM)
    });
  }

  Object.defineProperty(TextDecoderStream.prototype, "encoding", {
    configurable: true,
    get: function() {
      var state = textDecoderStreamState.get(this);
      if (!state)
        throw new TypeError("TextDecoderStream expected");
      return state.encoding;
    }
  });

  Object.defineProperty(TextDecoderStream.prototype, "fatal", {
    configurable: true,
    get: function() {
      var state = textDecoderStreamState.get(this);
      if (!state)
        throw new TypeError("TextDecoderStream expected");
      return state.fatal;
    }
  });

  Object.defineProperty(TextDecoderStream.prototype, "ignoreBOM", {
    configurable: true,
    get: function() {
      var state = textDecoderStreamState.get(this);
      if (!state)
        throw new TypeError("TextDecoderStream expected");
      return state.ignoreBOM;
    }
  });

  Object.defineProperty(TextDecoderStream.prototype, "readable", {
    configurable: true,
    get: function() {
      var state = textDecoderStreamState.get(this);
      if (!state)
        throw new TypeError("TextDecoderStream expected");
      return state.transform.readable;
    }
  });

  Object.defineProperty(TextDecoderStream.prototype, "writable", {
    configurable: true,
    get: function() {
      var state = textDecoderStreamState.get(this);
      if (!state)
        throw new TypeError("TextDecoderStream expected");
      return state.transform.writable;
    }
  });

  Object.defineProperty(TextDecoderStream.prototype, Symbol.toStringTag,
                        { value: "TextDecoderStream", configurable: true });

  var compressionStreamState = new WeakMap();
  var decompressionStreamState = new WeakMap();

  function enqueueCompressionChunks(nativeContext, chunk, finish, controller) {
    var chunks = global.__uxpCompressionProcess(nativeContext, chunk, finish);
    for (var i = 0; i < chunks.length; ++i)
      controller.enqueue(chunks[i]);
  }

  function CompressionStream(format) {
    if (!(this instanceof CompressionStream))
      throw new TypeError("CompressionStream must be constructed with new");
    var nativeContext = global.__uxpCreateCompressionContext(format, true);
    var transform = new TransformStream({
      transform: function(chunk, controller) {
        enqueueCompressionChunks(nativeContext, chunk, false, controller);
      },
      flush: function(controller) {
        enqueueCompressionChunks(nativeContext, undefined, true, controller);
      }
    });
    compressionStreamState.set(this, { transform: transform });
  }

  Object.defineProperty(CompressionStream.prototype, "readable", {
    configurable: true,
    get: function() {
      var state = compressionStreamState.get(this);
      if (!state)
        throw new TypeError("CompressionStream expected");
      return state.transform.readable;
    }
  });

  Object.defineProperty(CompressionStream.prototype, "writable", {
    configurable: true,
    get: function() {
      var state = compressionStreamState.get(this);
      if (!state)
        throw new TypeError("CompressionStream expected");
      return state.transform.writable;
    }
  });

  Object.defineProperty(CompressionStream.prototype, Symbol.toStringTag,
                        { value: "CompressionStream", configurable: true });

  function DecompressionStream(format) {
    if (!(this instanceof DecompressionStream))
      throw new TypeError("DecompressionStream must be constructed with new");
    var nativeContext = global.__uxpCreateCompressionContext(format, false);
    var transform = new TransformStream({
      transform: function(chunk, controller) {
        enqueueCompressionChunks(nativeContext, chunk, false, controller);
      },
      flush: function(controller) {
        enqueueCompressionChunks(nativeContext, undefined, true, controller);
      }
    });
    decompressionStreamState.set(this, { transform: transform });
  }

  Object.defineProperty(DecompressionStream.prototype, "readable", {
    configurable: true,
    get: function() {
      var state = decompressionStreamState.get(this);
      if (!state)
        throw new TypeError("DecompressionStream expected");
      return state.transform.readable;
    }
  });

  Object.defineProperty(DecompressionStream.prototype, "writable", {
    configurable: true,
    get: function() {
      var state = decompressionStreamState.get(this);
      if (!state)
        throw new TypeError("DecompressionStream expected");
      return state.transform.writable;
    }
  });

  Object.defineProperty(DecompressionStream.prototype, Symbol.toStringTag,
                        { value: "DecompressionStream", configurable: true });

  define("WritableStream", WritableStream);
  define("WritableStreamDefaultWriter", WritableStreamDefaultWriter);
  define("WritableStreamDefaultController", WritableStreamDefaultController);
  define("TransformStream", TransformStream);
  define("TransformStreamDefaultController", TransformStreamDefaultController);
  define("TextEncoderStream", TextEncoderStream);
  define("TextDecoderStream", TextDecoderStream);
  define("CompressionStream", CompressionStream);
  define("DecompressionStream", DecompressionStream);
})(this);
)JS";

    RootedValue rval(cx);
    JS::CompileOptions options(cx);
    options.setFileAndLine("builtin/StreamExtras.js", 1);
    options.setNoScriptRval(true);
    return JS::Evaluate(cx, options, source, strlen(source), &rval);
}
