// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QStringList>

#include <optional>

class QJsonArray;
class QRect;

/// Keeps a "Multi-POV" tab in sync with the POVs that have chat enabled on
/// lofi-nopixel.com's Multi-POV viewer. Driven by the `multipov` action of the
/// browser extension (see NativeMessaging.cpp).
namespace chatterino::multipov {

/// Converts lofi-nopixel POV slugs ("t-xqc", "k-xqc") or plain channel names
/// into a deduplicated, lowercase list of Twitch channel names, keeping the
/// original order. Kick POVs map to the Twitch channel with the same username.
/// Invalid names are skipped.
QStringList twitchChannelsFromPovs(const QJsonArray &povs);

/// Single-POV version of twitchChannelsFromPovs.
std::optional<QString> twitchChannelFromPov(const QString &pov);

/// Makes the Multi-POV tab show exactly `channels`, one split each.
/// Splits that are already showing a wanted channel are kept as they are.
/// The tab is created on first use. Must be called from the GUI thread.
void syncSplits(const QStringList &channels);

/// Docks the overlay window over `rect` (global, logical coordinates), showing
/// the Twitch chat for `channel`. Must be called from the GUI thread.
void showOverlay(const QString &channel, const QRect &rect);

/// Hides the overlay window, unless it's in use. Must be called from the GUI
/// thread.
void hideOverlay();

}  // namespace chatterino::multipov
