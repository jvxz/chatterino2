// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QString>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

class QJsonArray;
class QRect;

namespace chatterino {
class Channel;
class Paths;
using ChannelPtr = std::shared_ptr<Channel>;
}  // namespace chatterino

/// Keeps a "Multi-POV" tab in sync with the POVs that have chat enabled on
/// lofi-nopixel.com's Multi-POV viewer. Driven by the `multipov` action of the
/// browser extension (see NativeMessaging.cpp).
namespace chatterino::multipov {

enum class Platform : std::uint8_t {
    Twitch,
    Kick,
};

/// A streamer on the Multi-POV viewer.
struct Pov {
    /// The streamer's Twitch login. Kick slugs have '-' where Twitch logins
    /// have '_', so that's the login of a Kick POV.
    QString login;
    /// The streamer's name in kick.com URLs
    QString kickSlug;
    /// The platform the POV was opened on
    Platform platform;
};

/// Parses a lofi-nopixel POV slug ("t-xqc", "k-xqc") or a plain Twitch channel
/// name. Returns nothing for invalid names.
std::optional<Pov> parsePov(const QString &pov);

/// parsePov() for each of `povs`, keeping the original order. A streamer who
/// is open on both platforms only shows up once, as the first one.
std::vector<Pov> parsePovs(const QJsonArray &povs);

/// The platform whose chat is shown for `pov`: the one picked with chooseChat,
/// or else the one the POV was opened on.
Platform chosenPlatform(const Pov &pov);

/// Makes the chat of `pov` come from `platform` from now on.
void chooseChat(const Pov &pov, Platform platform);

/// The chat of `pov` on `platform`: the Twitch channel, or the Kick chat.
ChannelPtr chatOf(const Pov &pov, Platform platform);

/// Tells Chatterino the Kick chatroom id of a streamer, found by the browser
/// extension. Without an id, the streamer isn't on Kick. Must be called from
/// the GUI thread.
void setKickChatroom(const QString &slug, std::optional<int64_t> chatroomId);

/// Makes the Multi-POV tab show exactly the chats of `povs`, one split each.
/// Splits that are already showing a wanted chat are kept as they are.
/// The tab is created on first use. Must be called from the GUI thread.
void syncSplits(const std::vector<Pov> &povs);

/// Docks the overlay window over `rect` (global, logical coordinates), showing
/// the chat of `pov`. Must be called from the GUI thread.
void showOverlay(const Pov &pov, const QRect &rect);

/// Hides the overlay window, unless it's in use. Must be called from the GUI
/// thread.
void hideOverlay();

/// Hides the overlay window if another window now covers the browser window
/// it's over. Must be called from the GUI thread.
void hideOverlayIfCovered();

/// File the overlay's chat background color (#rrggbb) is written to, so the
/// browser extension host can pass it on to the page under the overlay.
QString overlayBackgroundPath(const Paths &paths);

}  // namespace chatterino::multipov
