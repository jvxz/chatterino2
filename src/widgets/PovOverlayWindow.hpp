// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "singletons/MultiPovSync.hpp"

#include <QWidget>

#include <memory>
#include <optional>

namespace chatterino {

class PlatformToggle;
class Split;

/// A borderless window that sits on top of the browser's chat panel on
/// lofi-nopixel.com and shows the selected POV's chat.
///
/// Unlike AttachedWindow, this doesn't track the browser window natively, so it
/// works on macOS too: the browser extension sends the panel's position in
/// screen coordinates whenever it changes. It always matches the panel; the
/// extension adds a handle to resize the panel on the page.
class PovOverlayWindow : public QWidget
{
public:
    PovOverlayWindow();

    /// Shows the window over `panel` (global, logical coordinates) with the
    /// chat of `pov`, without taking focus from the browser.
    void showAt(const QRect &panel, const multipov::Pov &pov);

    /// Hides the window, unless it's being used right now (the browser loses
    /// focus when the overlay is clicked).
    void requestHide();

    /// Hides the window when another app's window is in front of the browser
    /// window it's over. The overlay stays on top of everything, so it would
    /// cover that window otherwise.
    void hideIfCovered();

protected:
    void changeEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    /// Whether the overlay has keyboard focus, i.e. it's being used
    bool isFocused() const;

    /// Shows the chat of pov_ on the platform that is chosen for it
    void updateChat();

    Split *split_;
    PlatformToggle *toggle_;
    std::optional<multipov::Pov> pov_;

    /// macOS window number of the browser window the overlay was last shown
    /// over
    qint64 browserWindow_ = 0;
};

}  // namespace chatterino
