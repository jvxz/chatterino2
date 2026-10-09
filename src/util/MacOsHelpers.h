#pragma once

#include <QString>
#include <qwindowdefs.h>

void chatterinoSetMacOsActivationPolicyProhibited();

namespace chatterino {

#ifdef Q_OS_DARWIN
QString getMacOSDefaultBrowserPath();

/// When clicking the overlay window (`overlayWinId`) activates Chatterino,
/// Qt brings all of Chatterino's windows to the front. This puts the other
/// windows back behind the window under the overlay (the browser). Windows
/// that don't overlap the browser stay visible.
void keepMacOSWindowsBehindOverlay(WId overlayWinId);
#endif

}  // namespace chatterino
