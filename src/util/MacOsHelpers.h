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

/// Number of the window of another app right under the overlay window
/// (`overlayWinId`), i.e. the browser's, or 0
qint64 getMacOSWindowUnder(WId overlayWinId);

/// Whether a window of another app (than Chatterino) is in front of `window`
/// (from getMacOSWindowUnder) over the overlay window, or `window` is gone
bool isMacOSOverlayCovered(WId overlayWinId, qint64 window);

/// Calls `callback` with the process ID of every app that comes to the front,
/// including Chatterino itself
void onMacOSAppActivated(std::function<void(qint64 pid)> callback);
#endif

}  // namespace chatterino
