// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "singletons/MultiPovSync.hpp"
#include "widgets/BaseWidget.hpp"

#include <pajlada/signals/signal.hpp>

#include <array>

namespace chatterino {

/// A row of "Twitch" and "Kick" tabs, drawn like the tabs of a window's
/// notebook, to pick which platform's chat to show.
class PlatformToggle : public BaseWidget
{
public:
    explicit PlatformToggle(QWidget *parent);

    void setPlatform(multipov::Platform platform);

    /// The height the row needs at the current scale
    int preferredHeight() const;

    /// The user picked a platform that wasn't selected
    pajlada::Signals::Signal<multipov::Platform> platformPicked;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    struct Tab {
        multipov::Platform platform;
        QString title;
    };
    static const std::array<Tab, 2> TABS;

    /// Where each tab is, in TABS order
    std::array<QRect, 2> tabRects() const;
    /// The tab under `pos`, or -1
    int tabAt(QPoint pos) const;

    multipov::Platform platform_ = multipov::Platform::Twitch;
    int hovered_ = -1;
};

}  // namespace chatterino
