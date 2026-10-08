// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "singletons/MultiPovSync.hpp"

#include "Application.hpp"
#include "common/Channel.hpp"
#include "debug/AssertInGuiThread.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"
#include "singletons/Settings.hpp"
#include "singletons/WindowManager.hpp"
#include "widgets/helper/NotebookTab.hpp"
#include "widgets/Notebook.hpp"
#include "widgets/splits/Split.hpp"
#include "widgets/splits/SplitContainer.hpp"
#include "widgets/Window.hpp"

#include <QJsonArray>

namespace {

using namespace chatterino;

const QString TAB_TITLE = QStringLiteral("Multi-POV");

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

QStringList twitchChannelsFromPovs(const QJsonArray &povs)
{
    QStringList channels;
    channels.reserve(povs.size());

    for (const auto value : povs)
    {
        auto name = value.toString().trimmed().toLower();
        if (name.startsWith(u"t-") || name.startsWith(u"k-"))
        {
            name.remove(0, 2);
        }

        if (isValidTwitchLogin(name) && !channels.contains(name))
        {
            channels.append(name);
        }
    }

    return channels;
}

void syncSplits(const QStringList &channels)
{
    assertInGuiThread();

    auto *container = findContainer();
    if (container == nullptr)
    {
        if (channels.isEmpty())
        {
            // Don't open an empty tab just because every chat is off
            return;
        }
        container = createContainer();
    }

    // Diff instead of rebuilding, so splits that stay keep their scroll
    // position and don't have to be laid out again.
    QStringList missing = channels;
    bool changed = false;
    for (auto *split : container->getSplits())
    {
        if (missing.removeOne(split->getChannel()->getName().toLower()))
        {
            continue;
        }
        container->deleteSplit(split);
        changed = true;
    }

    for (const auto &name : missing)
    {
        auto *split = container->appendNewSplit(false);
        split->setChannel(getApp()->getTwitch()->getOrAddChannel(name));
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

}  // namespace chatterino::multipov
