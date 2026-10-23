// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/helper/PlatformToggle.hpp"

#include "Application.hpp"
#include "singletons/Fonts.hpp"
#include "singletons/Theme.hpp"
#include "widgets/helper/NotebookTab.hpp"

#include <QApplication>
#include <QMouseEvent>
#include <QPainter>

#include <cmath>

namespace {

// Same as the space around a tab's title in the notebook, without a close
// button
constexpr int TAB_PADDING = 8;
// Same as the line under the notebook's tabs
constexpr int DIVIDER_HEIGHT = 2;

}  // namespace

namespace chatterino {

const std::array<PlatformToggle::Tab, 2> PlatformToggle::TABS{{
    {.platform = multipov::Platform::Twitch, .title = QStringLiteral("Twitch")},
    {.platform = multipov::Platform::Kick, .title = QStringLiteral("Kick")},
}};

PlatformToggle::PlatformToggle(QWidget *parent)
    : BaseWidget(parent)
{
    this->setMouseTracking(true);
}

void PlatformToggle::setPlatform(multipov::Platform platform)
{
    if (this->platform_ != platform)
    {
        this->platform_ = platform;
        this->update();
    }
}

int PlatformToggle::preferredHeight() const
{
    return static_cast<int>((NOTEBOOK_TAB_HEIGHT + DIVIDER_HEIGHT) *
                            this->scale());
}

std::array<QRect, 2> PlatformToggle::tabRects() const
{
    const auto scale = this->scale();
    const auto metrics =
        getApp()->getFonts()->getFontMetrics(FontStyle::UiTabs, scale);
    const auto height = static_cast<int>(NOTEBOOK_TAB_HEIGHT * scale);

    std::array<QRect, 2> rects;
    int x = 0;
    for (size_t i = 0; i < TABS.size(); i++)
    {
        const auto width = metrics.horizontalAdvance(TABS[i].title) +
                           static_cast<int>(2 * TAB_PADDING * scale);
        rects[i] = QRect(x, 0, width, height);
        x += width;
    }
    return rects;
}

int PlatformToggle::tabAt(QPoint pos) const
{
    const auto rects = this->tabRects();
    for (size_t i = 0; i < rects.size(); i++)
    {
        if (rects[i].contains(pos))
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void PlatformToggle::paintEvent(QPaintEvent * /*event*/)
{
    // Painted like NotebookTab and Notebook do with their tabs at the top
    QPainter painter(this);
    const auto scale = this->scale();
    const auto &tabs = this->theme->tabs;

    painter.fillRect(this->rect(), this->theme->window.background);

    const auto windowFocused = this->window() == QApplication::activeWindow();
    painter.setFont(getApp()->getFonts()->getFont(FontStyle::UiTabs, scale));

    const auto rects = this->tabRects();
    for (size_t i = 0; i < TABS.size(); i++)
    {
        const auto selected = TABS[i].platform == this->platform_;
        const auto &colors = selected ? tabs.selected : tabs.regular;

        // Unselected tabs sit a pixel lower
        auto background = rects[i];
        background.setTop(
            static_cast<int>(std::ceil((selected ? 0 : 1) * scale)));
        painter.fillRect(background, windowFocused
                                         ? colors.backgrounds.regular
                                         : colors.backgrounds.unfocused);

        const auto lineThickness =
            static_cast<int>(std::ceil((selected ? 2 : 1) * scale));
        const auto lineColor =
            this->hovered_ == static_cast<int>(i)
                ? colors.line.hover
                : (windowFocused ? colors.line.regular : colors.line.unfocused);
        painter.fillRect(background.left(), background.top(),
                         background.width(), lineThickness, lineColor);

        painter.setPen(colors.text);
        painter.drawText(background, Qt::AlignCenter, TABS[i].title);
    }

    // The line under the tabs, where the chat starts
    const auto dividerHeight = static_cast<int>(DIVIDER_HEIGHT * scale);
    painter.fillRect(0, this->height() - dividerHeight, this->width(),
                     dividerHeight, tabs.dividerLine);
}

void PlatformToggle::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
    {
        return;
    }

    const auto index = this->tabAt(event->position().toPoint());
    if (index < 0)
    {
        return;
    }
    const auto platform = TABS[static_cast<size_t>(index)].platform;
    if (platform != this->platform_)
    {
        this->platformPicked.invoke(platform);
    }
}

void PlatformToggle::mouseMoveEvent(QMouseEvent *event)
{
    const auto hovered = this->tabAt(event->position().toPoint());
    if (hovered != this->hovered_)
    {
        this->hovered_ = hovered;
        this->update();
    }
}

void PlatformToggle::leaveEvent(QEvent * /*event*/)
{
    if (this->hovered_ != -1)
    {
        this->hovered_ = -1;
        this->update();
    }
}

}  // namespace chatterino
