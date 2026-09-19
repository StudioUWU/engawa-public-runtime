/*
 * Copyright (C) 2026 EngawaRuntime contributors.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/IntRect.h>
#include <WebCore/PlatformExportMacros.h>
#include <cstdint>
#include <memory>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class LocalFrame;
class EngawaInProcessChromeClient;
class ImageBuffer;
class KeyboardEvent;
class Page;
class PixelBuffer;

// This is a deliberately narrow vertical proof, not the public SDK boundary.
// It owns no thread: every call must run on the process-lifetime owner thread
// which first creates an instance (the EngawaRuntime worker on each host).
struct EngawaInProcessPhysicalDamageRect {
    int32_t x { 0 };
    int32_t y { 0 };
    uint32_t width { 0 };
    uint32_t height { 0 };
};

struct EngawaInProcessBGRAFrame {
    uint32_t width { 0 };
    uint32_t height { 0 };
    uint32_t rowBytes { 0 };
    const uint8_t* pixels { nullptr };
    size_t pixelByteCount { 0 };
    std::shared_ptr<const void> pixelOwner;
    EngawaInProcessPhysicalDamageRect dirtyRect;
    bool opacityKnown { false };
    bool fullyOpaque { false };
    uint64_t damageTransaction { 0 };
};

enum class EngawaInProcessCursorType : uint8_t {
    Default,
    None,
    ContextMenu,
    Help,
    Pointer,
    Progress,
    Wait,
    Cell,
    Crosshair,
    Text,
    VerticalText,
    Alias,
    Copy,
    Move,
    NoDrop,
    NotAllowed,
    Grab,
    Grabbing,
    AllScroll,
    ColumnResize,
    RowResize,
    NorthResize,
    EastResize,
    SouthResize,
    WestResize,
    NorthEastResize,
    NorthWestResize,
    SouthEastResize,
    SouthWestResize,
    EastWestResize,
    NorthSouthResize,
    NorthEastSouthWestResize,
    NorthWestSouthEastResize,
    ZoomIn,
    ZoomOut,
};

enum class EngawaInProcessMouseEventType : uint8_t {
    Move,
    Down,
    Up,
    Enter,
    Leave,
};

enum class EngawaInProcessMouseButton : uint8_t {
    None,
    Left,
    Middle,
    Right,
    Back,
    Forward,
};

enum class EngawaInProcessWheelDeltaMode : uint8_t {
    Pixel,
    Line,
    Page,
};

enum class EngawaInProcessWheelPhase : uint8_t {
    None,
    Began,
    Changed,
    Ended,
    Cancelled,
};

enum class EngawaInProcessKeyEventType : uint8_t {
    Down,
    Up,
};

enum class EngawaInProcessKeyLocation : uint8_t {
    Standard,
    Left,
    Right,
    Numpad,
};

enum EngawaInProcessInputModifier : uint32_t {
    EngawaInProcessShiftModifier = 1u << 0,
    EngawaInProcessControlModifier = 1u << 1,
    EngawaInProcessAltModifier = 1u << 2,
    EngawaInProcessMetaModifier = 1u << 3,
    EngawaInProcessCapsLockModifier = 1u << 4,
    EngawaInProcessAltGraphModifier = 1u << 5,
    EngawaInProcessNumLockModifier = 1u << 6,
};

struct EngawaInProcessMouseEvent {
    EngawaInProcessMouseEventType type { EngawaInProcessMouseEventType::Move };
    double x { 0 };
    double y { 0 };
    EngawaInProcessMouseButton button { EngawaInProcessMouseButton::None };
    uint32_t buttons { 0 };
    uint32_t modifiers { 0 };
    uint32_t clickCount { 0 };
};

struct EngawaInProcessWheelEvent {
    // The stable ABI supplies logical CSS-pixel coordinates as finite floats.
    // Preserve the fraction until the final PlatformWheelEvent boundary.
    double x { 0 };
    double y { 0 };
    float deltaX { 0 };
    float deltaY { 0 };
    EngawaInProcessWheelDeltaMode deltaMode { EngawaInProcessWheelDeltaMode::Pixel };
    EngawaInProcessWheelPhase phase { EngawaInProcessWheelPhase::None };
    uint32_t modifiers { 0 };
    bool hasPreciseDeltas { false };
    bool isMomentum { false };
};

struct EngawaInProcessKeyEvent {
    EngawaInProcessKeyEventType type { EngawaInProcessKeyEventType::Down };
    String code;
    String key;
    EngawaInProcessKeyLocation location { EngawaInProcessKeyLocation::Standard };
    uint32_t modifiers { 0 };
    bool isRepeat { false };
};

struct EngawaInProcessViewState {
    bool invalidated { false };
    EngawaInProcessCursorType cursor { EngawaInProcessCursorType::Default };
    bool editableFocus { false };
};

enum class EngawaInProcessBundleReadPurpose : uint8_t {
    MainDocument,
    Subresource,
};

enum class EngawaInProcessBundleReadMethod : uint8_t {
    Get,
    Head,
};

enum class EngawaInProcessBundleReadStatus : uint8_t {
    Success,
    InvalidRequest,
    NotFound,
    Unavailable,
};

struct EngawaInProcessBundleResource {
    String mount;
    String mimeType;
    String textEncoding;
    uint64_t contentLength { 0 };
    Vector<uint8_t> bytes;
};

using EngawaInProcessBundleReadFunction = EngawaInProcessBundleReadStatus (*)(
    void*, const String& url, const String& requiredMount,
    EngawaInProcessBundleReadPurpose, EngawaInProcessBundleReadMethod,
    EngawaInProcessBundleResource&);

// Private, synchronous bridge to the runtime-owned immutable bundle index.
// The context must outlive the prototype and every call occurs on the
// prototype's owner thread.
struct EngawaInProcessBundleProvider {
    void* context { nullptr };
    EngawaInProcessBundleReadFunction read { nullptr };

    explicit operator bool() const { return context && read; }
};

using EngawaInProcessPageMessageFunction = bool (*)(void*, const String& json);

struct EngawaInProcessPageMessageSink {
    void* context { nullptr };
    EngawaInProcessPageMessageFunction post { nullptr };

    explicit operator bool() const { return context && post; }
};

using EngawaInProcessClipboardReadFunction = bool (*)(void*, String& text);
using EngawaInProcessClipboardWriteFunction = bool (*)(void*, const String& text);

struct EngawaInProcessClipboardProvider {
    void* context { nullptr };
    EngawaInProcessClipboardReadFunction readText { nullptr };
    EngawaInProcessClipboardWriteFunction writeText { nullptr };

    bool canRead() const { return context && readText; }
    bool canWrite() const { return context && writeText; }
};

struct EngawaInProcessViewConfiguration {
    uint32_t logicalWidth { 0 };
    uint32_t logicalHeight { 0 };
    float deviceScale { 1 };
    uint32_t maxFramesPerSecond { 60 };
    bool transparent { false };
    EngawaInProcessBundleProvider bundleProvider;
    EngawaInProcessPageMessageSink pageMessageSink;
    EngawaInProcessClipboardProvider clipboardProvider;
};

class WEBCORE_EXPORT EngawaInProcessWebCorePrototype final {
public:
    // A pump call advances at most this many run-loop work units. A unit is
    // one due timer callback, one dispatched function, or one queued platform
    // message; the embedder chooses its budget by the number of pump calls.
    static constexpr uint32_t maximumWorkUnitsPerPump = 1;

    static std::unique_ptr<EngawaInProcessWebCorePrototype> create(uint32_t width, uint32_t height, String& error);
    static std::unique_ptr<EngawaInProcessWebCorePrototype> create(uint32_t width, uint32_t height, const EngawaInProcessBundleProvider&, String& error);
    static std::unique_ptr<EngawaInProcessWebCorePrototype> create(const EngawaInProcessViewConfiguration&, String& error);
    ~EngawaInProcessWebCorePrototype();

    EngawaInProcessWebCorePrototype(const EngawaInProcessWebCorePrototype&) = delete;
    EngawaInProcessWebCorePrototype& operator=(const EngawaInProcessWebCorePrototype&) = delete;

    // Replaces the current main document with trusted inline content. Without
    // a bundle provider, every subresource remains disabled.
    bool loadTrustedHTML(const String& html, const String& baseURL, String& error);
    bool loadTrustedHTML(const String& html, const String& baseURL,
        bool& replacementStarted, String& error);

    // Reads one HTML main document through the provider and pins this page to
    // its mount before parsing. CSS, JavaScript, and images from that same
    // mount then use normal SubresourceLoader delivery; all other URLs fail.
    bool loadTrustedBundleURL(const String& url, String& error);
    bool loadTrustedBundleURL(const String& url, bool& replacementStarted,
        String& error);

    // Evaluates a JavaScript expression and returns JSON.stringify(result).
    bool evaluateExpressionToJSON(const String& expression, String& json, String& error);

    // Backward-compatible direct-prototype convenience used by the bounded
    // scheduler proof. Runtime integration uses the complete event methods.
    bool dispatchLeftClick(int x, int y, String& error);

    bool setFocus(bool focused, String& error);
    bool dispatchMouse(const EngawaInProcessMouseEvent&, String& error);
    bool dispatchWheel(const EngawaInProcessWheelEvent&, String& error);
    bool dispatchKey(const EngawaInProcessKeyEvent&, String& error);
    bool insertText(const String&, String& error);
    bool dispatchHostMessage(const String& json, String& error);
    bool stopLoading(String& error);

    bool resize(uint32_t width, uint32_t height, float deviceScale, String& error);
    bool resize(uint32_t width, uint32_t height, String& error) { return resize(width, height, m_deviceScale, error); }
    bool setMaxFramesPerSecond(uint32_t, String& error);

    // Leases an immutable premultiplied BGRA8/sRGB view of the persistent CPU
    // surface. The transaction must be committed after publication or rolled
    // back after any failed publication attempt.
    bool snapshotBGRA(EngawaInProcessBGRAFrame&, String& error);
    bool canSnapshot() const;
    bool commitSnapshot(uint64_t damageTransaction, String& error);
    bool rollbackSnapshot(uint64_t damageTransaction, String& error);

    // Performs one bounded, nonblocking run-loop turn on the owner thread. It
    // never drains a timer, function, or message backlog; no thread is created.
    // A queued WM_QUIT is left intact and reported as a failed pump on Windows.
    bool pumpOnce(String& error);
    bool pumpOnce(bool& didWork, String& error);

    // Monotonically identifies invalidation requests made on this owner
    // thread. The in-process adapter uses this as an O(1) visual boundary
    // while advancing a bounded batch of otherwise one-unit pump turns.
    static uint64_t currentThreadInvalidationGeneration();

    // Terminal teardown for the process-lifetime owner thread. Every
    // prototype must already be destroyed and no later WebCore work may run.
    // This mirrors WebCore worker shutdown so thread-local font/timer state is
    // dismantled before the owning native thread exits.
    static void destroyCurrentThreadState();

    EngawaInProcessViewState viewState() const;
    void clearInvalidation();

    bool isOwnerThread() const;

private:
    friend class EngawaInProcessChromeClient;

    EngawaInProcessWebCorePrototype(const EngawaInProcessViewConfiguration&, uint32_t physicalWidth, uint32_t physicalHeight, uint32_t ownerThreadID);
    bool initializePage(String& error);
    bool installPageBridge(String& error);
    bool checkOwnerAndLoaded(String& error) const;
    void handleKeyboardEvent(KeyboardEvent&);
    bool writeTrustedDocument(const String& html, const String& baseURL,
        const String& mount, bool& replacementStarted, String& error);
    void renderingUpdateRequested();
    void didInvalidate(const IntRect&);
    void forceFullDamage();
    void resetSnapshotSurface();
    bool ensureSnapshotSurface(String& error);
    EngawaInProcessPhysicalDamageRect physicalDamageRect(const IntRect&) const;
    bool updateOpacityForDamage(const EngawaInProcessPhysicalDamageRect&, const uint8_t* pixels, size_t rowBytes, String& error);
    void didChangeCursor(EngawaInProcessCursorType);
    void didChangeEditableFocus(bool);

    RefPtr<Page> m_page;
    RefPtr<LocalFrame> m_frame;
    RefPtr<ImageBuffer> m_snapshotSurface;
    // Cocoa ImageBuffer readback returns a copy. Keep one complete CPU image
    // and patch only the damaged backing-store rectangle on later frames.
    RefPtr<PixelBuffer> m_snapshotPixelBuffer;
    std::weak_ptr<const void> m_snapshotPixelOwner;
    IntRect m_pendingLogicalDamage;
    IntRect m_activeLogicalDamage;
    Vector<uint8_t> m_opacityTiles;
    uint32_t m_logicalWidth { 0 };
    uint32_t m_logicalHeight { 0 };
    uint32_t m_physicalWidth { 0 };
    uint32_t m_physicalHeight { 0 };
    float m_deviceScale { 1 };
    uint32_t m_maxFramesPerSecond { 60 };
    uint32_t m_ownerThreadID { 0 };
    EngawaInProcessBundleProvider m_bundleProvider;
    EngawaInProcessPageMessageSink m_pageMessageSink;
    EngawaInProcessClipboardProvider m_clipboardProvider;
    const void* m_pageBridgeContext { nullptr };
    uint32_t m_pressedMouseButtons { 0 };
    uint32_t m_cancelledMouseButtons { 0 };
    double m_lastMouseX { 0 };
    double m_lastMouseY { 0 };
    uint32_t m_lastMouseModifiers { 0 };
    uint64_t m_evaluationGeneration { 0 };
    uint64_t m_nextDamageTransaction { 0 };
    uint64_t m_activeDamageTransaction { 0 };
    uint32_t m_opacityTileColumns { 0 };
    uint32_t m_opacityTileRows { 0 };
    uint32_t m_knownOpacityTileCount { 0 };
    uint32_t m_opaqueTileCount { 0 };
    EngawaInProcessCursorType m_cursor { EngawaInProcessCursorType::Default };
    bool m_transparent { false };
    bool m_focused { true };
    bool m_editableFocus { false };
    bool m_invalidated { true };
    bool m_renderingUpdatePending { true };
    bool m_inRenderingUpdate { false };
#if USE(SKIA) && PLATFORM(WPE)
    bool m_fullDamageWasForced { false };
#endif
    bool m_loaded { false };
};

} // namespace WebCore
