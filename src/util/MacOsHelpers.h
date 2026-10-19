#pragma once

#include <QString>
#include <qwindowdefs.h>

#include <functional>

class QImage;

void chatterinoSetMacOsActivationPolicyProhibited();

namespace chatterino {

#ifdef Q_OS_DARWIN
QString getMacOSDefaultBrowserPath();

/// When clicking the overlay window (`overlayWinId`) activates Chatterino,
/// Qt brings all of Chatterino's windows to the front. This puts the other
/// windows back behind the window under the overlay (the browser). Windows
/// that don't overlap the browser stay visible.
void keepMacOSWindowsBehindOverlay(WId overlayWinId);

/// Shows Chatterino in the Dock and the app switcher, or only in the menu bar
void setMacOSDockIconVisible(bool visible);

/// Makes Chatterino the active app, which it doesn't become on its own while
/// it's not in the Dock
void activateMacOSApp();

/// Number of the window of another app right under the overlay window
/// (`overlayWinId`), i.e. the browser's, or 0
qint64 getMacOSWindowUnder(WId overlayWinId);

/// The window of another app (than Chatterino) in front of `window` (from
/// getMacOSWindowUnder) over the overlay window, described for logs. Empty
/// when nothing covers `window` there.
QString getMacOSOverlayCover(WId overlayWinId, qint64 window);

/// Calls `callback` with the process ID of every app that comes to the front,
/// including Chatterino itself
void onMacOSAppActivated(std::function<void(qint64 pid)> callback);

/// An icon in the menu bar that opens a menu when clicked. It's removed when
/// this is destroyed, which mustn't happen from one of its own actions.
class MacOSStatusItem
{
public:
    /// `icon` is drawn as a template image, i.e. in the menu bar's text color
    /// using only its alpha channel
    MacOSStatusItem(const QImage &icon, const QString &toolTip);
    ~MacOSStatusItem();
    MacOSStatusItem(const MacOSStatusItem &) = delete;
    MacOSStatusItem &operator=(const MacOSStatusItem &) = delete;
    MacOSStatusItem(MacOSStatusItem &&) = delete;
    MacOSStatusItem &operator=(MacOSStatusItem &&) = delete;

    void addAction(const QString &title, std::function<void()> callback);
    void addSeparator();

private:
    void *item_;    // NSStatusItem
    void *target_;  // ChatterinoStatusItemTarget, receives the menu's clicks
};
#endif

}  // namespace chatterino
