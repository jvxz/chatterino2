// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "singletons/MultiPovSync.hpp"

#include "Test.hpp"

#include <QJsonArray>

using namespace chatterino;

TEST(MultiPovSync, twitchChannelsFromPovs)
{
    struct TestCase {
        QJsonArray input;
        QStringList expected;
    };

    std::vector<TestCase> tests{
        {{}, {}},
        // Twitch and Kick slugs both map to the Twitch name
        {{"t-xqc", "k-buddha"}, {"xqc", "buddha"}},
        // plain channel names are accepted as-is
        {{"anthonyz"}, {"anthonyz"}},
        // same streamer on both platforms only shows once, first wins
        {{"k-xqc", "t-xqc", "t-pengwin"}, {"xqc", "pengwin"}},
        // case and whitespace are normalized
        {{" T-XQC ", "k-Buddha"}, {"xqc", "buddha"}},
        // names that can't be Twitch logins are skipped
        {{"k-", "t-a-b", "k-way_too_long_for_twitch_login", "k-zaitohro"},
         {"zaitohro"}},
        // non-strings are skipped
        {{1, QJsonValue::Null, "t-omie"}, {"omie"}},
    };

    for (const auto &[input, expected] : tests)
    {
        ASSERT_EQ(multipov::twitchChannelsFromPovs(input), expected);
    }
}
