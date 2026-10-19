// SPDX-FileCopyrightText: 2024 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "providers/pronouns/Pronouns.hpp"

#include "Application.hpp"
#include "common/QLogging.hpp"
#include "providers/pronouns/alejo/PronounsAlejoApi.hpp"
#include "providers/pronouns/UserPronouns.hpp"
#include "singletons/WindowManager.hpp"

#include <mutex>
#include <unordered_map>

namespace {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
const auto &LOG = chatterinoPronouns;

/// How long to wait before fetching a user's pronouns again after a failure
constexpr std::chrono::seconds RETRY_DELAY{60};

/// How long to collect finished fetches before relayouting chat
constexpr int RELAYOUT_DELAY_MS = 250;

}  // namespace

namespace chatterino::pronouns {

Pronouns::Pronouns()
{
    this->relayoutTimer.setSingleShot(true);
    this->relayoutTimer.setInterval(RELAYOUT_DELAY_MS);
    QObject::connect(&this->relayoutTimer, &QTimer::timeout,
                     &this->relayoutTimer, [] {
                         getApp()->getWindows()->forceLayoutChannelViews();
                     });
}

void Pronouns::getUserPronoun(
    const QString &username,
    const std::function<void(UserPronouns)> &callbackSuccess,
    const std::function<void()> &callbackFail)
{
    // Only fetch pronouns if we haven't fetched before.
    auto cachedPronoun = this->getCachedUserPronoun(username);
    if (cachedPronoun.has_value())
    {
        callbackSuccess(*cachedPronoun);
        return;
    }

    this->fetch(username,
                [callbackSuccess, callbackFail](const auto &oUserPronoun) {
                    if (!oUserPronoun.has_value())
                    {
                        callbackFail();
                        return;
                    }
                    callbackSuccess(*oUserPronoun);
                });
}

std::optional<UserPronouns> Pronouns::getOrFetchUserPronoun(
    const QString &username)
{
    auto cachedPronoun = this->getCachedUserPronoun(username);
    if (cachedPronoun.has_value())
    {
        return cachedPronoun;
    }

    {
        std::unique_lock lock(this->mutex);

        auto retryIt = this->retryAfter.find(username);
        if (retryIt != this->retryAfter.end() && Clock::now() < retryIt->second)
        {
            return {};
        }

        if (!this->inFlight.insert(username).second)
        {
            // Already fetching
            return {};
        }
    }

    this->fetch(username, nullptr);
    return {};
}

void Pronouns::fetch(
    const QString &username,
    const std::function<void(std::optional<UserPronouns>)> &onDone)
{
    this->alejoApi.fetch(
        username, [this, username, onDone](const auto &oUserPronoun) {
            bool foundPronouns = false;
            {
                std::unique_lock lock(this->mutex);
                this->inFlight.erase(username);

                if (oUserPronoun.has_value())
                {
                    qCDebug(LOG) << "Caching pronoun" << oUserPronoun->format()
                                 << "for user" << username;
                    this->saved[username] = *oUserPronoun;
                    this->retryAfter.erase(username);
                    foundPronouns = !oUserPronoun->isUnspecified();
                }
                else
                {
                    this->retryAfter[username] = Clock::now() + RETRY_DELAY;
                }
            }

            if (foundPronouns)
            {
                // Queued so this works from any thread. If the timer is gone
                // (shutdown), Qt drops the call.
                QMetaObject::invokeMethod(
                    &this->relayoutTimer,
                    [timer = &this->relayoutTimer] {
                        if (!timer->isActive())
                        {
                            timer->start();
                        }
                    },
                    Qt::QueuedConnection);
            }

            if (onDone)
            {
                onDone(oUserPronoun);
            }
        });
}

std::optional<UserPronouns> Pronouns::getCachedUserPronoun(
    const QString &username)
{
    std::shared_lock lock(this->mutex);
    auto it = this->saved.find(username);
    if (it != this->saved.end())
    {
        return {it->second};
    }
    return {};
}

}  // namespace chatterino::pronouns
