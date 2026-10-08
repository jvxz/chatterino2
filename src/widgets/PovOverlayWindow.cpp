// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/PovOverlayWindow.hpp"

#include "common/Channel.hpp"
#include "widgets/splits/Split.hpp"

#ifdef Q_OS_MACOS
#    include "util/MacOsHelpers.h"

#    include <QWindow>
#endif

#include <QEvent>

namespace chatterino {

PovOverlayWindow::PovOverlayWindow()
#ifdef Q_OS_MACOS
    : split_(new Split(this))
    , macPanel_(createMacOSOverlayPanel())
#else
    : QWidget(nullptr,
              Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , split_(new Split(this))
#endif
{
#ifdef Q_OS_MACOS
    // The panel is the window; this widget fills its content view
    this->winId();
    this->windowHandle()->setParent(QWindow::fromWinId(this->macPanel_));
#else
    // Keep the focus in the browser when the overlay shows up
    this->setAttribute(Qt::WA_ShowWithoutActivating);
#endif

    this->hideTimer_.setSingleShot(true);
    this->hideTimer_.setInterval(250);
    QObject::connect(&this->hideTimer_, &QTimer::timeout, this, [this] {
        this->hideOverlay();
    });
}

void PovOverlayWindow::showAt(const QRect &panel, const ChannelPtr &channel)
{
    this->hideTimer_.stop();

    if (this->split_->getChannel() != channel)
    {
        this->split_->setChannel(channel);
    }

#ifdef Q_OS_MACOS
    showMacOSOverlayPanel(this->macPanel_, panel);
    const QRect geometry(QPoint(0, 0), panel.size());
#else
    const QRect &geometry = panel;
#endif
    if (this->geometry() != geometry)
    {
        this->setGeometry(geometry);
    }
    if (!this->isVisible())
    {
        this->show();
    }
}

void PovOverlayWindow::requestHide()
{
    if (this->isActiveWindow())
    {
        return;
    }
    this->hideTimer_.stop();
    this->hideOverlay();
}

void PovOverlayWindow::hideOverlay()
{
    this->hide();
#ifdef Q_OS_MACOS
    hideMacOSOverlayPanel(this->macPanel_);
#endif
}

void PovOverlayWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::ActivationChange && !this->isActiveWindow() &&
        this->isVisible())
    {
        this->hideTimer_.start();
    }
    QWidget::changeEvent(event);
}

void PovOverlayWindow::resizeEvent(QResizeEvent *event)
{
    // Placed by hand rather than with a layout: a layout would make the split's
    // minimum size the window's, which is wider than the site's chat.
    this->split_->setGeometry(this->rect());
    QWidget::resizeEvent(event);
}

}  // namespace chatterino
