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

#include "config.h"
#include "EngawaInProcessWebCorePrototype.h"

#include "BlobRegistry.h"
#include "Color.h"
#include "CommonVM.h"
#include "Cursor.h"
#include "DOMWrapperWorld.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "DocumentPage.h"
#include "DocumentView.h"
#include "DocumentWriter.h"
#include "DisplayRefreshMonitor.h"
#include "DisplayRefreshMonitorFactory.h"
#include "Editor.h"
#include "Element.h"
#include "EmptyClients.h"
#include "EventHandler.h"
#include "FocusController.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameLoader.h"
#include "FrameSnapshotting.h"
#include "GraphicsContext.h"
#include "GraphicsContextStateSaver.h"
#include "HandleUserInputEventResult.h"
#include "HostWindow.h"
#include "ImageBuffer.h"
#include "KeyboardEvent.h"
#include "LegacySchemeRegistry.h"
#include "LoaderStrategy.h"
#include "LocalFrame.h"
#include "LocalFrameInlines.h"
#include "LocalFrameView.h"
#include "MediaStrategy.h"
#if USE(SKIA)
#include "NativeImage.h"
#endif
#include "Node.h"
#include "NodeDocument.h"
#include "Page.h"
#include "PageConfiguration.h"
#include "PasteboardItemInfo.h"
#include "PasteboardStrategy.h"
#include "PixelBuffer.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "PlatformStrategies.h"
#include "ResourceError.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "ScriptController.h"
#include "ScrollingCoordinatorTypes.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include "SubresourceLoader.h"
#include "ThreadGlobalData.h"
#include "ThreadTimers.h"
#if defined(ER_INPROCESS_EMBEDDED_ENGINE) && PLATFORM(WIN)
#include "WebCoreInstanceHandle.h"
#endif
#include "WebCoreMainThread.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JavaScript.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/OpaqueJSString.h>
#include <pal/SessionID.h>
#if USE(SKIA)
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkPixmap.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#endif
#include <wtf/CanMakeWeakPtr.h>
#include <wtf/CurrentThread.h>
#include <wtf/HashMap.h>
#include <wtf/HexNumber.h>
#include <wtf/Lock.h>
#include <wtf/MonotonicTime.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/Seconds.h>
#include <wtf/SetForScope.h>
#include <wtf/StdLibExtras.h>
#include <wtf/URL.h>
#include <wtf/text/StringBuilder.h>

#if USE(GLIB_EVENT_LOOP)
#include <glib.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace WebCore {

using namespace WTF::StringLiterals;

class EngawaInProcessFramePixelOwner final {
public:
#if USE(SKIA)
    explicit EngawaInProcessFramePixelOwner(RefPtr<NativeImage>&& image)
        : m_image(WTF::move(image))
    {
    }
#else
    explicit EngawaInProcessFramePixelOwner(RefPtr<PixelBuffer>&& pixelBuffer)
        : m_pixelBuffer(WTF::move(pixelBuffer))
    {
    }
#endif

private:
#if USE(SKIA)
    RefPtr<NativeImage> m_image;
#else
    RefPtr<PixelBuffer> m_pixelBuffer;
#endif
};

namespace {

thread_local uint64_t inProcessInvalidationGeneration;
thread_local uint32_t liveInProcessPrototypeCount;
thread_local bool inProcessRuntimeInitialized;
#if USE(GLIB_EVENT_LOOP)
thread_local bool inProcessGLibPumpActive;
#endif
constexpr uint32_t maximumEngawaFramesPerSecond = 240;
constexpr PlatformDisplayID engawaDisplayIDPrefix = 0x45520000u;

#if defined(ER_ENABLE_WATERMARK)
// Volatile reads make the exact visible label a retained binary byte marker as
// well as render input. Package verification can therefore reject a stale or
// mismatched runtime binary instead of trusting metadata alone.
const volatile char engawaFrameWatermarkText[] =
    "EngawaRuntime - " ER_WATERMARK_COMMIT_SUFFIX;
constexpr size_t engawaFrameWatermarkTextLength = sizeof(engawaFrameWatermarkText) - 1;
constexpr int engawaWatermarkGlyphWidth = 5;
constexpr int engawaWatermarkGlyphHeight = 7;
constexpr int engawaWatermarkGlyphAdvance = 6;
constexpr uint8_t engawaWatermarkAlpha = 217;

static_assert(engawaFrameWatermarkTextLength == 21);

constexpr std::array<uint8_t, engawaWatermarkGlyphHeight> engawaWatermarkGlyph(char character)
{
    switch (character) {
    case '0': return { 0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e };
    case '1': return { 0x04, 0x0c, 0x14, 0x04, 0x04, 0x04, 0x1f };
    case '2': return { 0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f };
    case '3': return { 0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e };
    case '4': return { 0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02 };
    case '5': return { 0x1f, 0x10, 0x10, 0x1e, 0x01, 0x01, 0x1e };
    case '6': return { 0x06, 0x08, 0x10, 0x1e, 0x11, 0x11, 0x0e };
    case '7': return { 0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 };
    case '8': return { 0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e };
    case '9': return { 0x0e, 0x11, 0x11, 0x0f, 0x01, 0x02, 0x0c };
    case 'E': return { 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f };
    case 'R': return { 0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11 };
    case 'a': return { 0x00, 0x0e, 0x01, 0x0f, 0x11, 0x13, 0x0d };
    case 'b': return { 0x10, 0x10, 0x1e, 0x11, 0x11, 0x11, 0x1e };
    case 'c': return { 0x00, 0x0f, 0x10, 0x10, 0x10, 0x10, 0x0f };
    case 'd': return { 0x01, 0x01, 0x0f, 0x11, 0x11, 0x11, 0x0f };
    case 'e': return { 0x00, 0x0e, 0x11, 0x1f, 0x10, 0x10, 0x0f };
    case 'f': return { 0x06, 0x08, 0x08, 0x1e, 0x08, 0x08, 0x08 };
    case 'g': return { 0x00, 0x0f, 0x11, 0x0f, 0x01, 0x11, 0x0e };
    case 'i': return { 0x04, 0x00, 0x0c, 0x04, 0x04, 0x04, 0x0e };
    case 'm': return { 0x00, 0x1a, 0x15, 0x15, 0x15, 0x15, 0x15 };
    case 'n': return { 0x00, 0x1e, 0x11, 0x11, 0x11, 0x11, 0x11 };
    case 't': return { 0x08, 0x08, 0x1e, 0x08, 0x08, 0x09, 0x06 };
    case 'u': return { 0x00, 0x11, 0x11, 0x11, 0x11, 0x13, 0x0d };
    case 'w': return { 0x00, 0x11, 0x11, 0x11, 0x15, 0x15, 0x0a };
    case '-': return { 0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00 };
    default: return { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    }
}

constexpr int engawaWatermarkTextWidth()
{
    return static_cast<int>(engawaFrameWatermarkTextLength) * engawaWatermarkGlyphAdvance - 1;
}

bool engawaWatermarkTextPixel(int column, int row)
{
    if (column < 0 || row < 0 || row >= engawaWatermarkGlyphHeight
        || column >= engawaWatermarkTextWidth())
        return false;
    const auto characterIndex = static_cast<size_t>(column / engawaWatermarkGlyphAdvance);
    const int glyphColumn = column % engawaWatermarkGlyphAdvance;
    if (glyphColumn >= engawaWatermarkGlyphWidth)
        return false;
    const auto rows = engawaWatermarkGlyph(engawaFrameWatermarkText[characterIndex]);
    return rows[static_cast<size_t>(row)] & (1u << (engawaWatermarkGlyphWidth - glyphColumn - 1));
}

bool engawaWatermarkOutlinePixel(int column, int row)
{
    if (engawaWatermarkTextPixel(column, row))
        return false;
    for (int rowOffset = -1; rowOffset <= 1; ++rowOffset) {
        for (int columnOffset = -1; columnOffset <= 1; ++columnOffset) {
            if (engawaWatermarkTextPixel(column + columnOffset, row + rowOffset))
                return true;
        }
    }
    return false;
}

void paintEngawaWatermarkLabel(GraphicsContext& context, const FloatRect& clip,
    int textOriginX, int textOriginY, int scale)
{
    GraphicsContextStateSaver stateSaver { context };
    context.clip(clip);
    const Color lightInk { SRGBA<uint8_t> { 255, 255, 255, engawaWatermarkAlpha } };
    const Color darkInk { SRGBA<uint8_t> { 0, 0, 0, engawaWatermarkAlpha } };
    for (int row = -1; row <= engawaWatermarkGlyphHeight; ++row) {
        for (int column = -1; column <= engawaWatermarkTextWidth(); ++column) {
            const bool textPixel = engawaWatermarkTextPixel(column, row);
            if (!textPixel && !engawaWatermarkOutlinePixel(column, row))
                continue;
            context.fillRect({
                static_cast<float>(textOriginX + column * scale),
                static_cast<float>(textOriginY + row * scale),
                static_cast<float>(scale),
                static_cast<float>(scale)
            }, textPixel ? lightInk : darkInk);
        }
    }
}

void paintEngawaFrameWatermarks(ImageBuffer& buffer, const IntSize& logicalSize,
    const IntRect& logicalDamage)
{
    if (logicalSize.isEmpty() || logicalDamage.isEmpty())
        return;

    const int scale = std::clamp((logicalSize.width() + 240) / 480, 1, 8);
    const int margin = std::max(8, scale * 3);
    const int labelWidth = (engawaWatermarkTextWidth() + 2) * scale;
    const int labelHeight = (engawaWatermarkGlyphHeight + 2) * scale;
    const int topX = std::min(margin, std::max(0, logicalSize.width() - labelWidth));
    const int bottomX = std::max(0, logicalSize.width() - margin - labelWidth);
    const int topY = std::min(margin, std::max(0, logicalSize.height() - labelHeight));
    const int bottomY = std::max(0, logicalSize.height() - margin - labelHeight);
    const IntRect topLabelBounds { topX, topY, labelWidth, labelHeight };
    const IntRect bottomLabelBounds { bottomX, bottomY, labelWidth, labelHeight };
    const auto alignedLogicalDamage = encloseRectToDevicePixels(
        FloatRect { logicalDamage }, buffer.resolutionScale());
    const bool paintTopLabel = FloatRect { topLabelBounds }.intersects(alignedLogicalDamage);
    const bool paintBottomLabel = FloatRect { bottomLabelBounds }.intersects(alignedLogicalDamage);
    if (!paintTopLabel && !paintBottomLabel)
        return;

    auto& context = buffer.context();
    GraphicsContextStateSaver stateSaver { context };
    context.clip(alignedLogicalDamage);

    FloatRect topClip { 0, 0, static_cast<float>(logicalSize.width()),
        static_cast<float>(logicalSize.height()) };
    auto bottomClip = topClip;
    if (topLabelBounds.intersects(bottomLabelBounds)) {
        // Complete short-wide or narrow-tall labels can coexist whenever their
        // actual bounds are disjoint. Only partition the surface when both
        // axes overlap, choosing the axis that retains more of each label.
        const int horizontalSeparation = std::abs(bottomX - topX);
        const int verticalSeparation = std::abs(bottomY - topY);
        const bool partitionHorizontally = horizontalSeparation * labelHeight
            >= verticalSeparation * labelWidth;
        if (partitionHorizontally) {
            const int topCenterX = topX + labelWidth / 2;
            const int bottomCenterX = bottomX + labelWidth / 2;
            const int splitX = std::clamp((topCenterX + bottomCenterX) / 2,
                0, logicalSize.width());
            FloatRect leftClip { 0, 0, static_cast<float>(splitX),
                static_cast<float>(logicalSize.height()) };
            FloatRect rightClip { static_cast<float>(splitX), 0,
                static_cast<float>(logicalSize.width() - splitX),
                static_cast<float>(logicalSize.height()) };
            if (topCenterX <= bottomCenterX) {
                topClip = leftClip;
                bottomClip = rightClip;
            } else {
                topClip = rightClip;
                bottomClip = leftClip;
            }
        } else {
            const int topCenterY = topY + labelHeight / 2;
            const int bottomCenterY = bottomY + labelHeight / 2;
            const int splitY = std::clamp((topCenterY + bottomCenterY) / 2,
                0, logicalSize.height());
            FloatRect upperClip { 0, 0, static_cast<float>(logicalSize.width()),
                static_cast<float>(splitY) };
            FloatRect lowerClip { 0, static_cast<float>(splitY),
                static_cast<float>(logicalSize.width()),
                static_cast<float>(logicalSize.height() - splitY) };
            if (topCenterY <= bottomCenterY) {
                topClip = upperClip;
                bottomClip = lowerClip;
            } else {
                topClip = lowerClip;
                bottomClip = upperClip;
            }
        }
    }

    if (paintTopLabel)
        paintEngawaWatermarkLabel(context, topClip, topX + scale, topY + scale, scale);
    if (paintBottomLabel)
        paintEngawaWatermarkLabel(context, bottomClip, bottomX + scale, bottomY + scale, scale);
}
#endif

PlatformDisplayID engawaDisplayID(uint32_t maxFramesPerSecond)
{
    return engawaDisplayIDPrefix | maxFramesPerSecond;
}

class EngawaInProcessDisplayRefreshMonitor final
    : public DisplayRefreshMonitor
    , public CanMakeWeakPtr<EngawaInProcessDisplayRefreshMonitor> {
public:
    static Ref<EngawaInProcessDisplayRefreshMonitor> create(
        PlatformDisplayID displayID, FramesPerSecond framesPerSecond)
    {
        return adoptRef(*new EngawaInProcessDisplayRefreshMonitor(
            displayID, framesPerSecond));
    }

    std::optional<FramesPerSecond> displayNominalFramesPerSecond() final
    {
        return m_framesPerSecond;
    }

private:
    EngawaInProcessDisplayRefreshMonitor(PlatformDisplayID displayID,
        FramesPerSecond framesPerSecond)
        : DisplayRefreshMonitor(displayID)
        , m_timer(RunLoop::mainSingleton(),
              "EngawaInProcessDisplayRefreshMonitor::Timer"_s,
              this,
              &EngawaInProcessDisplayRefreshMonitor::timerFired)
        , m_framesPerSecond(framesPerSecond)
        , m_interval(Seconds {
              1.0 / static_cast<double>(m_framesPerSecond) })
        , m_currentUpdate({ 0, framesPerSecond })
    {
    }

    bool startNotificationMechanism() final
    {
        if (!m_notificationMechanismRunning) {
            m_notificationMechanismRunning = true;
            m_nextFireTime = MonotonicTime::now() + m_interval;
            m_timer.startOneShot(m_interval);
        }
        return true;
    }

    void stopNotificationMechanism() final
    {
        m_notificationMechanismRunning = false;
        m_timer.stop();
        m_nextFireTime = { };
    }

    void timerFired()
    {
        displayLinkFired(m_currentUpdate);
        m_currentUpdate = m_currentUpdate.nextUpdate();
        if (!m_notificationMechanismRunning)
            return;

        const auto now = MonotonicTime::now();
        // RunLoop's repeating timers rebase to their actual callback time.
        // With a sampled owner-thread pump that delay compounds on every tick
        // and underfills 60/120/240 Hz. Preserve the nominal phase across a
        // sub-frame delay, but rebase a fully missed interval without catch-up.
        if (m_nextFireTime && m_nextFireTime <= now
            && now - m_nextFireTime < m_interval)
            m_nextFireTime += m_interval;
        else
            m_nextFireTime = now + m_interval;
        m_timer.startOneShot(std::max<Seconds>(
            m_nextFireTime - now, 0_s));
    }

    RunLoop::Timer m_timer;
    FramesPerSecond m_framesPerSecond;
    Seconds m_interval;
    MonotonicTime m_nextFireTime;
    bool m_notificationMechanismRunning { false };
    DisplayUpdate m_currentUpdate;
};

class EngawaInProcessDisplayRefreshMonitorFactory final
    : public DisplayRefreshMonitorFactory {
public:
    RefPtr<DisplayRefreshMonitor> createDisplayRefreshMonitor(
        PlatformDisplayID displayID) final
    {
        if ((displayID & 0xffff0000u) != engawaDisplayIDPrefix)
            return nullptr;
        const auto framesPerSecond = displayID & 0xffffu;
        if (!framesPerSecond
            || framesPerSecond > maximumEngawaFramesPerSecond)
            return nullptr;
        return EngawaInProcessDisplayRefreshMonitor::create(
            displayID, framesPerSecond);
    }
};

EngawaInProcessDisplayRefreshMonitorFactory&
engawaDisplayRefreshMonitorFactory()
{
    static NeverDestroyed<EngawaInProcessDisplayRefreshMonitorFactory>
        factory;
    return factory.get();
}

}

namespace {

constexpr uint32_t maximumPrototypeLogicalDimension = 32768;
constexpr uint32_t maximumPrototypePhysicalDimension = 8192;
constexpr uint64_t maximumPrototypeFrameBytes = 256ull * 1024 * 1024;
constexpr uint32_t knownMouseButtonBits = 0x1fu;
constexpr uint32_t knownInputModifierBits = EngawaInProcessShiftModifier
    | EngawaInProcessControlModifier | EngawaInProcessAltModifier
    | EngawaInProcessMetaModifier | EngawaInProcessCapsLockModifier
    | EngawaInProcessAltGraphModifier | EngawaInProcessNumLockModifier;

bool fail(String& error, ASCIILiteral message)
{
    error = message;
    return false;
}

bool calculatePhysicalViewport(uint32_t logicalWidth, uint32_t logicalHeight,
    float deviceScale, uint32_t& physicalWidth, uint32_t& physicalHeight,
    String& error)
{
    physicalWidth = 0;
    physicalHeight = 0;
    if (!logicalWidth || !logicalHeight
        || logicalWidth > maximumPrototypeLogicalDimension
        || logicalHeight > maximumPrototypeLogicalDimension
        || !std::isfinite(deviceScale) || deviceScale <= 0)
        return fail(error, "The prototype viewport is outside its supported bounds."_s);

    const double scaledWidth = std::ceil(static_cast<double>(logicalWidth)
        * static_cast<double>(deviceScale));
    const double scaledHeight = std::ceil(static_cast<double>(logicalHeight)
        * static_cast<double>(deviceScale));
    if (!std::isfinite(scaledWidth) || !std::isfinite(scaledHeight)
        || scaledWidth <= 0 || scaledHeight <= 0
        || scaledWidth > maximumPrototypePhysicalDimension
        || scaledHeight > maximumPrototypePhysicalDimension)
        return fail(error, "The prototype physical viewport is outside its supported bounds."_s);

    physicalWidth = static_cast<uint32_t>(scaledWidth);
    physicalHeight = static_cast<uint32_t>(scaledHeight);
    if (static_cast<uint64_t>(physicalWidth) * physicalHeight * 4
        > maximumPrototypeFrameBytes)
        return fail(error, "The prototype viewport exceeds its CPU frame byte limit."_s);
    return true;
}

std::optional<OptionSet<PlatformEvent::Modifier>> platformModifiers(uint32_t modifiers)
{
    if (modifiers & ~knownInputModifierBits)
        return std::nullopt;
    OptionSet<PlatformEvent::Modifier> result;
    if (modifiers & EngawaInProcessShiftModifier)
        result.add(PlatformEvent::Modifier::ShiftKey);
    if (modifiers & EngawaInProcessControlModifier)
        result.add(PlatformEvent::Modifier::ControlKey);
    if (modifiers & EngawaInProcessAltModifier)
        result.add(PlatformEvent::Modifier::AltKey);
    if (modifiers & EngawaInProcessMetaModifier)
        result.add(PlatformEvent::Modifier::MetaKey);
    if (modifiers & EngawaInProcessCapsLockModifier)
        result.add(PlatformEvent::Modifier::CapsLockKey);
    if (modifiers & EngawaInProcessAltGraphModifier)
        result.add(PlatformEvent::Modifier::AltGraphKey);
    if (modifiers & EngawaInProcessNumLockModifier)
        result.add(PlatformEvent::Modifier::NumLockKey);
    return result;
}

MouseButton platformMouseButton(EngawaInProcessMouseButton button,
    bool pointerHasNotChanged = false)
{
    switch (button) {
    case EngawaInProcessMouseButton::None:
        return pointerHasNotChanged ? MouseButton::PointerHasNotChanged
                                    : MouseButton::None;
    case EngawaInProcessMouseButton::Left:
        return MouseButton::Left;
    case EngawaInProcessMouseButton::Middle:
        return MouseButton::Middle;
    case EngawaInProcessMouseButton::Right:
        return MouseButton::Right;
    case EngawaInProcessMouseButton::Back:
        return MouseButton::Back;
    case EngawaInProcessMouseButton::Forward:
        return MouseButton::Forward;
    }
    return MouseButton::Other;
}

uint32_t mouseButtonBit(EngawaInProcessMouseButton button)
{
    switch (button) {
    case EngawaInProcessMouseButton::Left:
        return 1u << 0;
    case EngawaInProcessMouseButton::Right:
        return 1u << 1;
    case EngawaInProcessMouseButton::Middle:
        return 1u << 2;
    case EngawaInProcessMouseButton::Back:
        return 1u << 3;
    case EngawaInProcessMouseButton::Forward:
        return 1u << 4;
    case EngawaInProcessMouseButton::None:
        return 0;
    }
    return 0;
}

MouseButton platformMouseButtonForMotion(uint32_t buttons)
{
    // The public event identifies the button that changed, so motion carries
    // None while its buttons mask retains every held button. WebCore's native
    // Windows and Cocoa adapters instead identify a held button on dragged
    // motion, which EventHandler uses to recognize left-button text selection.
    static constexpr std::array heldButtonPriority {
        EngawaInProcessMouseButton::Left,
        EngawaInProcessMouseButton::Middle,
        EngawaInProcessMouseButton::Right,
        EngawaInProcessMouseButton::Back,
        EngawaInProcessMouseButton::Forward,
    };
    for (auto button : heldButtonPriority) {
        if (buttons & mouseButtonBit(button))
            return platformMouseButton(button);
    }
    return MouseButton::PointerHasNotChanged;
}

class EngawaPlatformMouseEvent final : public PlatformMouseEvent {
public:
    EngawaPlatformMouseEvent(const DoublePoint& point, MouseButton button,
        PlatformEvent::Type type, int clickCount,
        OptionSet<PlatformEvent::Modifier> modifiers, uint32_t buttons)
        : PlatformMouseEvent(point, point, button, type, clickCount, modifiers,
            MonotonicTime::now(), 0, SyntheticClickType::NoTap,
            MouseEventInputSource::UserDriven)
    {
        m_buttons = static_cast<unsigned short>(buttons);
    }
};

PlatformWheelEventGranularity platformWheelGranularity(
    EngawaInProcessWheelDeltaMode mode)
{
    switch (mode) {
    case EngawaInProcessWheelDeltaMode::Pixel:
        return PlatformWheelEventGranularity::ScrollByPixelWheelEvent;
    case EngawaInProcessWheelDeltaMode::Line:
        return PlatformWheelEventGranularity::ScrollByLineWheelEvent;
    case EngawaInProcessWheelDeltaMode::Page:
        return PlatformWheelEventGranularity::ScrollByPageWheelEvent;
    }
    return PlatformWheelEventGranularity::ScrollByPixelWheelEvent;
}

PlatformWheelEventPhase platformWheelPhase(EngawaInProcessWheelPhase phase)
{
    switch (phase) {
    case EngawaInProcessWheelPhase::None:
        return PlatformWheelEventPhase::None;
    case EngawaInProcessWheelPhase::Began:
        return PlatformWheelEventPhase::Began;
    case EngawaInProcessWheelPhase::Changed:
        return PlatformWheelEventPhase::Changed;
    case EngawaInProcessWheelPhase::Ended:
        return PlatformWheelEventPhase::Ended;
    case EngawaInProcessWheelPhase::Cancelled:
        return PlatformWheelEventPhase::Cancelled;
    }
    return PlatformWheelEventPhase::None;
}

class EngawaPlatformWheelEvent final : public PlatformWheelEvent {
public:
    EngawaPlatformWheelEvent(const EngawaInProcessWheelEvent& event,
        OptionSet<PlatformEvent::Modifier> modifiers)
        : PlatformWheelEvent(roundedPoint(event.x, event.y),
            roundedPoint(event.x, event.y),
            -event.deltaX, -event.deltaY, -event.deltaX, -event.deltaY,
            platformWheelGranularity(event.deltaMode),
            modifiers.contains(PlatformEvent::Modifier::ShiftKey),
            modifiers.contains(PlatformEvent::Modifier::ControlKey),
            modifiers.contains(PlatformEvent::Modifier::AltKey),
            modifiers.contains(PlatformEvent::Modifier::MetaKey))
    {
        m_modifiers = modifiers;
        m_hasPreciseScrollingDeltas = event.hasPreciseDeltas;
        if (event.isMomentum)
            m_momentumPhase = platformWheelPhase(event.phase);
        else
            m_phase = platformWheelPhase(event.phase);
    }

private:
    static IntPoint roundedPoint(double x, double y)
    {
        auto roundedCoordinate = [](double value) {
            auto rounded = std::round(value);
            return static_cast<int>(std::clamp(rounded,
                static_cast<double>(std::numeric_limits<int>::min()),
                static_cast<double>(std::numeric_limits<int>::max())));
        };
        return { roundedCoordinate(x), roundedCoordinate(y) };
    }
};

std::optional<int> windowsVirtualKeyCode(const String& code)
{
    if (code.length() == 4 && code.startsWith("Key"_s)) {
        auto character = code[3];
        if (character >= 'A' && character <= 'Z')
            return character;
    }
    if (code.length() == 6 && code.startsWith("Digit"_s)) {
        auto character = code[5];
        if (character >= '0' && character <= '9')
            return character;
    }
    if (code.length() == 7 && code.startsWith("Numpad"_s)) {
        auto character = code[6];
        if (character >= '0' && character <= '9')
            return 0x60 + character - '0';
    }
    if (code.startsWith('F') && code.length() >= 2 && code.length() <= 3) {
        unsigned value = 0;
        for (unsigned index = 1; index < code.length(); ++index) {
            auto character = code[index];
            if (character < '0' || character > '9')
                return std::nullopt;
            value = value * 10 + character - '0';
        }
        if (value >= 1 && value <= 24)
            return 0x70 + static_cast<int>(value) - 1;
    }

    static constexpr std::array mappings {
        std::pair { "Backspace"_s, 0x08 }, std::pair { "Tab"_s, 0x09 },
        std::pair { "Enter"_s, 0x0d }, std::pair { "ShiftLeft"_s, 0xa0 },
        std::pair { "ShiftRight"_s, 0xa1 }, std::pair { "ControlLeft"_s, 0xa2 },
        std::pair { "ControlRight"_s, 0xa3 }, std::pair { "AltLeft"_s, 0xa4 },
        std::pair { "AltRight"_s, 0xa5 }, std::pair { "Pause"_s, 0x13 },
        std::pair { "CapsLock"_s, 0x14 }, std::pair { "Escape"_s, 0x1b },
        std::pair { "Space"_s, 0x20 }, std::pair { "PageUp"_s, 0x21 },
        std::pair { "PageDown"_s, 0x22 }, std::pair { "End"_s, 0x23 },
        std::pair { "Home"_s, 0x24 }, std::pair { "ArrowLeft"_s, 0x25 },
        std::pair { "ArrowUp"_s, 0x26 }, std::pair { "ArrowRight"_s, 0x27 },
        std::pair { "ArrowDown"_s, 0x28 }, std::pair { "PrintScreen"_s, 0x2c },
        std::pair { "Insert"_s, 0x2d }, std::pair { "Delete"_s, 0x2e },
        std::pair { "MetaLeft"_s, 0x5b }, std::pair { "MetaRight"_s, 0x5c },
        std::pair { "ContextMenu"_s, 0x5d }, std::pair { "NumpadEnter"_s, 0x0d },
        std::pair { "NumpadMultiply"_s, 0x6a },
        std::pair { "NumpadAdd"_s, 0x6b }, std::pair { "NumpadSubtract"_s, 0x6d },
        std::pair { "NumpadDecimal"_s, 0x6e }, std::pair { "NumpadDivide"_s, 0x6f },
        std::pair { "NumLock"_s, 0x90 }, std::pair { "ScrollLock"_s, 0x91 },
        std::pair { "Semicolon"_s, 0xba },
        std::pair { "Equal"_s, 0xbb }, std::pair { "Comma"_s, 0xbc },
        std::pair { "Minus"_s, 0xbd }, std::pair { "Period"_s, 0xbe },
        std::pair { "Slash"_s, 0xbf }, std::pair { "Backquote"_s, 0xc0 },
        std::pair { "BracketLeft"_s, 0xdb }, std::pair { "Backslash"_s, 0xdc },
        std::pair { "BracketRight"_s, 0xdd }, std::pair { "Quote"_s, 0xde },
        std::pair { "IntlBackslash"_s, 0xe2 },
        std::pair { "BrowserBack"_s, 0xa6 }, std::pair { "BrowserForward"_s, 0xa7 }
    };
    for (auto [candidate, keyCode] : mappings) {
        if (code == candidate)
            return keyCode;
    }
    return std::nullopt;
}

String keyIdentifierForCode(const String& code, int virtualKey)
{
    if (virtualKey == 0x12 || virtualKey == 0xa4 || virtualKey == 0xa5)
        return "Alt"_s;
    if (virtualKey == 0x11 || virtualKey == 0xa2 || virtualKey == 0xa3)
        return "Control"_s;
    if (virtualKey == 0x10 || virtualKey == 0xa0 || virtualKey == 0xa1)
        return "Shift"_s;
    if (virtualKey == 0x5b || virtualKey == 0x5c)
        return "Win"_s;
    if (virtualKey == 0x14)
        return "CapsLock"_s;
    if (virtualKey == 0x0c)
        return "Clear"_s;
    if (virtualKey == 0x28)
        return "Down"_s;
    if (virtualKey == 0x23)
        return "End"_s;
    if (virtualKey == 0x0d)
        return "Enter"_s;
    if (virtualKey == 0x2b)
        return "Execute"_s;
    if (virtualKey == 0x24)
        return "Home"_s;
    if (virtualKey == 0x2d)
        return "Insert"_s;
    if (virtualKey == 0x25)
        return "Left"_s;
    if (virtualKey == 0x22)
        return "PageDown"_s;
    if (virtualKey == 0x21)
        return "PageUp"_s;
    if (virtualKey == 0x13)
        return "Pause"_s;
    if (virtualKey == 0x2c)
        return "PrintScreen"_s;
    if (virtualKey == 0x27)
        return "Right"_s;
    if (virtualKey == 0x91)
        return "Scroll"_s;
    if (virtualKey == 0x29)
        return "Select"_s;
    if (virtualKey == 0x26)
        return "Up"_s;
    if (virtualKey == 0x2e)
        return "U+007F"_s;
    if (code.startsWith('F') && code.length() >= 2 && code.length() <= 3)
        return code;
    return makeString("U+"_s,
        hex(static_cast<unsigned>(virtualKey), 4, WTF::Uppercase));
}

String editingCommandForKey(const KeyboardEvent& event)
{
    enum : uint8_t {
        Shift = 1 << 0,
        Control = 1 << 1,
        Alt = 1 << 2,
        Meta = 1 << 3,
        AltGraph = 1 << 4,
    };
    uint8_t modifiers = 0;
    if (event.shiftKey())
        modifiers |= Shift;
    if (event.ctrlKey())
        modifiers |= Control;
    if (event.altKey())
        modifiers |= Alt;
    if (event.metaKey())
        modifiers |= Meta;
    if (event.modifierKeys().contains(PlatformEvent::Modifier::AltGraphKey))
        modifiers |= AltGraph;

    struct Entry {
        int virtualKey;
        uint8_t modifiers;
        ASCIILiteral command;
    };
    static constexpr std::array navigationEntries {
        Entry { 0x25, 0, "MoveLeft"_s },
        Entry { 0x25, Shift, "MoveLeftAndModifySelection"_s },
#if PLATFORM(MAC)
        Entry { 0x25, Alt, "MoveWordLeft"_s },
        Entry { 0x25, Alt | Shift, "MoveWordLeftAndModifySelection"_s },
        Entry { 0x25, Meta, "MoveToBeginningOfLine"_s },
        Entry { 0x25, Meta | Shift, "MoveToBeginningOfLineAndModifySelection"_s },
#else
        Entry { 0x25, Control, "MoveWordLeft"_s },
        Entry { 0x25, Control | Shift, "MoveWordLeftAndModifySelection"_s },
#endif
        Entry { 0x27, 0, "MoveRight"_s },
        Entry { 0x27, Shift, "MoveRightAndModifySelection"_s },
#if PLATFORM(MAC)
        Entry { 0x27, Alt, "MoveWordRight"_s },
        Entry { 0x27, Alt | Shift, "MoveWordRightAndModifySelection"_s },
        Entry { 0x27, Meta, "MoveToEndOfLine"_s },
        Entry { 0x27, Meta | Shift, "MoveToEndOfLineAndModifySelection"_s },
#else
        Entry { 0x27, Control, "MoveWordRight"_s },
        Entry { 0x27, Control | Shift, "MoveWordRightAndModifySelection"_s },
#endif
        Entry { 0x26, 0, "MoveUp"_s },
        Entry { 0x26, Shift, "MoveUpAndModifySelection"_s },
#if PLATFORM(MAC)
        Entry { 0x26, Meta, "MoveToBeginningOfDocument"_s },
        Entry { 0x26, Meta | Shift, "MoveToBeginningOfDocumentAndModifySelection"_s },
#else
        Entry { 0x26, Control, "MoveToBeginningOfParagraph"_s },
        Entry { 0x26, Control | Shift, "MoveToBeginningOfParagraphAndModifySelection"_s },
#endif
        Entry { 0x28, 0, "MoveDown"_s },
        Entry { 0x28, Shift, "MoveDownAndModifySelection"_s },
#if PLATFORM(MAC)
        Entry { 0x28, Meta, "MoveToEndOfDocument"_s },
        Entry { 0x28, Meta | Shift, "MoveToEndOfDocumentAndModifySelection"_s },
#else
        Entry { 0x28, Control, "MoveToEndOfParagraph"_s },
        Entry { 0x28, Control | Shift, "MoveToEndOfParagraphAndModifySelection"_s },
#endif
        Entry { 0x21, 0, "MovePageUp"_s },
        Entry { 0x21, Shift, "MovePageUpAndModifySelection"_s },
        Entry { 0x22, 0, "MovePageDown"_s },
        Entry { 0x22, Shift, "MovePageDownAndModifySelection"_s },
        Entry { 0x24, 0, "MoveToBeginningOfLine"_s },
        Entry { 0x24, Shift, "MoveToBeginningOfLineAndModifySelection"_s },
        Entry { 0x24, Control, "MoveToBeginningOfDocument"_s },
        Entry { 0x24, Control | Shift, "MoveToBeginningOfDocumentAndModifySelection"_s },
        Entry { 0x23, 0, "MoveToEndOfLine"_s },
        Entry { 0x23, Shift, "MoveToEndOfLineAndModifySelection"_s },
        Entry { 0x23, Control, "MoveToEndOfDocument"_s },
        Entry { 0x23, Control | Shift, "MoveToEndOfDocumentAndModifySelection"_s },
        Entry { 0x08, 0, "DeleteBackward"_s },
        Entry { 0x08, Shift, "DeleteBackward"_s },
#if PLATFORM(MAC)
        Entry { 0x08, Alt, "DeleteWordBackward"_s },
        Entry { 0x08, Meta, "DeleteToBeginningOfLine"_s },
#else
        Entry { 0x08, Control, "DeleteWordBackward"_s },
        Entry { 0x08, Control | Shift, "DeleteWordBackward"_s },
#endif
        Entry { 0x2e, 0, "DeleteForward"_s },
#if PLATFORM(MAC)
        Entry { 0x2e, Alt, "DeleteWordForward"_s },
#else
        Entry { 0x2e, Control, "DeleteWordForward"_s },
        Entry { 0x2e, Control | Shift, "DeleteWordForward"_s },
#endif
        Entry { 0x1b, 0, "Cancel"_s },
        Entry { 0x09, 0, "InsertTab"_s },
        Entry { 0x09, Shift, "InsertBacktab"_s },
        Entry { 0x0d, 0, "InsertNewline"_s },
        Entry { 0x0d, Control, "InsertNewline"_s },
        Entry { 0x0d, Alt, "InsertNewline"_s },
        Entry { 0x0d, Shift, "InsertLineBreak"_s },
        Entry { 0x0d, Alt | Shift, "InsertNewline"_s },
        Entry { 0x2d, 0, "OverWrite"_s },
    };
    for (auto entry : navigationEntries) {
        if (event.keyCode() == entry.virtualKey
            && modifiers == entry.modifiers)
            return entry.command;
    }

    // Editing shortcuts bind to the logical key so they follow the user's
    // active keyboard layout. macOS uses Command; the other supported hosts
    // use Control. AltGraph must remain a text-layout modifier, never a chord.
#if PLATFORM(MAC)
    constexpr auto primary = Meta;
#else
    constexpr auto primary = Control;
#endif
    if (modifiers == primary) {
        if (equalLettersIgnoringASCIICase(event.key(), "a"_s))
            return "SelectAll"_s;
        if (equalLettersIgnoringASCIICase(event.key(), "b"_s))
            return "ToggleBold"_s;
        if (equalLettersIgnoringASCIICase(event.key(), "c"_s))
            return "Copy"_s;
        if (equalLettersIgnoringASCIICase(event.key(), "i"_s))
            return "ToggleItalic"_s;
        if (equalLettersIgnoringASCIICase(event.key(), "u"_s))
            return "ToggleUnderline"_s;
        if (equalLettersIgnoringASCIICase(event.key(), "v"_s))
            return "Paste"_s;
        if (equalLettersIgnoringASCIICase(event.key(), "x"_s))
            return "Cut"_s;
        if (equalLettersIgnoringASCIICase(event.key(), "z"_s))
            return "Undo"_s;
#if !PLATFORM(MAC)
        if (equalLettersIgnoringASCIICase(event.key(), "y"_s))
            return "Redo"_s;
#endif
    }
    if (modifiers == (primary | Shift)
        && equalLettersIgnoringASCIICase(event.key(), "z"_s))
        return "Redo"_s;
#if !PLATFORM(MAC)
    if (modifiers == Control && event.keyCode() == 0x2d)
        return "Copy"_s;
    if (modifiers == Shift && event.keyCode() == 0x2d)
        return "Paste"_s;
    if (modifiers == Shift && event.keyCode() == 0x2e)
        return "Cut"_s;
#endif
    return { };
}

EngawaInProcessKeyLocation keyLocationForCode(const String& code)
{
    if (code == "ShiftLeft"_s || code == "ControlLeft"_s
        || code == "AltLeft"_s || code == "MetaLeft"_s)
        return EngawaInProcessKeyLocation::Left;
    if (code == "ShiftRight"_s || code == "ControlRight"_s
        || code == "AltRight"_s || code == "MetaRight"_s)
        return EngawaInProcessKeyLocation::Right;
    if (code.startsWith("Numpad"_s))
        return EngawaInProcessKeyLocation::Numpad;
    return EngawaInProcessKeyLocation::Standard;
}

EngawaInProcessCursorType engawaCursorType(const Cursor& cursor)
{
    switch (cursor.type()) {
    case PlatformCursorType::Pointer:
    case PlatformCursorType::Invalid:
    case PlatformCursorType::Custom:
        return EngawaInProcessCursorType::Default;
    case PlatformCursorType::None: return EngawaInProcessCursorType::None;
    case PlatformCursorType::ContextMenu: return EngawaInProcessCursorType::ContextMenu;
    case PlatformCursorType::Help: return EngawaInProcessCursorType::Help;
    case PlatformCursorType::Hand: return EngawaInProcessCursorType::Pointer;
    case PlatformCursorType::Progress: return EngawaInProcessCursorType::Progress;
    case PlatformCursorType::Wait: return EngawaInProcessCursorType::Wait;
    case PlatformCursorType::Cell: return EngawaInProcessCursorType::Cell;
    case PlatformCursorType::Cross: return EngawaInProcessCursorType::Crosshair;
    case PlatformCursorType::IBeam: return EngawaInProcessCursorType::Text;
    case PlatformCursorType::VerticalText: return EngawaInProcessCursorType::VerticalText;
    case PlatformCursorType::Alias: return EngawaInProcessCursorType::Alias;
    case PlatformCursorType::Copy: return EngawaInProcessCursorType::Copy;
    case PlatformCursorType::Move: return EngawaInProcessCursorType::Move;
    case PlatformCursorType::NoDrop: return EngawaInProcessCursorType::NoDrop;
    case PlatformCursorType::NotAllowed: return EngawaInProcessCursorType::NotAllowed;
    case PlatformCursorType::Grab: return EngawaInProcessCursorType::Grab;
    case PlatformCursorType::Grabbing: return EngawaInProcessCursorType::Grabbing;
    case PlatformCursorType::MiddlePanning:
    case PlatformCursorType::EastPanning:
    case PlatformCursorType::NorthPanning:
    case PlatformCursorType::NorthEastPanning:
    case PlatformCursorType::NorthWestPanning:
    case PlatformCursorType::SouthPanning:
    case PlatformCursorType::SouthEastPanning:
    case PlatformCursorType::SouthWestPanning:
    case PlatformCursorType::WestPanning:
        return EngawaInProcessCursorType::AllScroll;
    case PlatformCursorType::ColumnResize: return EngawaInProcessCursorType::ColumnResize;
    case PlatformCursorType::RowResize: return EngawaInProcessCursorType::RowResize;
    case PlatformCursorType::NorthResize: return EngawaInProcessCursorType::NorthResize;
    case PlatformCursorType::EastResize: return EngawaInProcessCursorType::EastResize;
    case PlatformCursorType::SouthResize: return EngawaInProcessCursorType::SouthResize;
    case PlatformCursorType::WestResize: return EngawaInProcessCursorType::WestResize;
    case PlatformCursorType::NorthEastResize: return EngawaInProcessCursorType::NorthEastResize;
    case PlatformCursorType::NorthWestResize: return EngawaInProcessCursorType::NorthWestResize;
    case PlatformCursorType::SouthEastResize: return EngawaInProcessCursorType::SouthEastResize;
    case PlatformCursorType::SouthWestResize: return EngawaInProcessCursorType::SouthWestResize;
    case PlatformCursorType::EastWestResize: return EngawaInProcessCursorType::EastWestResize;
    case PlatformCursorType::NorthSouthResize: return EngawaInProcessCursorType::NorthSouthResize;
    case PlatformCursorType::NorthEastSouthWestResize: return EngawaInProcessCursorType::NorthEastSouthWestResize;
    case PlatformCursorType::NorthWestSouthEastResize: return EngawaInProcessCursorType::NorthWestSouthEastResize;
    case PlatformCursorType::ZoomIn: return EngawaInProcessCursorType::ZoomIn;
    case PlatformCursorType::ZoomOut: return EngawaInProcessCursorType::ZoomOut;
    }
    return EngawaInProcessCursorType::Default;
}

HashMap<const void*, EngawaInProcessPageMessageSink>& pageMessageSinks()
{
    static NeverDestroyed<HashMap<const void*, EngawaInProcessPageMessageSink>> sinks;
    return sinks.get();
}

JSValueRef pagePostMessageCallback(JSContextRef context, JSObjectRef,
    JSObjectRef, size_t argumentCount, const JSValueRef arguments[],
    JSValueRef* exception)
{
    auto iterator = pageMessageSinks().find(context);
    if (iterator == pageMessageSinks().end() || argumentCount != 1
        || !JSValueIsString(context, arguments[0])) {
        if (exception) {
            auto message = OpaqueJSString::tryCreate(
                "window.engawa.postMessage requires one JSON-compatible value."_s);
            *exception = JSValueMakeString(context, message.get());
        }
        return JSValueMakeUndefined(context);
    }

    JSValueRef conversionException { nullptr };
    auto json = adoptRef(JSValueToStringCopy(
        context, arguments[0], &conversionException));
    if (conversionException || !json
        || !iterator->value.post(iterator->value.context, json->string())) {
        if (exception) {
            auto message = OpaqueJSString::tryCreate(
                "The host rejected the page message."_s);
            *exception = JSValueMakeString(context, message.get());
        }
    }
    return JSValueMakeUndefined(context);
}

ResourceError offlineError(const URL& url, bool cancellation = false)
{
    auto error = internalError(url);
    if (cancellation)
        error.setType(ResourceError::Type::Cancellation);
    return error;
}

ResourceError bundleError(const URL& url, EngawaInProcessBundleReadStatus status)
{
    auto type = status == EngawaInProcessBundleReadStatus::InvalidRequest
        ? ResourceError::Type::AccessControl : ResourceError::Type::General;
    int code = status == EngawaInProcessBundleReadStatus::NotFound ? 2 : 1;
    return { "EngawaBundle"_s, code, url,
        "The local bundle resource was rejected or unavailable."_s, type };
}

class EngawaInProcessLoaderStrategy final : public LoaderStrategy {
public:
    void registerPage(Page& page, const EngawaInProcessBundleProvider& provider)
    {
        ASSERT(!m_bindings.contains(&page));
        m_bindings.add(&page, PageBinding { provider, { } });
    }

    void unregisterPage(Page& page)
    {
        m_bindings.remove(&page);
    }

    void activateMount(Page& page, const String& mount)
    {
        auto iterator = m_bindings.find(&page);
        ASSERT(iterator != m_bindings.end());
        if (iterator != m_bindings.end())
            iterator->value.mount = mount;
    }

    void clearMount(Page& page)
    {
        auto iterator = m_bindings.find(&page);
        if (iterator != m_bindings.end())
            iterator->value.mount = emptyString();
    }

    String activeMount(Page& page) const
    {
        auto iterator = m_bindings.find(&page);
        return iterator == m_bindings.end() ? emptyString()
                                            : iterator->value.mount;
    }

    void loadResource(LocalFrame& frame, CachedResource& resource,
        ResourceRequest&& request, const ResourceLoaderOptions& options,
        CompletionHandler<void(RefPtr<SubresourceLoader>&&)>&& completionHandler) final
    {
        SubresourceLoader::create(frame, resource, WTF::move(request), options,
            [this, completionHandler = WTF::move(completionHandler)]
            (RefPtr<SubresourceLoader>&& loader) mutable {
                RefPtr protectedLoader = loader;
                completionHandler(WTF::move(loader));
                if (!protectedLoader || protectedLoader->reachedTerminalState())
                    return;

                RefPtr protectedFrame = protectedLoader->frame();
                RefPtr protectedPage = protectedFrame ? protectedFrame->page() : nullptr;

                const auto& request = protectedLoader->originalRequest();
                auto url = request.url();
                auto method = readMethod(request);
                EngawaInProcessBundleResource bundleResource;
                auto status = method
                    ? read(protectedPage.get(), url,
                        EngawaInProcessBundleReadPurpose::Subresource,
                        *method, bundleResource)
                    : EngawaInProcessBundleReadStatus::InvalidRequest;
                if (status != EngawaInProcessBundleReadStatus::Success
                    || !method) {
                    protectedLoader->didFail(bundleError(url, status));
                    return;
                }

                auto response = responseFor(url, bundleResource);
                RefPtr<FragmentedSharedBuffer> buffer;
                if (*method == EngawaInProcessBundleReadMethod::Get)
                    buffer = SharedBuffer::create(
                        WTF::move(bundleResource.bytes));
                protectedLoader->deliverResponseAndData(
                    WTF::move(response), WTF::move(buffer));
            });
    }

    void loadResourceSynchronously(FrameLoader& frameLoader,
        ResourceLoaderIdentifier, const ResourceRequest& request,
        ClientCredentialPolicy, const FetchOptions&, const HTTPHeaderMap&,
        ResourceError& error, ResourceResponse& response,
        Vector<uint8_t>& data) final
    {
        auto method = readMethod(request);
        EngawaInProcessBundleResource bundleResource;
        auto status = method
            ? read(frameLoader.frame().page(), request.url(),
                EngawaInProcessBundleReadPurpose::Subresource, *method,
                bundleResource)
            : EngawaInProcessBundleReadStatus::InvalidRequest;
        if (status != EngawaInProcessBundleReadStatus::Success || !method) {
            error = bundleError(request.url(), status);
            response = { };
            data.clear();
            return;
        }

        error = { };
        response = responseFor(request.url(), bundleResource);
        data = *method == EngawaInProcessBundleReadMethod::Get
            ? WTF::move(bundleResource.bytes) : Vector<uint8_t> { };
    }

    void pageLoadCompleted(Page&) final { }
    void browsingContextRemoved(LocalFrame&) final { }
    void remove(ResourceLoader*) final { }
    void setDefersLoading(ResourceLoader&, bool) final { }
    void crossOriginRedirectReceived(ResourceLoader*, const URL&) final { }
    void servePendingRequests(ResourceLoadPriority) final { }
    void suspendPendingRequests() final { }
    void resumePendingRequests() final { }

    bool usePingLoad() const final { return false; }

    void startPingLoad(LocalFrame&, ResourceRequest& request, const HTTPHeaderMap&, const FetchOptions&, ContentSecurityPolicyImposition, PingLoadCompletionHandler&& completionHandler) final
    {
        if (completionHandler)
            completionHandler(offlineError(request.url()), { });
    }

    void preconnectTo(FrameLoader&, ResourceRequest&& request, StoredCredentialsPolicy, ShouldPreconnectAsFirstParty, PreconnectCompletionHandler&& completionHandler) final
    {
        if (completionHandler)
            completionHandler(offlineError(request.url()));
    }

    void setCaptureExtraNetworkLoadMetricsEnabled(bool) final { }
    bool isOnLine() const final { return false; }

    void addOnlineStateChangeListener(Function<void(bool)>&& listener) final
    {
        listener(false);
    }

    void isResourceLoadFinished(CachedResource&, CompletionHandler<void(bool)>&& completionHandler) final
    {
        completionHandler(true);
    }

    ResourceError cancelledError(const ResourceRequest& request) const final { return offlineError(request.url(), true); }
    ResourceError blockedError(const ResourceRequest& request) const final { return offlineError(request.url()); }
    ResourceError blockedByContentBlockerError(const ResourceRequest& request) const final { return offlineError(request.url()); }
    ResourceError cannotShowURLError(const ResourceRequest& request) const final { return offlineError(request.url()); }
    ResourceError interruptedForPolicyChangeError(const ResourceRequest& request) const final { return offlineError(request.url()); }
#if ENABLE(CONTENT_FILTERING)
    ResourceError blockedByContentFilterError(const ResourceRequest& request) const final { return offlineError(request.url()); }
#endif
    ResourceError cannotShowMIMETypeError(const ResourceResponse& response) const final { return offlineError(response.url()); }
    ResourceError fileDoesNotExistError(const ResourceResponse& response) const final { return offlineError(response.url()); }
    ResourceError httpsUpgradeRedirectLoopError(const ResourceRequest& request) const final { return offlineError(request.url()); }
    ResourceError httpNavigationWithHTTPSOnlyError(const ResourceRequest& request) const final { return offlineError(request.url()); }
    ResourceError pluginWillHandleLoadError(const ResourceResponse& response) const final { return offlineError(response.url()); }

private:
    static constexpr uint64_t maximumBundleResponseBytes = 1024 * 1024;

    struct PageBinding {
        EngawaInProcessBundleProvider provider;
        String mount;
    };

    static std::optional<EngawaInProcessBundleReadMethod> readMethod(
        const ResourceRequest& request)
    {
        if (request.httpMethod() == "GET"_s)
            return EngawaInProcessBundleReadMethod::Get;
        if (request.httpMethod() == "HEAD"_s)
            return EngawaInProcessBundleReadMethod::Head;
        return std::nullopt;
    }

    static ResourceResponse responseFor(const URL& url,
        const EngawaInProcessBundleResource& resource)
    {
        ResourceResponse response(URL { url }, String { resource.mimeType },
            static_cast<long long>(resource.contentLength),
            String { resource.textEncoding });
        response.setHTTPStatusCode(200);
        response.setHTTPStatusText("OK"_s);
        response.setHTTPVersion("HTTP/1.1"_s);
        return response;
    }

    EngawaInProcessBundleReadStatus read(Page* page, const URL& url,
        EngawaInProcessBundleReadPurpose purpose,
        EngawaInProcessBundleReadMethod method,
        EngawaInProcessBundleResource& resource)
    {
        resource = { };
        if (!page)
            return EngawaInProcessBundleReadStatus::InvalidRequest;
        auto iterator = m_bindings.find(page);
        if (iterator == m_bindings.end() || !iterator->value.provider
            || (purpose == EngawaInProcessBundleReadPurpose::Subresource
                && iterator->value.mount.isEmpty()))
            return EngawaInProcessBundleReadStatus::InvalidRequest;

        auto binding = iterator->value;

        auto status = binding.provider.read(
            binding.provider.context, url.string(),
            purpose == EngawaInProcessBundleReadPurpose::Subresource
                ? binding.mount : emptyString(),
            purpose, method, resource);
        if (status != EngawaInProcessBundleReadStatus::Success) {
            resource = { };
            return status;
        }
        if (resource.mount.isEmpty() || resource.mimeType.isEmpty()
            || resource.contentLength > maximumBundleResponseBytes
            || (method == EngawaInProcessBundleReadMethod::Get
                && resource.bytes.size() != resource.contentLength)
            || (method == EngawaInProcessBundleReadMethod::Head
                && !resource.bytes.isEmpty())
            || (purpose == EngawaInProcessBundleReadPurpose::Subresource
                && resource.mount != binding.mount)) {
            resource = { };
            return EngawaInProcessBundleReadStatus::Unavailable;
        }
        return status;
    }

    HashMap<Page*, PageBinding> m_bindings;
};

EngawaInProcessLoaderStrategy& engawaLoaderStrategy()
{
    static NeverDestroyed<EngawaInProcessLoaderStrategy> strategy;
    return strategy.get();
}

class EngawaEmptyPasteboardStrategy final : public PasteboardStrategy {
public:
#if PLATFORM(COCOA)
    void getTypes(Vector<String>& types, const String&, const PasteboardContext*) final { types.clear(); }
    RefPtr<SharedBuffer> bufferForType(const String&, const String&, const PasteboardContext*) final { return nullptr; }
    void getPathnamesForType(Vector<String>& pathnames, const String&, const String&, const PasteboardContext*) final { pathnames.clear(); }
    String stringForType(const String&, const String&, const PasteboardContext*) final { return emptyString(); }
    Vector<String> allStringsForType(const String&, const String&, const PasteboardContext*) final { return { }; }
    int64_t changeCount(const String&, const PasteboardContext*) final { return 0; }
    Color color(const String&, const PasteboardContext*) final { return { }; }
    URL url(const String&, const PasteboardContext*) final { return { }; }
    int getNumberOfFiles(const String&, const PasteboardContext*) final { return 0; }
    int64_t addTypes(const Vector<String>&, const String&, const PasteboardContext*) final { return 0; }
    int64_t setTypes(const Vector<String>&, const String&, const PasteboardContext*) final { return 0; }
    int64_t setBufferForType(SharedBuffer*, const String&, const String&, const PasteboardContext*) final { return 0; }
    int64_t setURL(const PasteboardURL&, const String&, const PasteboardContext*) final { return 0; }
    int64_t setColor(const Color&, const String&, const PasteboardContext*) final { return 0; }
    int64_t setStringForType(const String&, const String&, const String&, const PasteboardContext*) final { return 0; }
    int64_t writeWebArchive(LegacyWebArchive&, const String&) final { return 0; }
    bool containsURLStringSuitableForLoading(const String&, const PasteboardContext*) final { return false; }
    String urlStringSuitableForLoading(const String&, String& title, const PasteboardContext*) final
    {
        title = emptyString();
        return emptyString();
    }
#endif
    String readStringFromPasteboard(size_t, const String&, const String&, const PasteboardContext*) final { return emptyString(); }
    RefPtr<SharedBuffer> readBufferFromPasteboard(std::optional<size_t>, const String&, const String&, const PasteboardContext*) final { return nullptr; }
    URL readURLFromPasteboard(size_t, const String&, String& title, const PasteboardContext*) final
    {
        title = emptyString();
        return { };
    }
    std::optional<PasteboardItemInfo> informationForItemAtIndex(size_t, const String&, int64_t, const PasteboardContext*) final { return std::nullopt; }
    std::optional<Vector<PasteboardItemInfo>> allPasteboardItemInfo(const String&, int64_t, const PasteboardContext*) final { return std::nullopt; }
    int getPasteboardItemsCount(const String&, const PasteboardContext*) final { return 0; }
    Vector<String> typesSafeForDOMToReadAndWrite(const String&, const String&, const PasteboardContext*) final { return { }; }
    int64_t writeCustomData(const Vector<PasteboardCustomData>&, const String&, const PasteboardContext*) final { return 0; }
    bool containsStringSafeForDOMToReadForType(const String&, const String&, const PasteboardContext*) final { return false; }
#if PLATFORM(GTK) || PLATFORM(WPE)
    Vector<String> types(const String&) final { return { }; }
    String readTextFromClipboard(const String&, const String&) final { return emptyString(); }
    Vector<String> readFilePathsFromClipboard(const String&) final { return { }; }
    RefPtr<SharedBuffer> readBufferFromClipboard(const String&, const String&) final { return nullptr; }
    void writeToClipboard(const String&, SelectionData&&) final { }
    void clearClipboard(const String&) final { }
    int64_t changeCount(const String&) final { return 0; }
#elif USE(LIBWPE)
    void getTypes(Vector<String>& types) final { types.clear(); }
    void writeToPasteboard(const PasteboardWebContent&) final { }
    void writeToPasteboard(const String&, const String&) final { }
#endif
};

class EngawaDisabledMediaStrategy final : public MediaStrategy {
};

class EngawaEmptyBlobRegistry final : public BlobRegistry {
public:
    void registerInternalFileBlobURL(const URL&, Ref<BlobDataFileReference>&&, const String&, const String&) final { }
    void registerInternalBlobURL(const URL&, Vector<BlobPart>&&, const String&) final { }
    void registerBlobURL(const URL&, const URL&, const PolicyContainer&, const std::optional<SecurityOriginData>&) final { }
    void registerInternalBlobURLOptionallyFileBacked(const URL&, const URL&, RefPtr<BlobDataFileReference>&&, const String&) final { }
    void registerInternalBlobURLForSlice(const URL&, const URL&, long long, long long, const String&) final { }
    void unregisterBlobURL(const URL&, const std::optional<SecurityOriginData>&) final { }
    void registerBlobURLHandle(const URL&, const std::optional<SecurityOriginData>&) final { }
    void unregisterBlobURLHandle(const URL&, const std::optional<SecurityOriginData>&) final { }
    String blobType(const URL&) final { return emptyString(); }
    unsigned long long blobSize(const URL&) final { return 0; }
    void writeBlobsToTemporaryFilesForIndexedDB(const Vector<String>&, CompletionHandler<void(Vector<String>&& filePaths)>&& completionHandler) final
    {
        completionHandler({ });
    }
};

class EngawaOfflinePlatformStrategies final : public PlatformStrategies {
public:
    EngawaOfflinePlatformStrategies() = default;

private:
    LoaderStrategy* createLoaderStrategy() final
    {
        return &engawaLoaderStrategy();
    }

    PasteboardStrategy* createPasteboardStrategy() final
    {
        static NeverDestroyed<EngawaEmptyPasteboardStrategy> strategy;
        return &strategy.get();
    }

    MediaStrategy* createMediaStrategy() final
    {
        static NeverDestroyed<EngawaDisabledMediaStrategy> strategy;
        return &strategy.get();
    }

    BlobRegistry* createBlobRegistry() final
    {
        static NeverDestroyed<EngawaEmptyBlobRegistry> registry;
        return &registry.get();
    }

#if ENABLE(DECLARATIVE_WEB_PUSH)
    PushStrategy* createPushStrategy() final { return nullptr; }
#endif
};

EngawaOfflinePlatformStrategies& engawaPlatformStrategies()
{
    static NeverDestroyed<EngawaOfflinePlatformStrategies> strategies;
    return strategies.get();
}

struct EngawaRuntimeState {
    Lock lock;
    std::optional<uint32_t> ownerThreadID;
};

EngawaRuntimeState& engawaRuntimeState()
{
    static NeverDestroyed<EngawaRuntimeState> state;
    return state.get();
}

bool ensureRuntimeOnCurrentThread(uint32_t& ownerThreadID, String& error)
{
    auto current = currentThreadID();
    auto& state = engawaRuntimeState();
    Locker locker { state.lock };

    if (state.ownerThreadID && *state.ownerThreadID != current)
        return fail(error, "WebCore is owned by a different EngawaRuntime worker thread."_s);

    auto& strategies = engawaPlatformStrategies();
    if (hasPlatformStrategies() && platformStrategies() != &strategies)
        return fail(error, "Another WebCore PlatformStrategies instance is already installed."_s);

    if (!state.ownerThreadID) {
#if defined(ER_INPROCESS_EMBEDDED_ENGINE) && PLATFORM(WIN)
        if (!initializeInstanceHandle())
            return fail(error, "The containing EngawaRuntime module could not be resolved."_s);
#endif
        initializeMainThreadIfNeeded();
        if (!isMainThread() || !RunLoop::isMain())
            return fail(error, "WTF/WebCore was previously initialized on a different thread."_s);
        if (!hasPlatformStrategies())
            setPlatformStrategies(&strategies);
        LegacySchemeRegistry::registerURLSchemeAsHandledBySchemeHandler(
            "engawa"_s);
        LegacySchemeRegistry::registerURLSchemeAsSecure("engawa"_s);
        LegacySchemeRegistry::setDomainRelaxationForbiddenForURLScheme(
            true, "engawa"_s);
        state.ownerThreadID = current;
    }

    ownerThreadID = *state.ownerThreadID;
    error = emptyString();
    return true;
}

} // anonymous namespace

class EngawaInProcessChromeClient final : public EmptyChromeClient {
    WTF_MAKE_TZONE_ALLOCATED(EngawaInProcessChromeClient);
public:
    explicit EngawaInProcessChromeClient(EngawaInProcessWebCorePrototype& owner)
        : m_owner(&owner)
    {
    }

private:
    void chromeDestroyed() final { m_owner = nullptr; }

    void focusedElementChanged(Element* element, LocalFrame*, FocusOptions,
        BroadcastFocusedElement) final
    {
        if (m_owner)
            m_owner->didChangeEditableFocus(element
                && (element->isTextField() || element->isContentEditable()));
    }

    bool hoverSupportedByPrimaryPointingDevice() const final { return true; }
    bool hoverSupportedByAnyAvailablePointingDevice() const final { return true; }

    // Root-view chrome is outside the CPU content surface. WebChromeClient
    // likewise leaves this callback inert; treating it as content damage
    // would force unnecessary raster work without painting any root chrome.
    void invalidateRootView(const IntRect&) final
    {
    }
    void invalidateContentsAndRootView(const IntRect& rect) final
    {
        if (m_owner)
            m_owner->didInvalidate(rect);
    }
    void invalidateContentsForSlowScroll(const IntRect& rect) final
    {
        if (m_owner)
            m_owner->didInvalidate(rect);
    }
    void scroll(const IntSize&, const IntRect&, const IntRect& clipRect) final
    {
        if (m_owner)
            m_owner->didInvalidate(clipRect);
    }

    void setCursor(const Cursor& cursor) final
    {
        if (m_owner)
            m_owner->didChangeCursor(engawaCursorType(cursor));
    }

    void triggerRenderingUpdate() final
    {
        if (m_owner)
            m_owner->renderingUpdateRequested();
    }

    // EmptyChromeClient deliberately installs an inert refresh monitor. The
    // strict runtime supplies a per-rate Engawa RunLoop timer so CSS transitions
    // and requestAnimationFrame continue without another input command forcing
    // an update.
    DisplayRefreshMonitorFactory* displayRefreshMonitorFactory() const final
    {
        return &engawaDisplayRefreshMonitorFactory();
    }

    EngawaInProcessWebCorePrototype* m_owner { nullptr };
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(EngawaInProcessChromeClient);

std::unique_ptr<EngawaInProcessWebCorePrototype> EngawaInProcessWebCorePrototype::create(uint32_t width, uint32_t height, String& error)
{
    return create(width, height, { }, error);
}

std::unique_ptr<EngawaInProcessWebCorePrototype> EngawaInProcessWebCorePrototype::create(uint32_t width, uint32_t height, const EngawaInProcessBundleProvider& bundleProvider, String& error)
{
    EngawaInProcessViewConfiguration configuration;
    configuration.logicalWidth = width;
    configuration.logicalHeight = height;
    configuration.bundleProvider = bundleProvider;
    return create(configuration, error);
}

std::unique_ptr<EngawaInProcessWebCorePrototype> EngawaInProcessWebCorePrototype::create(const EngawaInProcessViewConfiguration& configuration, String& error)
{
    uint32_t physicalWidth { 0 };
    uint32_t physicalHeight { 0 };
    if (!calculatePhysicalViewport(configuration.logicalWidth,
            configuration.logicalHeight, configuration.deviceScale,
            physicalWidth, physicalHeight, error))
        return nullptr;

    if ((configuration.bundleProvider.context
            && !configuration.bundleProvider.read)
        || (!configuration.bundleProvider.context
            && configuration.bundleProvider.read))
        return fail(error, "The local bundle provider is incomplete."_s), nullptr;
    if ((configuration.pageMessageSink.context
            && !configuration.pageMessageSink.post)
        || (!configuration.pageMessageSink.context
            && configuration.pageMessageSink.post))
        return fail(error, "The page-message sink is incomplete."_s), nullptr;
    if ((!configuration.clipboardProvider.context
            && (configuration.clipboardProvider.readText
                || configuration.clipboardProvider.writeText))
        || (configuration.clipboardProvider.context
            && !configuration.clipboardProvider.readText
            && !configuration.clipboardProvider.writeText))
        return fail(error, "The clipboard provider is incomplete."_s), nullptr;
    if (!configuration.maxFramesPerSecond
        || configuration.maxFramesPerSecond > maximumEngawaFramesPerSecond)
        return fail(error, "The maximum frame rate is outside the supported range."_s), nullptr;

    uint32_t ownerThreadID { 0 };
    if (!ensureRuntimeOnCurrentThread(ownerThreadID, error))
        return nullptr;
    inProcessRuntimeInitialized = true;

    auto prototype = std::unique_ptr<EngawaInProcessWebCorePrototype>(
        new EngawaInProcessWebCorePrototype(configuration, physicalWidth,
            physicalHeight, ownerThreadID));
    if (!prototype->initializePage(error))
        return nullptr;
    return prototype;
}

EngawaInProcessWebCorePrototype::EngawaInProcessWebCorePrototype(
    const EngawaInProcessViewConfiguration& configuration,
    uint32_t physicalWidth, uint32_t physicalHeight, uint32_t ownerThreadID)
    : m_logicalWidth(configuration.logicalWidth)
    , m_logicalHeight(configuration.logicalHeight)
    , m_physicalWidth(physicalWidth)
    , m_physicalHeight(physicalHeight)
    , m_deviceScale(configuration.deviceScale)
    , m_maxFramesPerSecond(configuration.maxFramesPerSecond)
    , m_ownerThreadID(ownerThreadID)
    , m_bundleProvider(configuration.bundleProvider)
    , m_pageMessageSink(configuration.pageMessageSink)
    , m_clipboardProvider(configuration.clipboardProvider)
    , m_transparent(configuration.transparent)
{
    ++liveInProcessPrototypeCount;
    forceFullDamage();
}

EngawaInProcessWebCorePrototype::~EngawaInProcessWebCorePrototype()
{
    RELEASE_ASSERT_WITH_MESSAGE(isOwnerThread(), "EngawaInProcessWebCorePrototype must be destroyed on its owner thread.");
    if (m_pageBridgeContext)
        pageMessageSinks().remove(m_pageBridgeContext);
    if (m_frame)
        m_frame->loader().stopAllLoaders();
    if (m_page)
        engawaLoaderStrategy().unregisterPage(*m_page);
    m_frame = nullptr;
    m_page = nullptr;
    RELEASE_ASSERT(liveInProcessPrototypeCount);
    --liveInProcessPrototypeCount;
}

bool EngawaInProcessWebCorePrototype::initializePage(String& error)
{
    auto pageConfiguration = pageConfigurationWithEmptyClients(
        std::nullopt, PAL::SessionID::defaultSessionID(), true,
        [this](KeyboardEvent& event) { handleKeyboardEvent(event); });
    pageConfiguration.loadsSubresources = !!m_bundleProvider;
    // Engawa's local game UI is always a visible presentation surface. Max FPS
    // remains its sole animation/output ceiling even when the host OS enters
    // low-power mode; Page still retains the truthful low-power state for DOM
    // timer alignment, media policy, and other non-rendering behavior.
    pageConfiguration.suppressLowPowerModeAnimationThrottling = true;
    pageConfiguration.chromeClient = makeUniqueRef<EngawaInProcessChromeClient>(*this);

    auto* localCreationParameters = std::get_if<PageConfiguration::LocalMainFrameCreationParameters>(&pageConfiguration.mainFrameCreationParameters);
    if (!localCreationParameters)
        return fail(error, "The empty PageConfiguration did not create a local main frame."_s);
    localCreationParameters->effectiveSandboxFlags = { };

    m_page = Page::create(WTF::move(pageConfiguration));
    engawaLoaderStrategy().registerPage(*m_page, m_bundleProvider);
    m_page->settings().setScriptEnabled(true);
    m_page->settings().setImagesEnabled(true);
    m_page->settings().setLoadsImagesAutomatically(true);
    m_page->settings().setAcceleratedCompositingEnabled(false);
    m_page->settings().setPreferPageRenderingUpdatesNear60FPSEnabled(false);
    m_page->settings().setLinkPreloadEnabled(false);
    m_page->setDeviceScaleFactor(m_deviceScale);

    m_frame = m_page->localMainFrame();
    if (!m_frame)
        return fail(error, "WebCore did not create a local main frame."_s);

    m_frame->setView(LocalFrameView::create(*m_frame, IntSize {
        static_cast<int>(m_logicalWidth), static_cast<int>(m_logicalHeight) }));
    m_frame->init();
    if (!m_frame->view())
        return fail(error, "WebCore did not create a local frame view."_s);

    m_frame->view()->setFrameRect({ { }, {
        static_cast<int>(m_logicalWidth), static_cast<int>(m_logicalHeight) } });
    m_frame->view()->setCanHaveScrollbars(false);
    m_frame->view()->setTransparent(m_transparent);
    m_frame->view()->setBaseBackgroundColor(
        m_transparent ? Color::transparentBlack : Color::white);
    m_page->focusController().setActive(true);
    m_page->focusController().setFocused(true);
    m_page->windowScreenDidChange(
        engawaDisplayID(m_maxFramesPerSecond), m_maxFramesPerSecond);

    error = emptyString();
    return true;
}

bool EngawaInProcessWebCorePrototype::setMaxFramesPerSecond(
    uint32_t maxFramesPerSecond, String& error)
{
    if (!isOwnerThread())
        return fail(error, "setMaxFramesPerSecond was called off the WebCore owner thread."_s);
    if (!maxFramesPerSecond
        || maxFramesPerSecond > maximumEngawaFramesPerSecond)
        return fail(error, "The maximum frame rate is outside the supported range."_s);
    if (maxFramesPerSecond == m_maxFramesPerSecond) {
        error = emptyString();
        return true;
    }
    m_maxFramesPerSecond = maxFramesPerSecond;
    if (m_page) {
        m_page->windowScreenDidChange(
            engawaDisplayID(m_maxFramesPerSecond), m_maxFramesPerSecond);
    }
    error = emptyString();
    return true;
}

bool EngawaInProcessWebCorePrototype::loadTrustedHTML(const String& html, const String& baseURLString, String& error)
{
    bool replacementStarted { false };
    return loadTrustedHTML(html, baseURLString, replacementStarted, error);
}

bool EngawaInProcessWebCorePrototype::loadTrustedHTML(const String& html,
    const String& baseURLString, bool& replacementStarted, String& error)
{
    replacementStarted = false;
    if (!isOwnerThread())
        return fail(error, "loadTrustedHTML was called off the WebCore owner thread."_s);
    URL baseURL { baseURLString };
    if (!baseURL.isValid())
        return fail(error, "The trusted document base URL is invalid."_s);

    String mount;
    if (baseURL.protocolIs("engawa"_s)) {
        if (!m_bundleProvider)
            return fail(error, "The engawa bundle base URL has no local bundle provider."_s);
        EngawaInProcessBundleResource resource;
        auto status = m_bundleProvider.read(m_bundleProvider.context,
            baseURL.string(), emptyString(),
            EngawaInProcessBundleReadPurpose::MainDocument,
            EngawaInProcessBundleReadMethod::Head, resource);
        if (status != EngawaInProcessBundleReadStatus::Success
            || resource.mount.isEmpty())
            return fail(error, "The engawa bundle base URL is unavailable."_s);
        mount = WTF::move(resource.mount);
    }
    return writeTrustedDocument(html, baseURLString, mount,
        replacementStarted, error);
}

bool EngawaInProcessWebCorePrototype::loadTrustedBundleURL(
    const String& urlString, String& error)
{
    bool replacementStarted { false };
    return loadTrustedBundleURL(urlString, replacementStarted, error);
}

bool EngawaInProcessWebCorePrototype::loadTrustedBundleURL(
    const String& urlString, bool& replacementStarted, String& error)
{
    replacementStarted = false;
    if (!isOwnerThread())
        return fail(error, "loadTrustedBundleURL was called off the WebCore owner thread."_s);
    if (!m_bundleProvider)
        return fail(error, "No local bundle provider is installed."_s);

    URL url { urlString };
    if (!url.isValid())
        return fail(error, "The trusted bundle URL is invalid."_s);

    EngawaInProcessBundleResource resource;
    auto status = m_bundleProvider.read(m_bundleProvider.context, url.string(),
        emptyString(), EngawaInProcessBundleReadPurpose::MainDocument,
        EngawaInProcessBundleReadMethod::Get, resource);
    if (status != EngawaInProcessBundleReadStatus::Success)
        return fail(error, "The trusted bundle document is unavailable."_s);
    if (resource.mount.isEmpty() || resource.mimeType != "text/html"_s
        || resource.textEncoding.convertToASCIILowercase() != "utf-8"_s
        || resource.contentLength > 1024 * 1024
        || resource.bytes.size() != resource.contentLength)
        return fail(error, "The trusted bundle document response is invalid."_s);

    auto byteSpan = resource.bytes.span();
    auto html = String::fromUTF8(std::span<const char> {
        reinterpret_cast<const char*>(byteSpan.data()), byteSpan.size() });
    if (html.isNull() && !byteSpan.empty())
        return fail(error, "The trusted bundle document is not valid UTF-8."_s);

    return writeTrustedDocument(html, urlString, resource.mount,
        replacementStarted, error);
}

bool EngawaInProcessWebCorePrototype::writeTrustedDocument(
    const String& html, const String& baseURLString, const String& mount,
    bool& replacementStarted, String& error)
{
    replacementStarted = false;
    URL baseURL { baseURLString };
    if (!baseURL.isValid())
        return fail(error, "The trusted document base URL is invalid."_s);

    RefPtr documentLoader = m_frame->loader().activeDocumentLoader();
    if (!documentLoader)
        return fail(error, "The local frame has no active DocumentLoader."_s);

    auto& writer = documentLoader->writer();
    writer.setMIMEType("text/html"_s);
    writer.setEncoding("UTF-8"_s, DocumentWriter::IsEncodingUserChosen::No);
    auto priorMount = engawaLoaderStrategy().activeMount(*m_page);
    if (mount.isEmpty())
        engawaLoaderStrategy().clearMount(*m_page);
    else
        engawaLoaderStrategy().activateMount(*m_page, mount);
    if (!writer.begin(baseURL)) {
        if (priorMount.isEmpty())
            engawaLoaderStrategy().clearMount(*m_page);
        else
            engawaLoaderStrategy().activateMount(*m_page, priorMount);
        return fail(error, "DocumentWriter rejected the trusted HTML document."_s);
    }
    replacementStarted = true;
    m_loaded = false;
    m_pressedMouseButtons = 0;
    m_cancelledMouseButtons = 0;
    m_cursor = EngawaInProcessCursorType::Default;
    m_editableFocus = false;
    if (!installPageBridge(error)) {
        writer.end();
        return false;
    }
    writer.insertDataSynchronously(html);
    writer.end();

    if (!m_frame->document())
        return fail(error, "The trusted HTML document did not create a DOM."_s);

    m_loaded = true;
    m_pendingLogicalDamage = { };
    forceFullDamage();
    error = emptyString();
    return true;
}

bool EngawaInProcessWebCorePrototype::installPageBridge(String& error)
{
    auto& world = mainThreadNormalWorldSingleton();
    JSC::JSLockHolder lock(commonVM());
    auto* globalObject = m_frame->script().globalObject(world);
    if (!globalObject)
        return fail(error, "The trusted document has no JavaScript global object."_s);

    auto context = toRef(globalObject);
    // DocumentWriter::begin() replaces the Document while retaining the
    // Window's JavaScript global. The public bridge is deliberately
    // non-configurable, so a successful bridge on that same global must be
    // reused rather than defined again. Refreshing the native sink keeps the
    // retained callback bound to the prototype's current host endpoint.
    if (m_pageBridgeContext == context) {
        if (m_pageMessageSink)
            pageMessageSinks().set(context, m_pageMessageSink);
        else
            pageMessageSinks().remove(context);
        error = emptyString();
        return true;
    }

    if (m_pageBridgeContext)
        pageMessageSinks().remove(m_pageBridgeContext);
    m_pageBridgeContext = nullptr;
    if (m_pageMessageSink)
        pageMessageSinks().set(context, m_pageMessageSink);

    auto hiddenName = OpaqueJSString::tryCreate("__engawaNativePostMessage"_s);
    auto functionName = OpaqueJSString::tryCreate("postMessage"_s);
    auto function = JSObjectMakeFunctionWithCallback(
        context, functionName.get(), pagePostMessageCallback);
    JSValueRef exception { nullptr };
    JSObjectSetProperty(context, JSContextGetGlobalObject(context),
        hiddenName.get(), function, kJSPropertyAttributeDontEnum, &exception);
    if (exception) {
        pageMessageSinks().remove(context);
        return fail(error, "The native page-message function could not be installed."_s);
    }

    auto result = m_frame->script().executeUserAgentScriptInWorld(world,
        "(()=>{const native=globalThis.__engawaNativePostMessage;delete globalThis.__engawaNativePostMessage;const api=Object.freeze({postMessage(value){const json=JSON.stringify(value);if(typeof json!=='string')throw new TypeError('window.engawa.postMessage value is not JSON-compatible');return native(json);}});Object.defineProperty(window,'engawa',{value:api,writable:false,enumerable:true,configurable:false});})()"_s,
        false);
    if (!result) {
        pageMessageSinks().remove(context);
        error = result.error().message;
        return false;
    }
    // Mark the context reusable only after the immutable public property was
    // installed successfully. A partial installation must fail again rather
    // than being mistaken for an idempotent reload.
    m_pageBridgeContext = context;
    error = emptyString();
    return true;
}

bool EngawaInProcessWebCorePrototype::checkOwnerAndLoaded(String& error) const
{
    if (!isOwnerThread())
        return fail(error, "The operation was called off the WebCore owner thread."_s);
    if (!m_loaded)
        return fail(error, "No trusted HTML document has been loaded."_s);
    return true;
}

bool EngawaInProcessWebCorePrototype::evaluateExpressionToJSON(const String& expression, String& json, String& error)
{
    json = emptyString();
    if (!checkOwnerAndLoaded(error))
        return false;
    if (m_evaluationGeneration == std::numeric_limits<uint64_t>::max())
        m_evaluationGeneration = 1;
    else
        ++m_evaluationGeneration;
    const auto generation = m_evaluationGeneration;

    auto& world = mainThreadNormalWorldSingleton();
    {
        JSC::JSLockHolder lock(commonVM());
        auto* globalObject = m_frame->script().globalObject(world);
        if (!globalObject)
            return fail(error, "The trusted document has no JavaScript global object."_s);
        auto context = toRef(globalObject);
        auto sourceName = OpaqueJSString::tryCreate(
            "__engawaEvaluationSource_8f55dc8a"_s);
        auto generationName = OpaqueJSString::tryCreate(
            "__engawaEvaluationGeneration_8f55dc8a"_s);
        auto sourceValue = OpaqueJSString::tryCreate(expression);
        if (!sourceName || !generationName || !sourceValue)
            return fail(error, "The JavaScript source exceeded available memory."_s);
        JSValueRef exception { nullptr };
        JSObjectSetProperty(context, JSContextGetGlobalObject(context),
            sourceName.get(), JSValueMakeString(context, sourceValue.get()),
            kJSPropertyAttributeDontEnum, &exception);
        if (!exception)
            JSObjectSetProperty(context, JSContextGetGlobalObject(context),
                generationName.get(),
                JSValueMakeNumber(context, static_cast<double>(generation)),
                kJSPropertyAttributeDontEnum, &exception);
        if (exception)
            return fail(error, "The JavaScript source could not be staged."_s);

        auto started = m_frame->script().executeUserAgentScriptInWorld(world,
            "(()=>{const source=globalThis.__engawaEvaluationSource_8f55dc8a;const generation=globalThis.__engawaEvaluationGeneration_8f55dc8a;delete globalThis.__engawaEvaluationSource_8f55dc8a;globalThis.__engawaEvaluationOutcome_8f55dc8a=undefined;const publish=outcome=>{if(globalThis.__engawaEvaluationGeneration_8f55dc8a===generation)globalThis.__engawaEvaluationOutcome_8f55dc8a=outcome};const failure=error=>publish('0'+String(error&&error.stack?error.stack:error));try{Promise.resolve((0,eval)(source)).then(value=>{try{const encoded=JSON.stringify(value===undefined?null:value);publish(typeof encoded==='string'?'1'+encoded:'0The JavaScript value is not JSON-compatible')}catch(error){failure(error)}},failure)}catch(error){failure(error)}})()"_s,
            false);
        if (!started) {
            error = started.error().message;
            return false;
        }
    }

    // Releasing the common-VM lock performs the bounded microtask checkpoint,
    // which settles plain values and already-resolved/rejected Promises. Never
    // spin the platform loop waiting for an arbitrary page Promise.
    JSC::JSLockHolder lock(commonVM());
    auto result = m_frame->script().executeUserAgentScriptInWorld(world,
        "(()=>{const outcome=globalThis.__engawaEvaluationOutcome_8f55dc8a;delete globalThis.__engawaEvaluationOutcome_8f55dc8a;globalThis.__engawaEvaluationGeneration_8f55dc8a=-1;return outcome})()"_s,
        false);
    if (!result) {
        error = result.error().message;
        return false;
    }
    if (!result.value().isString())
        return fail(error, "The JavaScript promise did not settle in the bounded evaluation turn."_s);

    auto* globalObject = m_frame->script().globalObject(world);
    if (!globalObject)
        return fail(error, "The trusted document has no JavaScript global object."_s);

    auto outcome = result.value().toWTFString(globalObject);
    if (outcome.isEmpty() || (outcome[0] != '0' && outcome[0] != '1'))
        return fail(error, "The JavaScript evaluation returned an invalid outcome."_s);
    if (outcome[0] == '0') {
        error = outcome.substring(1);
        if (error.isEmpty())
            error = "The JavaScript evaluation failed."_s;
        return false;
    }
    json = outcome.substring(1);
    error = emptyString();
    return true;
}

bool EngawaInProcessWebCorePrototype::dispatchLeftClick(int x, int y, String& error)
{
    EngawaInProcessMouseEvent moved;
    moved.type = EngawaInProcessMouseEventType::Move;
    moved.x = x;
    moved.y = y;
    if (!dispatchMouse(moved, error))
        return false;

    EngawaInProcessMouseEvent pressed;
    pressed.type = EngawaInProcessMouseEventType::Down;
    pressed.x = x;
    pressed.y = y;
    pressed.button = EngawaInProcessMouseButton::Left;
    pressed.buttons = 1;
    pressed.clickCount = 1;
    if (!dispatchMouse(pressed, error))
        return false;

    EngawaInProcessMouseEvent released = pressed;
    released.type = EngawaInProcessMouseEventType::Up;
    released.buttons = 0;
    return dispatchMouse(released, error);
}

bool EngawaInProcessWebCorePrototype::setFocus(bool focused, String& error)
{
    if (!isOwnerThread())
        return fail(error, "setFocus was called off the WebCore owner thread."_s);
    if (!m_page)
        return fail(error, "The WebCore page is unavailable for focus."_s);
    if (!focused && m_focused && m_loaded && m_pressedMouseButtons) {
        const auto pressedBeforeBlur = m_pressedMouseButtons;
        static constexpr std::array buttons {
            EngawaInProcessMouseButton::Left,
            EngawaInProcessMouseButton::Right,
            EngawaInProcessMouseButton::Middle,
            EngawaInProcessMouseButton::Back,
            EngawaInProcessMouseButton::Forward,
        };
        for (auto button : buttons) {
            auto bit = mouseButtonBit(button);
            if (!(m_pressedMouseButtons & bit))
                continue;
            EngawaInProcessMouseEvent release;
            release.type = EngawaInProcessMouseEventType::Up;
            release.x = m_lastMouseX;
            release.y = m_lastMouseY;
            release.button = button;
            release.buttons = m_pressedMouseButtons & ~bit;
            release.modifiers = m_lastMouseModifiers;
            release.clickCount = 1;
            if (!dispatchMouse(release, error))
                return false;
        }
        // WebCore has seen coherent releases. Suppress matching physical Ups
        // that were already accepted by the public queue before this blur.
        m_cancelledMouseButtons |= pressedBeforeBlur;
    }
    m_page->focusController().setActive(true);
    m_page->focusController().setFocused(focused);
    m_focused = focused;
    if (!focused)
        didChangeEditableFocus(false);
    else if (m_frame && m_frame->document()) {
        auto* element = m_frame->document()->focusedElement();
        didChangeEditableFocus(element
            && (element->isTextField() || element->isContentEditable()));
    }
    error = emptyString();
    return true;
}

bool EngawaInProcessWebCorePrototype::dispatchMouse(
    const EngawaInProcessMouseEvent& event, String& error)
{
    if (!checkOwnerAndLoaded(error))
        return false;
    if (!std::isfinite(event.x) || !std::isfinite(event.y)
        || (event.buttons & ~knownMouseButtonBits)
        || event.clickCount > static_cast<uint32_t>(std::numeric_limits<int>::max()))
        return fail(error, "The mouse event cannot be represented by WebCore."_s);
    auto modifiers = platformModifiers(event.modifiers);
    if (!modifiers)
        return fail(error, "The mouse event contains unsupported modifiers."_s);

    m_lastMouseX = event.x;
    m_lastMouseY = event.y;
    m_lastMouseModifiers = event.modifiers;

    const bool pointInside = event.x >= 0 && event.y >= 0
        && event.x < m_logicalWidth && event.y < m_logicalHeight;
    auto point = DoublePoint { event.x, event.y };
    PlatformEvent::Type platformType { PlatformEvent::Type::MouseMoved };
    MouseButton button { MouseButton::PointerHasNotChanged };
    uint32_t platformButtons = event.buttons;

    switch (event.type) {
    case EngawaInProcessMouseEventType::Move:
    case EngawaInProcessMouseEventType::Enter:
    case EngawaInProcessMouseEventType::Leave:
        // Motion and hover remain useful while the view is unfocused. They
        // also reconcile a release that an embedding window did not deliver
        // after focus cancellation.
        m_cancelledMouseButtons &= event.buttons;
        if (event.button != EngawaInProcessMouseButton::None
            || event.clickCount
            || event.buttons
                != (m_pressedMouseButtons | m_cancelledMouseButtons))
            return fail(error, "The mouse motion state is inconsistent with the view."_s);
        if (event.type == EngawaInProcessMouseEventType::Enter && !pointInside)
            return fail(error, "The mouse-enter point is outside the logical viewport."_s);
        if (event.type == EngawaInProcessMouseEventType::Leave)
            point = { -1, -1 };
        platformButtons = m_pressedMouseButtons;
        button = platformMouseButtonForMotion(platformButtons);
        break;
    case EngawaInProcessMouseEventType::Down: {
        auto bit = mouseButtonBit(event.button);
        m_cancelledMouseButtons &= ~bit;
        if (!pointInside || !bit || !event.clickCount
            || (m_pressedMouseButtons & bit)
            || event.buttons != (m_pressedMouseButtons | bit))
            return fail(error, "The mouse-down state is inconsistent with the view."_s);
        // UE delivers the first accepted Down before its subsequent Slate
        // focus callback. Focus synchronously only after full validation.
        if (!m_focused && !setFocus(true, error))
            return false;
        m_pressedMouseButtons = event.buttons;
        platformType = PlatformEvent::Type::MousePressed;
        button = platformMouseButton(event.button);
        break;
    }
    case EngawaInProcessMouseEventType::Up: {
        auto bit = mouseButtonBit(event.button);
        if (bit && (m_cancelledMouseButtons & bit)) {
            auto remainingCancelled = m_cancelledMouseButtons & ~bit;
            if (!event.clickCount
                || event.buttons
                    != (m_pressedMouseButtons | remainingCancelled))
                return fail(error, "The cancelled mouse-up state is inconsistent with the view."_s);
            m_cancelledMouseButtons = remainingCancelled;
            error = emptyString();
            return true;
        }
        if (!m_focused)
            return fail(error, "Mouse-up input requires a focused in-process view."_s);
        if (!bit || !event.clickCount || !(m_pressedMouseButtons & bit)
            || event.buttons != (m_pressedMouseButtons & ~bit))
            return fail(error, "The mouse-up state is inconsistent with the view."_s);
        m_pressedMouseButtons = event.buttons;
        platformType = PlatformEvent::Type::MouseReleased;
        button = platformMouseButton(event.button);
        break;
    }
    }

    EngawaPlatformMouseEvent platformEvent(point, button, platformType,
        static_cast<int>(event.clickCount), *modifiers, platformButtons);
    switch (platformType) {
    case PlatformEvent::Type::MouseMoved:
        m_frame->eventHandler().mouseMoved(platformEvent);
        break;
    case PlatformEvent::Type::MousePressed:
        m_frame->eventHandler().handleMousePressEvent(platformEvent);
        break;
    case PlatformEvent::Type::MouseReleased:
        m_frame->eventHandler().handleMouseReleaseEvent(platformEvent);
        break;
    default:
        ASSERT_NOT_REACHED();
        return fail(error, "The mouse event type is unsupported."_s);
    }
    // Input is not itself a visual change. WebCore's repaint and rendering-
    // update callbacks identify real hover, focus, scroll, editing and script
    // work without forcing a full capture for a nonvisual event.
    error = emptyString();
    return true;
}

bool EngawaInProcessWebCorePrototype::dispatchWheel(
    const EngawaInProcessWheelEvent& event, String& error)
{
    if (!checkOwnerAndLoaded(error))
        return false;
    if (!std::isfinite(event.x) || !std::isfinite(event.y)
        || !std::isfinite(event.deltaX) || !std::isfinite(event.deltaY))
        return fail(error, "The wheel deltas cannot be represented by WebCore."_s);
    auto modifiers = platformModifiers(event.modifiers);
    if (!modifiers)
        return fail(error, "The wheel event contains unsupported modifiers."_s);

    EngawaPlatformWheelEvent platformEvent(event, *modifiers);
    m_frame->eventHandler().handleWheelEvent(platformEvent, {
        WheelEventProcessingSteps::SynchronousScrolling,
        WheelEventProcessingSteps::BlockingDOMEventDispatch });
    error = emptyString();
    return true;
}

bool EngawaInProcessWebCorePrototype::dispatchKey(
    const EngawaInProcessKeyEvent& event, String& error)
{
    if (!checkOwnerAndLoaded(error))
        return false;
    if (!m_focused)
        return fail(error, "Key input requires a focused in-process view."_s);
    auto modifiers = platformModifiers(event.modifiers);
    auto virtualKey = windowsVirtualKeyCode(event.code);
    if (!modifiers || !virtualKey
        || event.location != keyLocationForCode(event.code))
        return fail(error, "The physical key or modifier state is unsupported."_s);

    auto type = event.type == EngawaInProcessKeyEventType::Down
        ? PlatformEvent::Type::RawKeyDown : PlatformEvent::Type::KeyUp;
    PlatformKeyboardEvent platformEvent(type, emptyString(), emptyString(),
        event.key, event.code, keyIdentifierForCode(event.code, *virtualKey),
        *virtualKey, event.isRepeat,
        event.location == EngawaInProcessKeyLocation::Numpad,
        modifiers->contains(PlatformEvent::Modifier::AltKey), *modifiers,
        MonotonicTime::now());
    m_frame->eventHandler().keyEvent(platformEvent);
    error = emptyString();
    return true;
}

void EngawaInProcessWebCorePrototype::handleKeyboardEvent(KeyboardEvent& event)
{
    auto commandName = editingCommandForKey(event);
    RefPtr node = dynamicDowncast<Node>(event.target());
    RefPtr frame = node ? node->document().frame() : nullptr;
    if (commandName.isEmpty() || !frame)
        return;
    auto& editor = frame->editor();
    bool handled = false;
    if (commandName == "Copy"_s || commandName == "Cut"_s) {
        const bool canEditClipboard = commandName == "Copy"_s
            ? editor.canCopy() : editor.canCut();
        if (canEditClipboard && m_clipboardProvider.canWrite()) {
            const auto selectedText = editor.selectedText();
            handled = m_clipboardProvider.writeText(
                m_clipboardProvider.context, selectedText);
            if (handled && commandName == "Cut"_s)
                editor.deleteSelectionWithSmartDelete(
                    editor.canSmartCopyOrDelete(), EditAction::Cut);
        }
    } else if (commandName == "Paste"_s) {
        String text;
        if (editor.canEdit() && m_clipboardProvider.canRead()
            && m_clipboardProvider.readText(m_clipboardProvider.context, text)) {
            editor.pasteAsPlainText(text, false);
            handled = true;
        }
    } else
        handled = editor.command(commandName).execute(&event);
    if (handled)
        event.setDefaultHandled();
}

bool EngawaInProcessWebCorePrototype::insertText(
    const String& text, String& error)
{
    if (!checkOwnerAndLoaded(error))
        return false;
    if (!m_focused)
        return fail(error, "Text input requires a focused in-process view."_s);
    if (text.isEmpty()) {
        error = emptyString();
        return true;
    }
    for (unsigned index = 0; index < text.length(); ++index) {
        auto codeUnit = text[index];
        if (codeUnit <= 0x1f
            || (codeUnit >= 0x7f && codeUnit <= 0x9f))
            return fail(error, "Text input cannot contain C0 or C1 controls."_s);
    }
    if (!m_frame->editor().insertText(text, nullptr))
        return fail(error, "WebCore rejected the inserted text."_s);
    error = emptyString();
    return true;
}

bool EngawaInProcessWebCorePrototype::dispatchHostMessage(
    const String& json, String& error)
{
    if (!checkOwnerAndLoaded(error))
        return false;

    auto& world = mainThreadNormalWorldSingleton();
    JSC::JSLockHolder lock(commonVM());
    auto* globalObject = m_frame->script().globalObject(world);
    if (!globalObject)
        return fail(error, "The trusted document has no JavaScript global object."_s);
    auto context = toRef(globalObject);
    auto jsonString = OpaqueJSString::tryCreate(json);
    if (!jsonString)
        return fail(error, "The host message exceeded available memory."_s);
    auto detail = JSValueMakeFromJSONString(context, jsonString.get());
    if (!detail)
        return fail(error, "The host message is not valid JSON."_s);

    auto propertyName = OpaqueJSString::tryCreate(
        "__engawaHostMessageDetail_6ee2d07f"_s);
    JSValueRef exception { nullptr };
    auto global = JSContextGetGlobalObject(context);
    JSObjectSetProperty(context, global, propertyName.get(), detail,
        kJSPropertyAttributeDontEnum, &exception);
    if (exception)
        return fail(error, "The host message could not be staged in the page."_s);

    auto result = m_frame->script().executeUserAgentScriptInWorld(world,
        "globalThis.dispatchEvent(new CustomEvent('engawa-message',{detail:globalThis.__engawaHostMessageDetail_6ee2d07f}))"_s,
        false);
    JSValueRef cleanupException { nullptr };
    JSObjectDeleteProperty(context, global, propertyName.get(),
        &cleanupException);
    if (!result) {
        error = result.error().message;
        return false;
    }
    if (cleanupException)
        return fail(error, "The host message staging value could not be removed."_s);
    error = emptyString();
    return true;
}

bool EngawaInProcessWebCorePrototype::stopLoading(String& error)
{
    if (!isOwnerThread())
        return fail(error, "stopLoading was called off the WebCore owner thread."_s);
    if (!m_frame)
        return fail(error, "The local frame is unavailable for stopLoading."_s);
    m_frame->loader().stopAllLoaders();
    error = emptyString();
    return true;
}

bool EngawaInProcessWebCorePrototype::resize(uint32_t width, uint32_t height,
    float deviceScale, String& error)
{
    if (!isOwnerThread())
        return fail(error, "resize was called off the WebCore owner thread."_s);
    uint32_t physicalWidth { 0 };
    uint32_t physicalHeight { 0 };
    if (!calculatePhysicalViewport(width, height, deviceScale,
            physicalWidth, physicalHeight, error))
        return false;
    if (!m_page || !m_frame || !m_frame->view())
        return fail(error, "The local frame view is unavailable for resize."_s);
    if (m_activeDamageTransaction)
        return fail(error, "The current snapshot transaction must be resolved before resize."_s);

    m_page->setDeviceScaleFactor(deviceScale);
    m_frame->view()->setFrameRect({ { }, {
        static_cast<int>(width), static_cast<int>(height) } });
    m_frame->view()->setTransparent(m_transparent);
    m_frame->view()->setBaseBackgroundColor(
        m_transparent ? Color::transparentBlack : Color::white);
    m_logicalWidth = width;
    m_logicalHeight = height;
    m_physicalWidth = physicalWidth;
    m_physicalHeight = physicalHeight;
    m_deviceScale = deviceScale;
    resetSnapshotSurface();
    m_pendingLogicalDamage = { };
    forceFullDamage();
    error = emptyString();
    return true;
}

bool EngawaInProcessWebCorePrototype::snapshotBGRA(EngawaInProcessBGRAFrame& output, String& error)
{
    output = { };
    if (!checkOwnerAndLoaded(error))
        return false;
    if (m_activeDamageTransaction)
        return fail(error, "The previous snapshot transaction has not been resolved."_s);
    if (!m_snapshotPixelOwner.expired())
        return fail(error, "The previous snapshot pixels are still leased."_s);
    if (!ensureSnapshotSurface(error))
        return false;

    const bool hadRenderingUpdateRequest = std::exchange(m_renderingUpdatePending, false);
    auto logicalDamage = m_pendingLogicalDamage;
    m_pendingLogicalDamage = { };
    const bool performsRenderingUpdate = !m_inRenderingUpdate;
    if (performsRenderingUpdate) {
        m_inRenderingUpdate = true;
        m_page->updateRendering();
        m_page->finalizeRenderingUpdate({ });
    }

    IntSize logicalSize { static_cast<int>(m_logicalWidth),
        static_cast<int>(m_logicalHeight) };
    IntSize physicalSize { static_cast<int>(m_physicalWidth),
        static_cast<int>(m_physicalHeight) };
    // Chrome invalidations emitted by updateRendering belong to this same
    // frame; incorporate them before opening the damage transaction. New
    // invalidations emitted during paint remain pending for the next frame.
    logicalDamage.unite(m_pendingLogicalDamage);
    m_pendingLogicalDamage = { };
    if (logicalDamage.isEmpty() && hadRenderingUpdateRequest) {
        // requestAnimationFrame and WebCore's rendering scheduler request an
        // update even when script changes no pixels. Complete that lifecycle
        // at full cadence, but do not turn a nonvisual callback into a full
        // paint, readback, CPU-frame publication, texture upload, and draw.
        if (performsRenderingUpdate) {
            m_page->didUpdateRendering();
            m_inRenderingUpdate = false;
        }
        m_invalidated = m_renderingUpdatePending
            || !m_pendingLogicalDamage.isEmpty();
        error = emptyString();
        return true;
    }
    // An explicit snapshot without a pending Chrome signal is unusual (the
    // runtime normally snapshots only after its invalidation latch fires),
    // but a full fallback keeps this private entry point correctness-complete.
    if (logicalDamage.isEmpty())
        logicalDamage = { { }, logicalSize };

    if (m_nextDamageTransaction == std::numeric_limits<uint64_t>::max())
        m_nextDamageTransaction = 1;
    else
        ++m_nextDamageTransaction;
    m_activeDamageTransaction = m_nextDamageTransaction;
    m_activeLogicalDamage = logicalDamage;

    const auto expectedBytes = static_cast<size_t>(m_physicalWidth)
        * m_physicalHeight * 4;
#if USE(SKIA) && PLATFORM(WPE)
    // WPE can conservatively invalidate the full root for a localized
    // removal. Preserve a copy of the prior raster only for that case so the
    // public damage remains as precise as the Windows and Cocoa paths.
    RefPtr<NativeImage> priorNativeImage;
    SkPixmap priorPixmap;
    const auto priorOpacityTileCount = m_opacityTileColumns * m_opacityTileRows;
    const bool canTightenFullDamage = logicalDamage == IntRect { { }, logicalSize }
        && !m_fullDamageWasForced
        && priorOpacityTileCount
        && m_knownOpacityTileCount == priorOpacityTileCount;
    if (canTightenFullDamage) {
        priorNativeImage = m_snapshotSurface->copyNativeImage();
        if (!priorNativeImage || !priorNativeImage->platformImage()
            || !priorNativeImage->platformImage()->peekPixels(&priorPixmap)
            || priorPixmap.colorType() != kBGRA_8888_SkColorType
            || priorPixmap.alphaType() != kPremul_SkAlphaType
            || priorPixmap.width() != physicalSize.width()
            || priorPixmap.height() != physicalSize.height()
            || priorPixmap.rowBytes() != static_cast<size_t>(m_physicalWidth) * 4
            || priorPixmap.computeByteSize() != expectedBytes
            || !priorPixmap.addr()) {
            priorNativeImage = nullptr;
        }
    }
#elif PLATFORM(COCOA)
    // Cocoa can conservatively invalidate the entire root view for a localized
    // removal. Compare the new readback with the complete CPU image retained
    // from the prior frame so tightening does not require a second full-surface
    // readback before every conservative repaint.
    RefPtr<PixelBuffer> priorPixelBuffer;
    const auto priorOpacityTileCount = m_opacityTileColumns * m_opacityTileRows;
    const bool canTightenFullDamage = logicalDamage == IntRect { { }, logicalSize }
        && priorOpacityTileCount
        && m_knownOpacityTileCount == priorOpacityTileCount
        && m_snapshotPixelBuffer;
    if (canTightenFullDamage) {
        priorPixelBuffer = m_snapshotPixelBuffer;
        if (priorPixelBuffer->format().alphaFormat != AlphaPremultiplication::Premultiplied
            || priorPixelBuffer->format().pixelFormat != PixelFormat::BGRA8
            || priorPixelBuffer->size() != physicalSize
            || priorPixelBuffer->bytes().size() != expectedBytes)
            priorPixelBuffer = nullptr;
    }
#endif

    // Damage incorporated into this paint is no longer a future invalidation.
    // A callback raised during paint or didUpdateRendering repopulates the
    // pending state and keeps the latch set for the next frame.
    m_invalidated = m_renderingUpdatePending;
    auto snapshotOptions = SnapshotOptions {
        { SnapshotFlags::InViewCoordinates },
        PixelFormat::BGRA8,
        DestinationColorSpace::SRGB()
    };
    bool painted = paintFrameRectIntoImageBuffer(*m_frame, *m_snapshotSurface,
        { { }, logicalSize }, logicalDamage, WTF::move(snapshotOptions));
    // didUpdateRendering() completes the update that this outer snapshot
    // started. Keep the nested-update guard asserted through painting, and
    // complete the lifecycle even when painting failed.
    if (performsRenderingUpdate) {
        m_page->didUpdateRendering();
        m_inRenderingUpdate = false;
    }
    if (!painted) {
        m_pendingLogicalDamage.unite(m_activeLogicalDamage);
        m_activeLogicalDamage = { };
        m_activeDamageTransaction = 0;
        m_invalidated = true;
        return fail(error, "WebCore could not paint the persistent CPU snapshot."_s);
    }
    if (m_snapshotSurface->backendSize() != physicalSize) {
        m_pendingLogicalDamage.unite(m_activeLogicalDamage);
        m_activeLogicalDamage = { };
        m_activeDamageTransaction = 0;
        m_invalidated = true;
        return fail(error, "WebCore created a snapshot with the wrong fractional-DPI backing size."_s);
    }

#if defined(ER_ENABLE_WATERMARK)
    // Composite into the same persistent surface and the same device-aligned
    // damage enclosure as the page paint. Retained pixels keep both labels on
    // unchanged frames, while damage through a label clears and repaints it
    // exactly once without broadening the public dirty rectangle.
    paintEngawaFrameWatermarks(*m_snapshotSurface, logicalSize, logicalDamage);
#endif

    m_snapshotSurface->flushDrawingContext();
    const uint8_t* snapshotPixels { nullptr };
    size_t snapshotRowBytes { 0 };
    std::shared_ptr<EngawaInProcessFramePixelOwner> pixelOwner;
    auto physicalDamage = physicalDamageRect(logicalDamage);
#if USE(SKIA)
    auto nativeImage = m_snapshotSurface->createNativeImageReference();
    SkPixmap pixmap;
    if (!nativeImage || !nativeImage->platformImage()
        || !nativeImage->platformImage()->peekPixels(&pixmap)
        || pixmap.colorType() != kBGRA_8888_SkColorType
        || pixmap.alphaType() != kPremul_SkAlphaType
        || pixmap.width() != physicalSize.width()
        || pixmap.height() != physicalSize.height()
        || pixmap.rowBytes() != static_cast<size_t>(m_physicalWidth) * 4
        || pixmap.computeByteSize() != expectedBytes
        || !pixmap.addr()) {
        m_pendingLogicalDamage.unite(m_activeLogicalDamage);
        m_activeLogicalDamage = { };
        m_activeDamageTransaction = 0;
        m_invalidated = true;
        return fail(error, "WebCore returned an incompatible CPU snapshot surface."_s);
    }
    snapshotPixels = static_cast<const uint8_t*>(pixmap.addr());
    snapshotRowBytes = pixmap.rowBytes();
    pixelOwner = std::make_shared<EngawaInProcessFramePixelOwner>(
        WTF::move(nativeImage));
#else
    const PixelBufferFormat pixelFormat {
        AlphaPremultiplication::Premultiplied,
        PixelFormat::BGRA8,
        DestinationColorSpace::SRGB()
    };
    const bool needsCompleteReadback = !m_snapshotPixelBuffer
        || (physicalDamage.x == 0 && physicalDamage.y == 0
            && physicalDamage.width == m_physicalWidth
            && physicalDamage.height == m_physicalHeight);
    IntRect readbackRect;
    if (needsCompleteReadback)
        readbackRect = { { }, physicalSize };
    else {
        readbackRect = {
            { physicalDamage.x, physicalDamage.y },
            { static_cast<int>(physicalDamage.width),
                static_cast<int>(physicalDamage.height) }
        };
    }
    auto readbackPixelBuffer = m_snapshotSurface->getPixelBufferForBackendRect(
        pixelFormat, readbackRect);
    const auto readbackSize = readbackRect.size();
    const auto expectedReadbackBytes = static_cast<size_t>(readbackSize.width())
        * readbackSize.height() * 4;
    auto readbackBytes = readbackPixelBuffer
        ? readbackPixelBuffer->bytes() : std::span<uint8_t> { };
    if (!readbackPixelBuffer
        || readbackPixelBuffer->format().alphaFormat != AlphaPremultiplication::Premultiplied
        || readbackPixelBuffer->format().pixelFormat != PixelFormat::BGRA8
        || readbackPixelBuffer->size() != readbackSize
        || readbackBytes.size() != expectedReadbackBytes
        || !readbackBytes.data()) {
        m_pendingLogicalDamage.unite(m_activeLogicalDamage);
        m_activeLogicalDamage = { };
        m_activeDamageTransaction = 0;
        m_invalidated = true;
        return fail(error, "WebCore returned an incompatible CPU pixel buffer."_s);
    }

    if (needsCompleteReadback)
        m_snapshotPixelBuffer = WTF::move(readbackPixelBuffer);
    else {
        auto completeBytes = m_snapshotPixelBuffer->bytes();
        if (m_snapshotPixelBuffer->format().alphaFormat != AlphaPremultiplication::Premultiplied
            || m_snapshotPixelBuffer->format().pixelFormat != PixelFormat::BGRA8
            || m_snapshotPixelBuffer->size() != physicalSize
            || completeBytes.size() != expectedBytes
            || !completeBytes.data()) {
            m_pendingLogicalDamage.unite(m_activeLogicalDamage);
            m_activeLogicalDamage = { };
            m_activeDamageTransaction = 0;
            m_invalidated = true;
            return fail(error, "The retained Cocoa CPU pixel buffer is incompatible."_s);
        }
        const auto completeRowBytes = static_cast<size_t>(m_physicalWidth) * 4;
        const auto readbackRowBytes = static_cast<size_t>(physicalDamage.width) * 4;
        for (uint32_t row = 0; row < physicalDamage.height; ++row) {
            auto source = readbackBytes.begin() + static_cast<size_t>(row) * readbackRowBytes;
            auto destination = completeBytes.begin()
                + (static_cast<size_t>(physicalDamage.y + row) * completeRowBytes)
                + static_cast<size_t>(physicalDamage.x) * 4;
            std::copy_n(source, readbackRowBytes, destination);
        }
    }
    auto pixelBytes = m_snapshotPixelBuffer->bytes();
    snapshotPixels = pixelBytes.data();
    snapshotRowBytes = static_cast<size_t>(m_physicalWidth) * 4;
    RefPtr<PixelBuffer> leasedPixelBuffer = m_snapshotPixelBuffer;
    pixelOwner = std::make_shared<EngawaInProcessFramePixelOwner>(
        WTF::move(leasedPixelBuffer));
#endif

#if USE(SKIA) && PLATFORM(WPE)
    if (priorNativeImage) {
        const auto* priorPixels = static_cast<const uint8_t*>(priorPixmap.addr());
        auto priorBytes = unsafeMakeSpan(priorPixels, expectedBytes);
        auto currentBytes = unsafeMakeSpan(snapshotPixels, expectedBytes);
        uint32_t left = m_physicalWidth;
        uint32_t top = m_physicalHeight;
        uint32_t right = 0;
        uint32_t bottom = 0;
        for (uint32_t y = 0; y < m_physicalHeight; ++y) {
            for (uint32_t x = 0; x < m_physicalWidth; ++x) {
                const auto offset = (static_cast<size_t>(y) * m_physicalWidth + x) * 4;
                auto priorPixel = priorBytes.subspan(offset, 4);
                auto currentPixel = currentBytes.subspan(offset, 4);
                if (std::equal(priorPixel.begin(), priorPixel.end(),
                        currentPixel.begin()))
                    continue;
                left = std::min(left, x);
                top = std::min(top, y);
                right = std::max(right, x + 1);
                bottom = std::max(bottom, y + 1);
            }
        }
        if (right > left && bottom > top) {
            physicalDamage = {
                static_cast<int32_t>(left), static_cast<int32_t>(top),
                right - left, bottom - top
            };
        } else {
            m_activeLogicalDamage = { };
            m_activeDamageTransaction = 0;
            m_invalidated = m_invalidated || m_renderingUpdatePending
                || !m_pendingLogicalDamage.isEmpty();
            error = emptyString();
            return true;
        }
    }
#endif

#if PLATFORM(COCOA) && !USE(SKIA)
    if (priorPixelBuffer) {
        auto priorBytes = priorPixelBuffer->bytes();
        uint32_t left = m_physicalWidth;
        uint32_t top = m_physicalHeight;
        uint32_t right = 0;
        uint32_t bottom = 0;
        for (uint32_t y = 0; y < m_physicalHeight; ++y) {
            for (uint32_t x = 0; x < m_physicalWidth; ++x) {
                const auto offset = (static_cast<size_t>(y) * m_physicalWidth + x) * 4;
                if (std::equal(priorBytes.begin() + offset,
                        priorBytes.begin() + offset + 4,
                        pixelBytes.begin() + offset))
                    continue;
                left = std::min(left, x);
                top = std::min(top, y);
                right = std::max(right, x + 1);
                bottom = std::max(bottom, y + 1);
            }
        }
        if (right > left && bottom > top) {
            physicalDamage = {
                static_cast<int32_t>(left), static_cast<int32_t>(top),
                right - left, bottom - top
            };
        } else {
            // Cocoa's conservative root invalidation repainted an identical
            // surface. Resolve the private damage transaction locally and
            // return the same coherent no-frame result used by nonvisual rAF;
            // there is nothing for the mailbox or Unreal texture to consume.
            m_activeLogicalDamage = { };
            m_activeDamageTransaction = 0;
            m_invalidated = m_invalidated || m_renderingUpdatePending
                || !m_pendingLogicalDamage.isEmpty();
            error = emptyString();
            return true;
        }
    }
#endif
    if (!updateOpacityForDamage(physicalDamage,
        snapshotPixels, snapshotRowBytes, error)) {
        m_pendingLogicalDamage.unite(m_activeLogicalDamage);
        m_activeLogicalDamage = { };
        m_activeDamageTransaction = 0;
        m_invalidated = true;
        return false;
    }

    output.width = m_physicalWidth;
    output.height = m_physicalHeight;
    output.rowBytes = m_physicalWidth * 4;
    output.pixels = snapshotPixels;
    output.pixelByteCount = expectedBytes;
    output.dirtyRect = physicalDamage;
    const auto opacityTileCount = m_opacityTileColumns * m_opacityTileRows;
    output.opacityKnown = opacityTileCount
        && m_knownOpacityTileCount == opacityTileCount;
    output.fullyOpaque = output.opacityKnown
        && m_opaqueTileCount == opacityTileCount;
    output.damageTransaction = m_activeDamageTransaction;
    output.pixelOwner = pixelOwner;
    m_snapshotPixelOwner = pixelOwner;
#if USE(SKIA) && PLATFORM(WPE)
    m_fullDamageWasForced = false;
#endif
    m_invalidated = m_invalidated || m_renderingUpdatePending
        || !m_pendingLogicalDamage.isEmpty();
    error = emptyString();
    return true;
}

bool EngawaInProcessWebCorePrototype::canSnapshot() const
{
    return isOwnerThread() && !m_activeDamageTransaction
        && m_snapshotPixelOwner.expired();
}

bool EngawaInProcessWebCorePrototype::commitSnapshot(
    uint64_t damageTransaction, String& error)
{
    if (!isOwnerThread())
        return fail(error, "commitSnapshot was called off the WebCore owner thread."_s);
    if (!damageTransaction || damageTransaction != m_activeDamageTransaction)
        return fail(error, "The snapshot transaction is not current."_s);
    m_activeLogicalDamage = { };
    m_activeDamageTransaction = 0;
    error = emptyString();
    return true;
}

bool EngawaInProcessWebCorePrototype::rollbackSnapshot(
    uint64_t damageTransaction, String& error)
{
    if (!isOwnerThread())
        return fail(error, "rollbackSnapshot was called off the WebCore owner thread."_s);
    if (!damageTransaction || damageTransaction != m_activeDamageTransaction)
        return fail(error, "The snapshot transaction is not current."_s);
    m_pendingLogicalDamage.unite(m_activeLogicalDamage);
    // The retained surface already contains the pixels whose publication just
    // failed. Drop the platform's retained state so the retry cannot compare
    // against that unpublished image, classify it as unchanged, and suppress
    // the required frame.
#if USE(SKIA) && PLATFORM(WPE)
    resetSnapshotSurface();
#else
    m_snapshotPixelBuffer = nullptr;
#endif
    m_activeLogicalDamage = { };
    m_activeDamageTransaction = 0;
    m_invalidated = true;
    error = emptyString();
    return true;
}

void EngawaInProcessWebCorePrototype::resetSnapshotSurface()
{
    m_snapshotSurface = nullptr;
    m_snapshotPixelBuffer = nullptr;
    m_snapshotPixelOwner.reset();
    m_opacityTiles.clear();
    m_opacityTileColumns = 0;
    m_opacityTileRows = 0;
    m_knownOpacityTileCount = 0;
    m_opaqueTileCount = 0;
}

bool EngawaInProcessWebCorePrototype::ensureSnapshotSurface(String& error)
{
    if (m_snapshotSurface)
        return true;
    if (!m_frame || !m_frame->document())
        return fail(error, "The document is unavailable for CPU snapshot allocation."_s);

    Ref document = *m_frame->document();
    auto hostWindow = (document->view() && document->view()->root())
        ? document->view()->root()->hostWindow() : nullptr;
    m_snapshotSurface = ImageBuffer::create(
        IntSize { static_cast<int>(m_logicalWidth), static_cast<int>(m_logicalHeight) },
        RenderingMode::Unaccelerated, RenderingPurpose::Snapshot,
        m_deviceScale, DestinationColorSpace::SRGB(), PixelFormat::BGRA8,
        hostWindow);
    if (!m_snapshotSurface)
        return fail(error, "WebCore could not create the persistent unaccelerated CPU snapshot."_s);
    IntSize expectedPhysicalSize {
        static_cast<int>(m_physicalWidth), static_cast<int>(m_physicalHeight)
    };
    if (m_snapshotSurface->backendSize() != expectedPhysicalSize) {
        resetSnapshotSurface();
        return fail(error, "WebCore created a persistent snapshot with the wrong fractional-DPI backing size."_s);
    }

    static constexpr uint32_t opacityTileSize = 32;
    m_opacityTileColumns = (m_physicalWidth + opacityTileSize - 1) / opacityTileSize;
    m_opacityTileRows = (m_physicalHeight + opacityTileSize - 1) / opacityTileSize;
    m_opacityTiles.fill(0, static_cast<size_t>(m_opacityTileColumns)
        * m_opacityTileRows);
    m_knownOpacityTileCount = 0;
    m_opaqueTileCount = 0;
    forceFullDamage();
    error = emptyString();
    return true;
}

EngawaInProcessPhysicalDamageRect EngawaInProcessWebCorePrototype::physicalDamageRect(
    const IntRect& logicalDamage) const
{
    const auto left = std::clamp<int64_t>(static_cast<int64_t>(std::floor(
        static_cast<double>(logicalDamage.x()) * m_deviceScale)), 0,
        m_physicalWidth);
    const auto top = std::clamp<int64_t>(static_cast<int64_t>(std::floor(
        static_cast<double>(logicalDamage.y()) * m_deviceScale)), 0,
        m_physicalHeight);
    const auto right = std::clamp<int64_t>(static_cast<int64_t>(std::ceil(
        static_cast<double>(logicalDamage.maxX()) * m_deviceScale)), left,
        m_physicalWidth);
    const auto bottom = std::clamp<int64_t>(static_cast<int64_t>(std::ceil(
        static_cast<double>(logicalDamage.maxY()) * m_deviceScale)), top,
        m_physicalHeight);
    return {
        static_cast<int32_t>(left),
        static_cast<int32_t>(top),
        static_cast<uint32_t>(right - left),
        static_cast<uint32_t>(bottom - top)
    };
}

bool EngawaInProcessWebCorePrototype::updateOpacityForDamage(
    const EngawaInProcessPhysicalDamageRect& damage, const uint8_t* pixels,
    size_t rowBytes, String& error)
{
    static constexpr uint32_t opacityTileSize = 32;
    static constexpr uint8_t unknownTile = 0;
    static constexpr uint8_t nonOpaqueTile = 1;
    static constexpr uint8_t opaqueTile = 2;
    if (!pixels || !damage.width || !damage.height || damage.x < 0
        || damage.y < 0
        || static_cast<uint64_t>(damage.x) + damage.width > m_physicalWidth
        || static_cast<uint64_t>(damage.y) + damage.height > m_physicalHeight
        || rowBytes < static_cast<size_t>(m_physicalWidth) * 4)
        return fail(error, "The CPU snapshot damage is outside its backing surface."_s);

    if (!m_transparent) {
        // The opaque view uses an opaque white base backing, so composited
        // output cannot contain a non-255 alpha byte. Keep exact public opacity
        // metadata without rescanning every damaged pixel of fullscreen UI.
        const auto tileCount = static_cast<uint32_t>(m_opacityTileColumns)
            * m_opacityTileRows;
        if (!tileCount || m_opacityTiles.size() != tileCount)
            return fail(error, "The opaque CPU snapshot tile state is incomplete."_s);
        if (m_knownOpacityTileCount != tileCount
            || m_opaqueTileCount != tileCount) {
            m_opacityTiles.fill(opaqueTile);
            m_knownOpacityTileCount = tileCount;
            m_opaqueTileCount = tileCount;
        }
        error = emptyString();
        return true;
    }

    const uint32_t firstColumn = static_cast<uint32_t>(damage.x) / opacityTileSize;
    const uint32_t firstRow = static_cast<uint32_t>(damage.y) / opacityTileSize;
    const uint32_t lastColumn = (static_cast<uint32_t>(damage.x)
        + damage.width - 1) / opacityTileSize;
    const uint32_t lastRow = (static_cast<uint32_t>(damage.y)
        + damage.height - 1) / opacityTileSize;
    for (uint32_t tileRow = firstRow; tileRow <= lastRow; ++tileRow) {
        const uint32_t yStart = tileRow * opacityTileSize;
        const uint32_t yEnd = std::min(yStart + opacityTileSize, m_physicalHeight);
        for (uint32_t tileColumn = firstColumn; tileColumn <= lastColumn; ++tileColumn) {
            const uint32_t xStart = tileColumn * opacityTileSize;
            const uint32_t xEnd = std::min(xStart + opacityTileSize, m_physicalWidth);
            bool isOpaque = true;
            for (uint32_t y = yStart; y < yEnd && isOpaque; ++y) {
                auto* alpha = pixels + static_cast<size_t>(y) * rowBytes
                    + static_cast<size_t>(xStart) * 4 + 3;
                for (uint32_t x = xStart; x < xEnd; ++x, alpha += 4) {
                    if (*alpha != 0xff) {
                        isOpaque = false;
                        break;
                    }
                }
            }

            const size_t tileIndex = static_cast<size_t>(tileRow)
                * m_opacityTileColumns + tileColumn;
            auto& priorState = m_opacityTiles[tileIndex];
            if (priorState == unknownTile)
                ++m_knownOpacityTileCount;
            else if (priorState == opaqueTile)
                --m_opaqueTileCount;
            priorState = isOpaque ? opaqueTile : nonOpaqueTile;
            if (priorState == opaqueTile)
                ++m_opaqueTileCount;
        }
    }
    error = emptyString();
    return true;
}

bool EngawaInProcessWebCorePrototype::pumpOnce(String& error)
{
    bool didWork { false };
    return pumpOnce(didWork, error);
}

bool EngawaInProcessWebCorePrototype::pumpOnce(bool& didWork, String& error)
{
    didWork = false;
    if (!isOwnerThread())
        return fail(error, "pumpOnce was called off the WebCore owner thread."_s);
#if USE(WINDOWS_EVENT_LOOP)
    auto result = RunLoop::cycleOnce();
    if (result == RunLoop::CycleResult::Stop)
        return fail(error, "The owner thread's Windows run loop requested termination."_s);
    didWork = result != RunLoop::CycleResult::Idle;
    error = emptyString();
    return true;
#elif USE(COCOA_EVENT_LOOP) && USE(CF)
    // Advance exactly one dispatched function, imminent WebKit timer, or Core
    // Foundation source. The Cocoa implementation suppresses recursive turns
    // and waits no more than one millisecond to promote an imminent timer.
    auto result = RunLoop::cycleOnce();
    if (result == RunLoop::CycleResult::Stop)
        return fail(error, "The owner thread's Cocoa run loop requested termination."_s);
    didWork = result != RunLoop::CycleResult::Idle;
    error = emptyString();
    return true;
#elif USE(GLIB_EVENT_LOOP)
    // WPE owns a GLib main context on the WebCore thread. Advance exactly one
    // nonblocking context iteration and preserve GLib's dispatch result so the
    // adapter can stop promptly when no platform work is ready. Recursive
    // turns are treated as idle, matching cycleOnce() on the other backends.
    if (inProcessGLibPumpActive) {
        error = emptyString();
        return true;
    }
    SetForScope pumpActive { inProcessGLibPumpActive, true };
    GMainContext* context = g_main_context_get_thread_default();
    if (!context)
        context = g_main_context_default();
    if (!context)
        return fail(error, "The owner thread has no GLib main context."_s);
    didWork = g_main_context_iteration(context, FALSE);
    error = emptyString();
    return true;
#else
    return fail(error, "The bounded Phase-1 pump is not implemented for this platform."_s);
#endif
}

uint64_t EngawaInProcessWebCorePrototype::currentThreadInvalidationGeneration()
{
    return inProcessInvalidationGeneration;
}

void EngawaInProcessWebCorePrototype::destroyCurrentThreadState()
{
    if (!inProcessRuntimeInitialized)
        return;

    RELEASE_ASSERT_WITH_MESSAGE(!liveInProcessPrototypeCount,
        "EngawaInProcessWebCorePrototype thread state must outlive every prototype.");
    auto current = currentThreadID();
    auto& runtimeState = engawaRuntimeState();
    {
        Locker locker { runtimeState.lock };
        RELEASE_ASSERT_WITH_MESSAGE(runtimeState.ownerThreadID
                && *runtimeState.ownerThreadID == current,
            "Engawa in-process WebCore thread state must be destroyed on its owner thread.");
    }

    // Match WebCore worker teardown ordering: first detach the shared timer so
    // it cannot call back through ThreadGlobalData during TLS destruction,
    // then invalidate and release FontCache-owned thread state explicitly.
    auto& threadData = threadGlobalDataSingleton();
    threadData.threadTimers().setSharedTimer(nullptr);
    threadData.destroy();
    inProcessRuntimeInitialized = false;
}

EngawaInProcessViewState EngawaInProcessWebCorePrototype::viewState() const
{
    return { m_invalidated, m_cursor, m_editableFocus };
}

void EngawaInProcessWebCorePrototype::clearInvalidation()
{
    RELEASE_ASSERT_WITH_MESSAGE(isOwnerThread(),
        "clearInvalidation must run on the WebCore owner thread.");
    m_invalidated = false;
}

void EngawaInProcessWebCorePrototype::renderingUpdateRequested()
{
    m_renderingUpdatePending = true;
    m_invalidated = true;
    ++inProcessInvalidationGeneration;
}

void EngawaInProcessWebCorePrototype::didInvalidate(const IntRect& rect)
{
    auto clippedRect = rect;
    clippedRect.intersect({ 0, 0, static_cast<int>(m_logicalWidth),
        static_cast<int>(m_logicalHeight) });
    if (clippedRect.isEmpty())
        return;
    m_pendingLogicalDamage.unite(clippedRect);
    m_invalidated = true;
    ++inProcessInvalidationGeneration;
}

void EngawaInProcessWebCorePrototype::forceFullDamage()
{
#if USE(SKIA) && PLATFORM(WPE)
    m_fullDamageWasForced = true;
#endif
    didInvalidate({ 0, 0, static_cast<int>(m_logicalWidth),
        static_cast<int>(m_logicalHeight) });
}

void EngawaInProcessWebCorePrototype::didChangeCursor(
    EngawaInProcessCursorType cursor)
{
    m_cursor = cursor;
}

void EngawaInProcessWebCorePrototype::didChangeEditableFocus(bool editable)
{
    m_editableFocus = m_focused && editable;
}

bool EngawaInProcessWebCorePrototype::isOwnerThread() const
{
    return m_ownerThreadID && currentThreadID() == m_ownerThreadID;
}

} // namespace WebCore
