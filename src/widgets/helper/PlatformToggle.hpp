// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "singletons/MultiPovSync.hpp"
#include "widgets/BaseWidget.hpp"

#include <pajlada/signals/signal.hpp>

namespace chatterino {

/// A small "Twitch | Kick" switch in the top right corner of its parent, to
/// pick which platform's chat to show.
class PlatformToggle : public BaseWidget
{
public:
    explicit PlatformToggle(QWidget *parent);

    void setPlatform(multipov::Platform platform);

    /// The user picked a platform that wasn't selected
    pajlada::Signals::Signal<multipov::Platform> platformPicked;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void scaleChangedEvent(float scale) override;

private:
    /// Resizes to fit the labels and moves to the parent's corner
    void updateGeometry_();

    QFont font_() const;

    multipov::Platform platform_ = multipov::Platform::Twitch;
};

}  // namespace chatterino
