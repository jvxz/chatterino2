#pragma once

#include <QString>
#include <qwindowdefs.h>

void chatterinoSetMacOsActivationPolicyProhibited();

namespace chatterino {

#ifdef Q_OS_DARWIN
QString getMacOSDefaultBrowserPath();

/// Lets the window take clicks and typing without activating Chatterino, so
/// the app's other windows don't come to the front with it. Only works for
/// Qt::Tool windows, which are NSPanels.
void makeMacOSWindowNonActivating(WId winId);
#endif

}  // namespace chatterino
