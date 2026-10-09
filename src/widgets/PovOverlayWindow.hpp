// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QWidget>

#include <memory>

namespace chatterino {

class Split;
class Channel;
using ChannelPtr = std::shared_ptr<Channel>;

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

    /// Shows the window over `panel` (global, logical coordinates) with
    /// `channel`, without taking focus from the browser.
    void showAt(const QRect &panel, const ChannelPtr &channel);

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

    Split *split_;

    /// macOS window number of the browser window the overlay was last shown
    /// over
    qint64 browserWindow_ = 0;
};

}  // namespace chatterino
