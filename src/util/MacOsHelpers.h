#pragma once

#include <QRect>
#include <QString>
#include <qwindowdefs.h>

void chatterinoSetMacOsActivationPolicyProhibited();

namespace chatterino {

#ifdef Q_OS_DARWIN
QString getMacOSDefaultBrowserPath();

/// Creates a borderless panel that floats above other apps' windows and takes
/// clicks and typing without activating Chatterino, so Chatterino's other
/// windows stay where they are. Returns its content view, for a QWindow to be
/// embedded in with QWindow::fromWinId.
///
/// Qt can't create such a window itself: macOS only routes typing to a
/// non-activating panel when it's created as one.
WId createMacOSOverlayPanel();

/// Moves the panel to `rect` (global, logical coordinates) and shows it
/// without making it key.
void showMacOSOverlayPanel(WId contentView, const QRect &rect);

void hideMacOSOverlayPanel(WId contentView);
#endif

}  // namespace chatterino
