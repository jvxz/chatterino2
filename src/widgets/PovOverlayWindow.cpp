// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/PovOverlayWindow.hpp"

#include "common/Channel.hpp"
#include "widgets/splits/Split.hpp"

#ifdef Q_OS_MACOS
#    include "util/MacOsHelpers.h"
#endif

#include <QEvent>

namespace chatterino {

PovOverlayWindow::PovOverlayWindow()
    : QWidget(nullptr,
              Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , split_(new Split(this))
{
    // Tool windows are hidden while Chatterino isn't the active app, which is
    // always the case while the browser is focused.
    this->setAttribute(Qt::WA_MacAlwaysShowToolWindow);
    // Keep the focus in the browser when the overlay shows up
    this->setAttribute(Qt::WA_ShowWithoutActivating);

    this->hideTimer_.setSingleShot(true);
    this->hideTimer_.setInterval(250);
    QObject::connect(&this->hideTimer_, &QTimer::timeout, this, &QWidget::hide);
}

void PovOverlayWindow::showAt(const QRect &panel, const ChannelPtr &channel)
{
    this->hideTimer_.stop();

    if (this->split_->getChannel() != channel)
    {
        this->split_->setChannel(channel);
    }
    if (this->geometry() != panel)
    {
        this->setGeometry(panel);
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
    this->hide();
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

void PovOverlayWindow::showEvent(QShowEvent *event)
{
#ifdef Q_OS_MACOS
    // Clicking the overlay would otherwise activate Chatterino, which brings
    // the main window up in front of the browser too.
    makeMacOSWindowNonActivating(this->winId());
#endif
    QWidget::showEvent(event);
}

void PovOverlayWindow::resizeEvent(QResizeEvent *event)
{
    // Placed by hand rather than with a layout: a layout would make the split's
    // minimum size the window's, which is wider than the site's chat.
    this->split_->setGeometry(this->rect());
    QWidget::resizeEvent(event);
}

}  // namespace chatterino
