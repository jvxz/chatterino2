// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/helper/PlatformToggle.hpp"

#include "Application.hpp"
#include "singletons/Fonts.hpp"
#include "singletons/Theme.hpp"

#include <QEvent>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>

namespace {

constexpr int SIDE_MARGIN = 22;  // clears the scrollbar
constexpr int TOP_MARGIN = 6;
constexpr int PADDING_X = 9;
constexpr int PADDING_Y = 4;

}  // namespace

namespace chatterino {

PlatformToggle::PlatformToggle(QWidget *parent)
    : BaseWidget(parent)
{
    this->setCursor(Qt::PointingHandCursor);
    this->setToolTip(QStringLiteral("Switch between the Twitch and Kick chat"));
    parent->installEventFilter(this);
    this->updateGeometry_();
}

void PlatformToggle::setPlatform(multipov::Platform platform)
{
    if (this->platform_ != platform)
    {
        this->platform_ = platform;
        this->update();
    }
}

QFont PlatformToggle::font_() const
{
    return getApp()->getFonts()->getFont(FontStyle::UiMediumBold,
                                         this->scale());
}

void PlatformToggle::updateGeometry_()
{
    const QFontMetrics metrics(this->font_());
    const auto scale = this->scale();
    // Both halves are as wide as the longer label
    const auto half = metrics.horizontalAdvance(QStringLiteral("Twitch")) +
                      static_cast<int>(2 * PADDING_X * scale);
    const auto height =
        metrics.height() + static_cast<int>(2 * PADDING_Y * scale);
    this->resize(2 * half, height);

    if (auto *parent = this->parentWidget())
    {
        this->move(parent->width() - this->width() -
                       static_cast<int>(SIDE_MARGIN * scale),
                   static_cast<int>(TOP_MARGIN * scale));
        this->raise();
    }
}

void PlatformToggle::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const auto outer = QRectF(this->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const auto radius = outer.height() / 2;

    auto background = this->theme->splits.header.background;
    background.setAlpha(225);
    painter.setPen(this->theme->splits.header.border);
    painter.setBrush(background);
    painter.drawRoundedRect(outer, radius, radius);

    const auto halfWidth = outer.width() / 2;
    const QRectF left(outer.left(), outer.top(), halfWidth, outer.height());
    const QRectF right(outer.left() + halfWidth, outer.top(), halfWidth,
                       outer.height());

    const auto selected =
        this->platform_ == multipov::Platform::Twitch ? left : right;
    painter.setPen(Qt::NoPen);
    painter.setBrush(this->theme->accent);
    painter.drawRoundedRect(selected.adjusted(2, 2, -2, -2), radius - 2,
                            radius - 2);

    painter.setFont(this->font_());
    painter.setPen(this->theme->splits.header.text);
    painter.drawText(left, Qt::AlignCenter, QStringLiteral("Twitch"));
    painter.drawText(right, Qt::AlignCenter, QStringLiteral("Kick"));
}

void PlatformToggle::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
    {
        return;
    }

    const auto picked = event->position().x() < this->width() / 2.0
                            ? multipov::Platform::Twitch
                            : multipov::Platform::Kick;
    if (picked != this->platform_)
    {
        this->platformPicked.invoke(picked);
    }
}

bool PlatformToggle::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == this->parentWidget() && event->type() == QEvent::Resize)
    {
        this->updateGeometry_();
    }
    return BaseWidget::eventFilter(watched, event);
}

void PlatformToggle::scaleChangedEvent(float /*scale*/)
{
    this->updateGeometry_();
}

}  // namespace chatterino
