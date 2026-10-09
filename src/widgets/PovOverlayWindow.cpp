// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/PovOverlayWindow.hpp"

#include "common/Channel.hpp"
#include "widgets/splits/Split.hpp"

#ifdef Q_OS_MACOS
#    include "util/MacOsHelpers.h"
#endif

#include <QApplication>
#include <QCursor>
#include <QEvent>
#include <QPointer>

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

#ifdef Q_OS_MACOS
    // Clicking the overlay activates Chatterino, which brings its other
    // windows up in front of the browser
    keepMacOSWindowsBehindOverlay(this->winId());

    // The browser only notices other apps coming to the front while it's
    // focused, not while the overlay is
    onMacOSAppActivated([self = QPointer<PovOverlayWindow>(this)](qint64 pid) {
        if (self && pid != QCoreApplication::applicationPid() &&
            pid != self->browserPid_)
        {
            self->hide();
        }
    });
#endif
}

void PovOverlayWindow::showAt(const QRect &panel, const ChannelPtr &channel)
{
#ifdef Q_OS_MACOS
    // The extension only shows the overlay while the browser is focused
    if (auto pid = getMacOSFrontmostAppPid();
        pid != QCoreApplication::applicationPid())
    {
        this->browserPid_ = pid;
    }
#endif

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

bool PovOverlayWindow::isFocused() const
{
    // Not isActiveWindow(): on macOS that's always true for a Qt::Tool window
    // without a parent, which kept the overlay from ever hiding
    return QApplication::activeWindow() == this;
}

void PovOverlayWindow::requestHide()
{
    if (this->isFocused())
    {
        return;
    }
    this->hide();
}

void PovOverlayWindow::changeEvent(QEvent *event)
{
    // Another Chatterino window was clicked. When another app took the focus,
    // there's no active window, and the app activation handler decides. The
    // cursor check skips the activation that clicking the overlay itself can
    // briefly give to another window.
    if (event->type() == QEvent::ActivationChange && this->isVisible())
    {
        auto *active = QApplication::activeWindow();
        if (active != nullptr && active != this &&
            !this->geometry().contains(QCursor::pos()))
        {
            this->hide();
        }
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
