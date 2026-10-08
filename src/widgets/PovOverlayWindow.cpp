// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/PovOverlayWindow.hpp"

#include "common/Channel.hpp"
#include "widgets/splits/Split.hpp"

#include <QEvent>
#include <QVBoxLayout>

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

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(this->split_);

    this->hideTimer_.setSingleShot(true);
    this->hideTimer_.setInterval(250);
    QObject::connect(&this->hideTimer_, &QTimer::timeout, this, &QWidget::hide);
}

void PovOverlayWindow::showAt(const QRect &rect, const ChannelPtr &channel)
{
    this->hideTimer_.stop();

    if (this->split_->getChannel() != channel)
    {
        this->split_->setChannel(channel);
    }
    if (this->geometry() != rect)
    {
        this->setGeometry(rect);
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

}  // namespace chatterino
