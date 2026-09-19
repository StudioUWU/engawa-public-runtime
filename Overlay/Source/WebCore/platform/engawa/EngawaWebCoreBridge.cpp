/*
 * Copyright (C) 2026 EngawaRuntime contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This implementation is part of the public, replaceable WebCore library.
 * The C ABI performs marshaling only. Host policy, queues, bundle storage,
 * scheduling, and presentation remain in the caller.
 */
#include "config.h"
#include "EngawaInProcessWebCorePrototype.h"
#include "engawa_webcore.h"
#include <wtf/text/CString.h>
#include <memory>
#include <span>

using namespace WebCore;

struct ERWC_ViewImpl {
    ERWC_Callbacks callbacks { };
    std::unique_ptr<EngawaInProcessWebCorePrototype> prototype;
};

namespace {
ERWC_Bytes bytes(const CString& value) { return { value.data(), value.length() }; }
bool decode(ERWC_Bytes input, String& output)
{
    if (input.size && !input.data) return false;
    output = String::fromUTF8(std::span<const char>(
        input.data ? static_cast<const char*>(input.data) : "", input.size));
    return !output.isNull() || !input.size;
}
template<typename T> void ERWC_CALL release(void* pointer) { delete static_cast<T*>(pointer); }
struct ResponseStorage { CString value, error; };
void respond(ERWC_Response* output, bool success, const String& error,
    bool changed = false, const String& value = { })
{
    *output = { };
    output->success = success;
    output->changed = changed;
    // Most input and pump responses have no strings. Avoid an allocation on
    // that hot path while retaining independent ownership for actual text.
    if (error.isEmpty() && value.isEmpty())
        return;
    auto storage = std::make_unique<ResponseStorage>();
    storage->value = value.utf8(); storage->error = error.utf8();
    *output = { static_cast<uint32_t>(success), static_cast<uint32_t>(changed),
        bytes(storage->value), bytes(storage->error), storage.get(), release<ResponseStorage> };
    storage.release();
}
template<typename T> struct Owned {
    T value { };
    ~Owned() { if (value.release) value.release(value.owner); }
};
EngawaInProcessBundleReadStatus readBundle(void* context, const String& url,
    const String& requiredMount, EngawaInProcessBundleReadPurpose purpose,
    EngawaInProcessBundleReadMethod method, EngawaInProcessBundleResource& output)
{
    auto& callbacks = static_cast<ERWC_ViewImpl*>(context)->callbacks;
    Owned<ERWC_Resource> resource;
    auto urlBytes = url.utf8(); auto mountBytes = requiredMount.utf8();
    auto status = callbacks.read_bundle(callbacks.context, bytes(urlBytes), bytes(mountBytes),
        static_cast<uint32_t>(purpose), static_cast<uint32_t>(method), &resource.value);
    if (status > static_cast<uint32_t>(EngawaInProcessBundleReadStatus::Unavailable))
        return EngawaInProcessBundleReadStatus::Unavailable;
    if (status) return static_cast<EngawaInProcessBundleReadStatus>(status);
    const auto& input = resource.value;
    if (!decode(input.mount, output.mount) || !decode(input.mime_type, output.mimeType)
        || !decode(input.text_encoding, output.textEncoding)
        || (input.bytes.size && !input.bytes.data)
        || !output.bytes.tryAppend(std::span<const uint8_t>(
            static_cast<const uint8_t*>(input.bytes.data), input.bytes.size)))
        return EngawaInProcessBundleReadStatus::Unavailable;
    output.contentLength = input.content_length;
    return EngawaInProcessBundleReadStatus::Success;
}
bool postMessage(void* context, const String& message)
{
    auto& callbacks = static_cast<ERWC_ViewImpl*>(context)->callbacks;
    auto text = message.utf8();
    return !!callbacks.post_message(callbacks.context, bytes(text));
}
bool readClipboard(void* context, String& output)
{
    auto& callbacks = static_cast<ERWC_ViewImpl*>(context)->callbacks;
    Owned<ERWC_OwnedBytes> text;
    return callbacks.read_clipboard(callbacks.context, &text.value)
        && decode(text.value.bytes, output);
}
bool writeClipboard(void* context, const String& input)
{
    auto& callbacks = static_cast<ERWC_ViewImpl*>(context)->callbacks;
    auto text = input.utf8();
    return !!callbacks.write_clipboard(callbacks.context, bytes(text));
}
ERWC_View ERWC_CALL create(const ERWC_ViewConfig* input, ERWC_Response* response)
{
    if (!response) return nullptr;
    *response = { };
    if (!input || input->struct_size < sizeof(ERWC_ViewConfig)) return nullptr;
    auto view = std::make_unique<ERWC_ViewImpl>();
    view->callbacks = input->callbacks;
    EngawaInProcessViewConfiguration config;
    config.logicalWidth = input->logical_width; config.logicalHeight = input->logical_height;
    config.deviceScale = input->device_scale; config.maxFramesPerSecond = input->max_frames_per_second;
    config.transparent = !!input->transparent;
    if (view->callbacks.read_bundle) config.bundleProvider = { view.get(), readBundle };
    if (view->callbacks.post_message) config.pageMessageSink = { view.get(), postMessage };
    config.clipboardProvider = { view.get(),
        view->callbacks.read_clipboard ? readClipboard : nullptr,
        view->callbacks.write_clipboard ? writeClipboard : nullptr };
    String error;
    view->prototype = EngawaInProcessWebCorePrototype::create(config, error);
    respond(response, !!view->prototype, error);
    return view->prototype ? view.release() : nullptr;
}
void ERWC_CALL destroy(ERWC_View view) { delete view; }
void ERWC_CALL execute(ERWC_View view, const ERWC_Command* input, ERWC_Response* response)
{
    if (!response) return;
    *response = { };
    if (!view || !input || input->struct_size < sizeof(ERWC_Command)) return;
    String first, second, error, value;
    if (!decode(input->first, first) || !decode(input->second, second)) return;
    auto& engine = *view->prototype;
    bool success = false, changed = false;
    switch (input->operation) {
    case ERWC_LOAD_HTML: success = engine.loadTrustedHTML(first, second, changed, error); break;
    case ERWC_LOAD_BUNDLE: success = engine.loadTrustedBundleURL(first, changed, error); break;
    case ERWC_EVALUATE: success = engine.evaluateExpressionToJSON(first, value, error); break;
    case ERWC_FOCUS: success = engine.setFocus(!!input->value, error); break;
    case ERWC_MOUSE: {
        if (input->type > 4 || input->button > 5) break;
        EngawaInProcessMouseEvent event;
        event.type = static_cast<EngawaInProcessMouseEventType>(input->type);
        event.button = static_cast<EngawaInProcessMouseButton>(input->button);
        event.x = input->x; event.y = input->y; event.buttons = input->buttons;
        event.modifiers = input->modifiers; event.clickCount = input->click_count;
        success = engine.dispatchMouse(event, error); break;
    }
    case ERWC_WHEEL: {
        if (input->delta_mode > 2 || input->phase > 4 || input->flags > 3) break;
        EngawaInProcessWheelEvent event;
        event.x = input->x; event.y = input->y; event.deltaX = input->delta_x; event.deltaY = input->delta_y;
        event.deltaMode = static_cast<EngawaInProcessWheelDeltaMode>(input->delta_mode);
        event.phase = static_cast<EngawaInProcessWheelPhase>(input->phase);
        event.modifiers = input->modifiers; event.hasPreciseDeltas = input->flags & 1; event.isMomentum = input->flags & 2;
        success = engine.dispatchWheel(event, error); break;
    }
    case ERWC_KEY: {
        if (input->type > 1 || input->location > 3 || input->flags > 1) break;
        EngawaInProcessKeyEvent event;
        event.type = static_cast<EngawaInProcessKeyEventType>(input->type);
        event.location = static_cast<EngawaInProcessKeyLocation>(input->location);
        event.code = first; event.key = second; event.modifiers = input->modifiers; event.isRepeat = !!input->flags;
        success = engine.dispatchKey(event, error); break;
    }
    case ERWC_INSERT_TEXT: success = engine.insertText(first, error); break;
    case ERWC_POST_MESSAGE: success = engine.dispatchHostMessage(first, error); break;
    case ERWC_STOP: success = engine.stopLoading(error); break;
    case ERWC_RESIZE: success = engine.resize(input->width, input->height, input->device_scale, error); break;
    case ERWC_FRAME_RATE: success = engine.setMaxFramesPerSecond(input->value, error); break;
    case ERWC_COMMIT_FRAME: success = engine.commitSnapshot(input->transaction, error); break;
    case ERWC_ROLLBACK_FRAME: success = engine.rollbackSnapshot(input->transaction, error); break;
    case ERWC_PUMP: success = engine.pumpOnce(changed, error); break;
    default: break;
    }
    respond(response, success, error, changed, value);
}
void ERWC_CALL snapshot(ERWC_View view, ERWC_Frame* output, ERWC_Response* response)
{
    if (!response) return;
    *response = { };
    if (!output) return;
    *output = { };
    if (!view) return;
    String error;
    EngawaInProcessBGRAFrame frame;
    bool success = view->prototype->snapshotBGRA(frame, error);
    if (success) {
        output->width = frame.width; output->height = frame.height; output->row_bytes = frame.rowBytes;
        output->pixels = frame.pixels; output->pixel_byte_count = frame.pixelByteCount;
        output->damage_x = frame.dirtyRect.x; output->damage_y = frame.dirtyRect.y;
        output->damage_width = frame.dirtyRect.width; output->damage_height = frame.dirtyRect.height;
        output->opacity_known = frame.opacityKnown; output->fully_opaque = frame.fullyOpaque;
        output->transaction = frame.damageTransaction;
        if (frame.pixelOwner) {
            output->owner = new std::shared_ptr<const void>(WTF::move(frame.pixelOwner));
            output->release = release<std::shared_ptr<const void>>;
        }
    }
    respond(response, success, error);
}
ERWC_ViewState ERWC_CALL viewState(ERWC_View view)
{
    if (!view) return { };
    auto state = view->prototype->viewState();
    return { state.invalidated, static_cast<uint32_t>(state.cursor), state.editableFocus };
}
void ERWC_CALL clearInvalidation(ERWC_View view) { if (view) view->prototype->clearInvalidation(); }
uint64_t ERWC_CALL generation() { return EngawaInProcessWebCorePrototype::currentThreadInvalidationGeneration(); }
void ERWC_CALL destroyThread() { EngawaInProcessWebCorePrototype::destroyCurrentThreadState(); }
const ERWC_API api { sizeof(ERWC_API), ERWC_ABI_VERSION, create, destroy, execute,
    snapshot, viewState, clearInvalidation, generation, destroyThread };
}
extern "C" ERWC_EXPORT uint32_t ERWC_CALL er_webcore_get_api(uint32_t version,
    uint32_t minimumTableSize, const ERWC_API** output)
{
    if (!output) return 0;
    *output = nullptr;
    if (version != ERWC_ABI_VERSION || minimumTableSize > sizeof(ERWC_API)) return 0;
    *output = &api;
    return 1;
}
