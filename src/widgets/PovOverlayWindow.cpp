// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/PovOverlayWindow.hpp"

#include "common/Channel.hpp"
#include "singletons/Settings.hpp"
#include "widgets/splits/Split.hpp"

#ifdef Q_OS_MACOS
#    include "util/MacOsHelpers.h"
#endif

#include <QEvent>
#include <QMouseEvent>
#include <QScreen>

#include <algorithm>

namespace chatterino {

namespace {

constexpr int RESIZE_GRIP_WIDTH = 6;
constexpr int MIN_WIDTH = 150;

}  // namespace

PovOverlayWindow::PovOverlayWindow()
    : QWidget(nullptr,
              Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , split_(new Split(this))
    , resizeGrip_(new QWidget(this))
{
    // Tool windows are hidden while Chatterino isn't the active app, which is
    // always the case while the browser is focused.
    this->setAttribute(Qt::WA_MacAlwaysShowToolWindow);
    // Keep the focus in the browser when the overlay shows up
    this->setAttribute(Qt::WA_ShowWithoutActivating);

    // The split is placed in resizeEvent rather than in a layout. A layout
    // would make the split's minimum size the window's, which is wider than
    // the site's chat.
    this->resizeGrip_->setCursor(Qt::SizeHorCursor);
    this->resizeGrip_->installEventFilter(this);

    this->hideTimer_.setSingleShot(true);
    this->hideTimer_.setInterval(250);
    QObject::connect(&this->hideTimer_, &QTimer::timeout, this, &QWidget::hide);
}

void PovOverlayWindow::showAt(const QRect &panel, const ChannelPtr &channel)
{
    this->hideTimer_.stop();
    this->panel_ = panel;

    if (this->split_->getChannel() != channel)
    {
        this->split_->setChannel(channel);
    }
    const auto geometry = this->geometryFor(panel);
    if (!this->drag_ && this->geometry() != geometry)
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
    this->hide();
}

QRect PovOverlayWindow::geometryFor(const QRect &panel) const
{
    const int width = getSettings()->povOverlayWidth;
    if (width <= 0)
    {
        return panel;
    }
    auto geometry = panel;
    geometry.setLeft(panel.x() + panel.width() - width);
    return geometry;
}

void PovOverlayWindow::dragTo(int globalX)
{
    const int right = this->x() + this->width();
    const int maxWidth =
        std::max(MIN_WIDTH, right - this->screen()->availableGeometry().x());
    const int width =
        std::clamp(this->drag_->startWidth + this->drag_->startX - globalX,
                   MIN_WIDTH, maxWidth);
    this->setGeometry(right - width, this->y(), width, this->height());
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
    this->split_->setGeometry(this->rect());
    this->resizeGrip_->setGeometry(0, 0, RESIZE_GRIP_WIDTH, this->height());
    this->resizeGrip_->raise();
    QWidget::resizeEvent(event);
}

bool PovOverlayWindow::eventFilter(QObject *object, QEvent *event)
{
    const auto *mouse = dynamic_cast<QMouseEvent *>(event);
    if (object != this->resizeGrip_ || mouse == nullptr)
    {
        return QWidget::eventFilter(object, event);
    }

    const int globalX = qRound(mouse->globalPosition().x());
    switch (event->type())
    {
        case QEvent::MouseButtonPress:
            if (mouse->button() == Qt::LeftButton)
            {
                this->drag_ = Drag{
                    .startX = globalX,
                    .startWidth = this->width(),
                };
                return true;
            }
            break;

        case QEvent::MouseMove:
            if (this->drag_)
            {
                this->dragTo(globalX);
                return true;
            }
            break;

        case QEvent::MouseButtonRelease:
            if (this->drag_)
            {
                // A click without a drag keeps following the panel's width
                if (this->width() != this->drag_->startWidth)
                {
                    getSettings()->povOverlayWidth = this->width();
                }
                this->drag_.reset();
                return true;
            }
            break;

        case QEvent::MouseButtonDblClick:
            this->drag_.reset();
            getSettings()->povOverlayWidth = 0;
            if (!this->panel_.isEmpty())
            {
                this->setGeometry(this->panel_);
            }
            return true;

        default:
            break;
    }
    return QWidget::eventFilter(object, event);
}

}  // namespace chatterino
