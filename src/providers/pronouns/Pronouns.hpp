// SPDX-FileCopyrightText: 2024 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "providers/pronouns/alejo/PronounsAlejoApi.hpp"
#include "providers/pronouns/UserPronouns.hpp"

#include <QTimer>

#include <chrono>
#include <functional>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>

namespace chatterino::pronouns {

class Pronouns
{
public:
    Pronouns();

    void getUserPronoun(
        const QString &username,
        const std::function<void(UserPronouns)> &callbackSuccess,
        const std::function<void()> &callbackFail);

    /// Returns the cached pronouns for the user. On a cache miss, this starts
    /// a background fetch (at most one per user at a time) and returns
    /// std::nullopt. Once a fetch finds pronouns, chat views are relayouted so
    /// existing messages pick them up.
    ///
    /// Never blocks on the network, safe to call from any thread.
    std::optional<UserPronouns> getOrFetchUserPronoun(const QString &username);

private:
    using Clock = std::chrono::steady_clock;

    // Retrieve cached pronouns for user.
    std::optional<UserPronouns> getCachedUserPronoun(const QString &username);

    void fetch(const QString &username,
               const std::function<void(std::optional<UserPronouns>)> &onDone);

    // mutex for editing the saved map, inFlight and retryAfter.
    std::shared_mutex mutex;
    // Login name -> Pronouns
    std::unordered_map<QString, UserPronouns> saved;
    // Login names with a running fetch
    std::unordered_set<QString> inFlight;
    // Login name -> earliest time to fetch again after a failure
    std::unordered_map<QString, Clock::time_point> retryAfter;
    AlejoApi alejoApi;

    // Coalesces relayouts when many fetches finish close together
    QTimer relayoutTimer;
};

}  // namespace chatterino::pronouns
