#pragma once
// Hover that never sticks (Direction Update 2, and the GarageBand hotfix). In GarageBand a
// control could stay highlighted after the mouse had left it. The cause was on both sides:
//  - the framework (JUCE 8.0.16, juce_NSViewComponentPeer_mac.mm) drops a mouseExited event
//    while a mouse button is held (a click and a quick move away) and when the event belongs
//    to a tracking area the view has since replaced (the view relaid or resized), and a
//    host's out-of-process view may not forward an exit at all (window not key, the mouse
//    leaving the plug-in's window quickly);
//  - our code relied on those events only: the hover was cleared on a mouse exit, or by a
//    sweep on the next mouse event anywhere in the window. When no event came, nothing ran.
// So now:
//  - HoverButton keeps its own hover flag, set on enter and move, cleared on exit and
//    whenever it is hidden, and paints from it;
//  - sweepHover() runs on every mouse move, enter and exit anywhere in the window, and
//  - HoverGuard runs on the window's existing 10 Hz UI timer: it reads the real mouse
//    position from the system and clears every hover the mouse is no longer over (ours and
//    the framework's: it sends the framework the exit it missed, so the sliders, combo
//    boxes and text buttons forget the mouse too), and clears all of them when the window
//    loses focus or the mouse leaves the editor. Never while a mouse button is held.
// The selected place is a separate, stable state (its toggle state: the gold diamond).
#include <juce_gui_basics/juce_gui_basics.h>
#include <optional>

namespace dc::ui {

class HoverButton : public juce::Button {
public:
    explicit HoverButton(const juce::String& name) : juce::Button(name) {}
    bool hot() const noexcept { return hovered; }
    void setHot(bool on) {
        if (on == hovered) return;
        hovered = on;
        repaint();
        if (onHover) onHover();
    }
    std::function<void()> onHover;   // the hover flag changed
    void mouseEnter(const juce::MouseEvent& e) override { juce::Button::mouseEnter(e); setHot(true); }
    void mouseMove(const juce::MouseEvent& e) override { juce::Button::mouseMove(e); setHot(true); }
    void mouseExit(const juce::MouseEvent& e) override { juce::Button::mouseExit(e); setHot(false); }
    void visibilityChanged() override {
        juce::Button::visibilityChanged();
        if (!isVisible()) setHot(false);
    }
    void paintButton(juce::Graphics& g, bool, bool down) override { paintHover(g, hovered && isEnabled(), down && isMouseButtonDown()); }
    virtual void paintHover(juce::Graphics&, bool hover, bool down) = 0;

private:
    bool hovered = false;
};

// A control with a hover of its own that is not a button (the search results' rows): it
// clears it when the mouse (screen coordinates) is not over it.
class Hoverable {
public:
    virtual ~Hoverable() = default;
    // Returns true when a hover was shown and is now cleared.
    virtual bool clearHoverUnless(juce::Point<float> screen) = 0;
    virtual bool hoverShown() const = 0;
};

namespace detail {
// Visible all the way up (the top level's own visibility belongs to its window: a test's
// editor has none) and the point inside its bounds (screen coordinates).
inline bool over(juce::Component& c, juce::Point<float> screen) {
    for (auto* p = &c; p != nullptr && p->getParentComponent() != nullptr; p = p->getParentComponent())
        if (!p->isVisible()) return false;
    return c.getScreenBounds().toFloat().contains(screen);
}
} // namespace detail

// Clears every stale hover under `root` for a mouse at `screen` (screen coordinates).
// Cheap: a walk over the window's few dozen controls.
inline int sweepHover(juce::Component& root, juce::Point<float> screen) {
    int cleared = 0;
    std::function<void(juce::Component&)> walk = [&](juce::Component& c) {
        for (auto* child : c.getChildren()) {
            if (auto* b = dynamic_cast<juce::Button*>(child)) {
                if (!detail::over(*b, screen)) {
                    if (auto* h = dynamic_cast<HoverButton*>(b); h != nullptr && h->hot()) { h->setHot(false); ++cleared; }
                    if (b->getState() == juce::Button::buttonOver) { b->setState(juce::Button::buttonNormal); ++cleared; }
                }
            }
            if (auto* h = dynamic_cast<Hoverable*>(child)) cleared += h->clearHoverUnless(screen) ? 1 : 0;
            walk(*child);
        }
    };
    walk(root);
    return cleared;
}

// Every hover under `root`, ours and the framework's (tests and the log).
inline int countHovers(juce::Component& root) {
    int lit = 0;
    std::function<void(juce::Component&)> walk = [&](juce::Component& c) {
        for (auto* child : c.getChildren()) {
            if (auto* h = dynamic_cast<HoverButton*>(child); h != nullptr && h->hot()) ++lit;
            else if (auto* b = dynamic_cast<juce::Button*>(child); b != nullptr && b->getState() == juce::Button::buttonOver) ++lit;
            if (auto* h = dynamic_cast<Hoverable*>(child); h != nullptr && h->hoverShown()) ++lit;
            walk(*child);
        }
    };
    walk(root);
    return lit;
}

// The guard: the real mouse position against every hoverable control, on a timer.
class HoverGuard final : private juce::MouseListener {
public:
    explicit HoverGuard(juce::Component& r) : root(r) {
        root.addMouseListener(this, true);
        if (const auto path = juce::SystemStats::getEnvironmentVariable("LIGHTSEEKER_HOVER_LOG", {}); path.isNotEmpty())
            logFile = juce::File::isAbsolutePath(path) ? juce::File(path) : defaultLogFile();
        else if (defaultLogFile().getSiblingFile("hover-log-on").existsAsFile())
            logFile = defaultLogFile();
    }
    ~HoverGuard() override { root.removeMouseListener(this); }

    // What happened (tests, the AU harness and the log).
    struct Stats {
        int enters = 0, exits = 0, moves = 0;      // mouse events the editor received
        int missedExits = 0;                      // hovers the timer cleared (no exit had come)
        int focusClears = 0;                      // cleared because the window lost focus
        int outsideClears = 0;                    // cleared because the mouse left the editor
        int framework = 0;                        // exits / moves sent to the framework
    };
    const Stats& stats() const noexcept { return counts; }

    // Tests: the real mouse (screen coordinates; nullopt = unknown), whether a button is
    // held and whether the window has focus, instead of asking the system.
    struct Probe {
        std::function<std::optional<juce::Point<float>>()> mouse;
        std::function<bool()> buttonDown, focused;
    };
    void setProbeForTest(Probe p) { probe = std::move(p); }

    // On the UI timer (10 Hz). Cheap: one system call and a walk over a few dozen controls,
    // and only when something is lit or JUCE thinks the mouse is in the editor.
    void tick() {
        if (!root.isShowing() && !probe.mouse) return;
        const bool down = probe.buttonDown ? probe.buttonDown() : juce::ModifierKeys::getCurrentModifiersRealtime().isAnyMouseButtonDown();
        const bool focused = probe.focused ? probe.focused() : windowFocused();
        const bool lostFocus = wasFocused && !focused;
        wasFocused = focused;
        if (down) return;   // never during a click or a drag
        const auto real = realMouse();
        if (!real) return;
        auto* under = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse();
        const bool frameworkInside = under != nullptr && (under == &root || root.isParentOf(under));
        if (!frameworkInside && countHovers(root) == 0) return;
        const bool inside = detail::over(root, *real);
        if (lostFocus || !inside) {
            // Everything goes: the framework is told the mouse left, then our own flags.
            if (frameworkInside) sendExitToFramework();
            const int n = sweepHover(root, { -1.0e6f, -1.0e6f });
            if (n > 0 || frameworkInside) {
                (lostFocus ? counts.focusClears : counts.outsideClears) += 1;
                log(juce::String(lostFocus ? "window lost focus" : "mouse left the editor") + ": cleared " + juce::String(n) + " hover(s)"
                    + (frameworkInside ? ", sent the framework its missed exit" : "") + at(*real));
            }
            return;
        }
        // Inside: the framework's idea of what is under the mouse, then each control.
        if (frameworkInside) {
            auto* really = root.getComponentAt(root.getLocalPoint(nullptr, *real).roundToInt());
            if (really != under) sendMoveToFramework(*real);
        }
        if (const int n = sweepHover(root, *real); n > 0) {
            counts.missedExits += n;
            log("missed exit: cleared " + juce::String(n) + " hover(s) the mouse is no longer over" + at(*real));
        }
    }

    juce::File logFileForTest() const { return logFile; }

private:
    juce::Component& root;
    Probe probe;
    Stats counts;
    bool wasFocused = false, synthesising = false;
    // The system's mouse position minus the one in the events (the same once calibrated:
    // an out-of-process view can report another origin), from the last real mouse event.
    juce::Point<float> offset;
    juce::File logFile;
    juce::int64 lastMoveLog = 0;

    static juce::File defaultLogFile() {
       #if JUCE_MAC
        return juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("Library/Logs/LightSeeker/hover.log");
       #else
        return juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("LightSeeker/hover.log");
       #endif
    }
    bool windowFocused() const {
        if (auto* peer = root.getPeer()) return peer->isFocused() || root.hasKeyboardFocus(true);
        return false;
    }
    std::optional<juce::Point<float>> realMouse() const {
        if (probe.mouse) return probe.mouse();
        return juce::Desktop::getMousePosition().toFloat() - offset;
    }
    void sendExitToFramework() {
        auto* peer = root.getPeer();
        if (peer == nullptr) return;
        const juce::ScopedValueSetter<bool> s(synthesising, true);
        peer->handleMouseEvent(juce::MouseInputSource::InputSourceType::mouse, juce::MouseInputSource::offscreenMousePos,
                               juce::ModifierKeys::currentModifiers.withoutMouseButtons(), juce::MouseInputSource::defaultPressure,
                               juce::MouseInputSource::defaultOrientation, juce::Time::currentTimeMillis());
        ++counts.framework;
    }
    void sendMoveToFramework(juce::Point<float> screen) {
        auto* peer = root.getPeer();
        if (peer == nullptr) return;
        const juce::ScopedValueSetter<bool> s(synthesising, true);
        peer->handleMouseEvent(juce::MouseInputSource::InputSourceType::mouse, peer->globalToLocal(screen),
                               juce::ModifierKeys::currentModifiers.withoutMouseButtons(), juce::MouseInputSource::defaultPressure,
                               juce::MouseInputSource::defaultOrientation, juce::Time::currentTimeMillis());
        ++counts.framework;
    }
    void calibrate(const juce::MouseEvent& e) {
        if (synthesising || probe.mouse) return;
        offset = juce::Desktop::getMousePosition().toFloat() - e.getScreenPosition().toFloat();
        if (offset.getDistanceFromOrigin() < 3.0f) offset = {};
    }
    juce::String at(juce::Point<float> p) const {
        return " (mouse " + juce::String(juce::roundToInt(p.x)) + ", " + juce::String(juce::roundToInt(p.y)) + "; editor "
               + root.getScreenBounds().toString() + ")";
    }
    static juce::String nameOf(const juce::MouseEvent& e) {
        auto* c = e.eventComponent;
        if (c == nullptr) return "?";
        juce::String n = c->getName();
        if (auto* b = dynamic_cast<juce::Button*>(c); b != nullptr && n.isEmpty()) n = b->getButtonText();
        return juce::String(typeid(*c).name()).fromLastOccurrenceOf("::", false, false).retainCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789")
               + (n.isNotEmpty() ? " '" + n + "'" : juce::String());
    }
    void log(const juce::String& line) {
        if (logFile == juce::File()) return;
        logFile.getParentDirectory().createDirectory();
        logFile.appendText(juce::Time::getCurrentTime().formatted("%H:%M:%S") + juce::String::formatted(".%03d ", juce::Time::getCurrentTime().getMilliseconds())
                           + line + (windowFocused() ? "" : " [window not focused]") + "\n");
    }
    void mouseEnter(const juce::MouseEvent& e) override {
        if (synthesising) return;
        ++counts.enters;
        calibrate(e);
        log("enter " + nameOf(e) + at(e.getScreenPosition().toFloat()));
    }
    void mouseExit(const juce::MouseEvent& e) override {
        if (synthesising) return;
        ++counts.exits;
        log("exit " + nameOf(e) + at(e.getScreenPosition().toFloat()));
    }
    void mouseMove(const juce::MouseEvent& e) override {
        if (synthesising) return;
        ++counts.moves;
        calibrate(e);
        if (const auto now = juce::Time::currentTimeMillis(); now - lastMoveLog >= 100) {
            lastMoveLog = now;
            log("move " + nameOf(e) + at(e.getScreenPosition().toFloat()));
        }
    }
};

} // namespace dc::ui
