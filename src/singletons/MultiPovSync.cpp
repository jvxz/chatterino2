// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "singletons/MultiPovSync.hpp"

#include "Application.hpp"
#include "common/Channel.hpp"
#include "debug/AssertInGuiThread.hpp"
#include "providers/kick/KickChannel.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"
#include "singletons/Paths.hpp"
#include "singletons/Settings.hpp"
#include "singletons/Theme.hpp"
#include "singletons/WindowManager.hpp"
#include "widgets/helper/NotebookTab.hpp"
#include "widgets/Notebook.hpp"
#include "widgets/PovOverlayWindow.hpp"
#include "widgets/splits/Split.hpp"
#include "widgets/splits/SplitContainer.hpp"
#include "widgets/Window.hpp"

#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QPointer>

#include <algorithm>

namespace {

using namespace chatterino;

const QString TAB_TITLE = QStringLiteral("Multi-POV");

QPointer<PovOverlayWindow> &overlay()
{
    static QPointer<PovOverlayWindow> window;
    return window;
}

/// Twitch logins are 1-25 characters of [a-z0-9_]. Notably they can't contain
/// '-', which is what makes the "t-"/"k-" prefixes unambiguous.
bool isValidTwitchLogin(QStringView login)
{
    if (login.isEmpty() || login.size() > 25)
    {
        return false;
    }
    for (auto c : login)
    {
        if (!((c >= u'a' && c <= u'z') || (c >= u'0' && c <= u'9') ||
              c == u'_'))
        {
            return false;
        }
    }
    return true;
}

/// Kick channel names are what is in their kick.com URL: letters, digits, '_'
/// and '-'. Underscores are written as '-' there.
bool isValidKickSlug(QStringView slug)
{
    if (slug.isEmpty() || slug.size() > 25)
    {
        return false;
    }
    for (auto c : slug)
    {
        if (!((c >= u'a' && c <= u'z') || (c >= u'0' && c <= u'9') ||
              c == u'_' || c == u'-'))
        {
            return false;
        }
    }
    return true;
}

/// The platforms picked for streamers with chatOf(), by Twitch login
QHash<QString, multipov::Platform> &chosenPlatforms()
{
    static QHash<QString, multipov::Platform> chosen;
    return chosen;
}

/// Finds the Multi-POV tab by its title, so the one restored from the window
/// layout on startup is reused instead of opening a second one.
SplitContainer *findContainer()
{
    for (auto *window : getApp()->getWindows()->windows())
    {
        auto &notebook = window->getNotebook();
        for (int i = 0; i < notebook.getPageCount(); ++i)
        {
            auto *container =
                dynamic_cast<SplitContainer *>(notebook.getPageAt(i));
            if (container != nullptr && container->getTab() != nullptr &&
                container->getTab()->getCustomTitle() == TAB_TITLE)
            {
                return container;
            }
        }
    }
    return nullptr;
}

SplitContainer *createContainer()
{
    auto *windows = getApp()->getWindows();

    SplitContainer *container = nullptr;
    if (getSettings()->multiPovSyncInMainWindow)
    {
        container = windows->getMainWindow().getNotebook().addPage(false);
    }
    else
    {
        auto &popup = windows->createWindow(WindowType::Popup, {
                                                                   .show = true,
                                                               });
        container = popup.getNotebook().getOrAddSelectedPage();
    }

    container->getTab()->setCustomTitle(TAB_TITLE);
    return container;
}

}  // namespace

namespace chatterino::multipov {

std::optional<Pov> parsePov(const QString &pov)
{
    auto name = pov.trimmed().toLower();

    auto platform = Platform::Twitch;
    if (name.startsWith(u"t-"))
    {
        name.remove(0, 2);
    }
    else if (name.startsWith(u"k-"))
    {
        name.remove(0, 2);
        platform = Platform::Kick;
    }

    if (platform == Platform::Kick)
    {
        if (!isValidKickSlug(name))
        {
            return std::nullopt;
        }
        auto login = name;
        login.replace(u'-', u'_');
        return Pov{.login = login, .kickSlug = name, .platform = platform};
    }

    if (!isValidTwitchLogin(name))
    {
        return std::nullopt;
    }
    auto slug = name;
    slug.replace(u'_', u'-');
    return Pov{.login = name, .kickSlug = slug, .platform = platform};
}

std::vector<Pov> parsePovs(const QJsonArray &povs)
{
    std::vector<Pov> parsed;
    parsed.reserve(povs.size());

    for (const auto value : povs)
    {
        auto pov = parsePov(value.toString());
        if (!pov)
        {
            continue;
        }
        if (std::ranges::none_of(parsed, [&](const Pov &other) {
                return other.login == pov->login;
            }))
        {
            parsed.push_back(std::move(*pov));
        }
    }

    return parsed;
}

Platform chosenPlatform(const Pov &pov)
{
    return chosenPlatforms().value(pov.login, pov.platform);
}

void chooseChat(const Pov &pov, Platform platform)
{
    chosenPlatforms().insert(pov.login, platform);
}

ChannelPtr chatOf(const Pov &pov, Platform platform)
{
    if (platform == Platform::Kick)
    {
        return KickChannel::getOrAdd(pov.kickSlug);
    }
    return getApp()->getTwitch()->getOrAddChannel(pov.login);
}

void setKickChatroom(const QString &slug, const KickChatroom &chatroom)
{
    KickChannel::setChatroom(slug, chatroom);
}

void syncSplits(const std::vector<Pov> &povs)
{
    assertInGuiThread();

    auto *container = findContainer();
    if (container == nullptr)
    {
        if (povs.empty())
        {
            // Don't open an empty tab just because every chat is off
            return;
        }
        container = createContainer();
    }

    // Diff instead of rebuilding, so splits that stay keep their scroll
    // position and don't have to be laid out again.
    std::vector<ChannelPtr> missing;
    missing.reserve(povs.size());
    for (const auto &pov : povs)
    {
        missing.push_back(chatOf(pov, chosenPlatform(pov)));
    }

    bool changed = false;
    for (auto *split : container->getSplits())
    {
        if (auto it = std::ranges::find(missing, split->getChannel());
            it != missing.end())
        {
            missing.erase(it);
            continue;
        }
        container->deleteSplit(split);
        changed = true;
    }

    for (const auto &channel : missing)
    {
        auto *split = container->appendNewSplit(false);
        split->setChannel(channel);
        changed = true;
    }

    if (changed && getSettings()->multiPovSyncRaiseWindow)
    {
        // Raise without activating, so focus stays in the browser
        auto *window = container->window();
        window->show();
        window->raise();
    }
}

void showOverlay(const Pov &pov, const QRect &rect)
{
    assertInGuiThread();

    // Only written when the theme changed since the last time
    static QString writtenBackground;
    const auto background =
        getApp()->getThemes()->messages.backgrounds.regular.name();
    if (background != writtenBackground)
    {
        QFile file(overlayBackgroundPath(getApp()->getPaths()));
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            file.write(background.toLatin1());
            writtenBackground = background;
        }
    }

    auto &window = overlay();
    if (window.isNull())
    {
        window = new PovOverlayWindow();
    }
    window->showAt(rect, pov);
}

void hideOverlay()
{
    assertInGuiThread();

    if (auto &window = overlay(); !window.isNull())
    {
        window->requestHide();
    }
}

void hideOverlayIfCovered()
{
    assertInGuiThread();

    if (auto &window = overlay(); !window.isNull())
    {
        window->hideIfCovered();
    }
}

QString overlayBackgroundPath(const Paths &paths)
{
    return paths.miscDirectory + QStringLiteral("/lofi-overlay-background");
}

}  // namespace chatterino::multipov
