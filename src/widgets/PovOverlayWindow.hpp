// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QTimer>
#include <QWidget>

#include <memory>
#include <optional>

namespace chatterino {

class Split;
class Channel;
using ChannelPtr = std::shared_ptr<Channel>;

/// A borderless window that sits on top of the browser's chat panel on
/// lofi-nopixel.com and shows the selected POV's chat.
///
/// Unlike AttachedWindow, this doesn't track the browser window natively, so it
/// works on macOS too: the browser extension sends the panel's position in
/// screen coordinates whenever it changes.
///
/// Its left edge can be dragged to make it wider or narrower than the panel,
/// keeping its right edge on the panel's. Double-clicking the edge goes back to
/// the panel's width.
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

protected:
    void changeEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    /// The window's geometry over `panel`, with the width it was dragged to
    QRect geometryFor(const QRect &panel) const;

    void dragTo(int globalX);

    Split *split_;
    /// Strip along the left edge that resizes the window
    QWidget *resizeGrip_;

    /// Last position of the browser's chat panel
    QRect panel_;

    struct Drag {
        int startX;
        int startWidth;
    };
    std::optional<Drag> drag_;

    /// Hides the window shortly after it loses focus, unless the browser
    /// shows it again in the meantime (focus went back to the browser).
    QTimer hideTimer_;
};

}  // namespace chatterino
