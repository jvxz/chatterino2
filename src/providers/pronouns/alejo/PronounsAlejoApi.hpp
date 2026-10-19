// SPDX-FileCopyrightText: 2024 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "providers/pronouns/UserPronouns.hpp"

#include <QJsonObject>
#include <QString>

#include <functional>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace chatterino::pronouns {

class AlejoApi
{
public:
    AlejoApi();

    /// Fetch the user's pronouns from the alejo.io API
    ///
    /// onDone can be invoked from any thread
    ///
    /// The argument is std::nullopt if and only if the request failed.
    ///
    /// If the pronoun list hasn't loaded yet, the fetch is queued until it has.
    void fetch(const QString &username,
               const std::function<void(std::optional<UserPronouns>)> &onDone);

private:
    using Callback = std::function<void(std::optional<UserPronouns>)>;

    /// Callers must set loadingPronouns first
    void loadAvailablePronouns();
    void fetchUser(const QString &username, const Callback &onDone);

    std::shared_mutex mutex;

    /// True while the pronoun list request is running
    bool loadingPronouns;
    /// Fetches waiting for the pronoun list to load
    std::vector<std::pair<QString, Callback>> pendingFetches;
    struct PronounEntry {
        QString subject;
        QString object;
        bool singular;
    };

    /// Maps alejo.io pronoun IDs to their human readable representation (subject, object, and singularity)
    std::unordered_map<QString, PronounEntry> pronouns;

    /// Parse a pronoun definition from the /users endpoint into a finished UserPronouns
    UserPronouns parsePronoun(const QJsonObject &object);
};

}  // namespace chatterino::pronouns
