#pragma once

#include <QString>
#include <qwindowdefs.h>

#include <functional>

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

/// Process ID of the app that's in the front right now
qint64 getMacOSFrontmostAppPid();

/// Calls `callback` with the process ID of every app that comes to the front,
/// including Chatterino itself
void onMacOSAppActivated(std::function<void(qint64 pid)> callback);
#endif

}  // namespace chatterino
