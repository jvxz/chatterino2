// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "singletons/MultiPovSync.hpp"

#include "Test.hpp"

#include <QJsonArray>

using namespace chatterino;

TEST(MultiPovSync, parsePovs)
{
    struct TestCase {
        QJsonArray input;
        QStringList expected;
    };

    std::vector<TestCase> tests{
        {{}, {}},
        // Twitch and Kick slugs both give the streamer's Twitch login
        {{"t-xqc", "k-buddha"}, {"xqc", "buddha"}},
        // plain channel names are accepted as-is
        {{"anthonyz"}, {"anthonyz"}},
        // same streamer on both platforms only shows once, first wins
        {{"k-xqc", "t-xqc", "t-pengwin"}, {"xqc", "pengwin"}},
        // case and whitespace are normalized
        {{" T-XQC ", "k-Buddha"}, {"xqc", "buddha"}},
        // Kick writes '_' as '-'
        {{"k-hello-kiko", "t-hello_kiko"}, {"hello_kiko"}},
        // names that can't be Twitch logins or Kick slugs are skipped
        {{"k-", "t-a-b", "k-way_too_long_for_a_kick_channel", "k-zaitohro"},
         {"zaitohro"}},
        // non-strings are skipped
        {{1, QJsonValue::Null, "t-omie"}, {"omie"}},
    };

    for (const auto &[input, expected] : tests)
    {
        QStringList logins;
        for (const auto &pov : multipov::parsePovs(input))
        {
            logins.append(pov.login);
        }
        ASSERT_EQ(logins, expected);
    }
}

TEST(MultiPovSync, parsePov)
{
    const auto kick = multipov::parsePov("k-Hello-Kiko");
    ASSERT_TRUE(kick.has_value());
    ASSERT_EQ(kick->login, QString("hello_kiko"));
    ASSERT_EQ(kick->kickSlug, QString("hello-kiko"));
    ASSERT_EQ(kick->platform, multipov::Platform::Kick);

    const auto twitch = multipov::parsePov("t-hello_kiko");
    ASSERT_TRUE(twitch.has_value());
    ASSERT_EQ(twitch->login, QString("hello_kiko"));
    ASSERT_EQ(twitch->kickSlug, QString("hello-kiko"));
    ASSERT_EQ(twitch->platform, multipov::Platform::Twitch);

    const auto plain = multipov::parsePov("xqc");
    ASSERT_TRUE(plain.has_value());
    ASSERT_EQ(plain->platform, multipov::Platform::Twitch);

    ASSERT_FALSE(multipov::parsePov("").has_value());
    // '-' is not allowed in Twitch logins
    ASSERT_FALSE(multipov::parsePov("t-not-twitch").has_value());
    ASSERT_FALSE(multipov::parsePov("xqc!").has_value());
}
