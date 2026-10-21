// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "common/Channel.hpp"
#include "common/websockets/WebSocketPool.hpp"
#include "util/ExponentialBackoff.hpp"

#include <QString>

#include <cstdint>
#include <memory>
#include <optional>

class QJsonObject;

namespace chatterino {

/// A read-only Kick chat. Kick has no public chat API that doesn't need an app
/// and a login, so this listens to the same Pusher channel the kick.com chat
/// does (chatrooms.<id>.v2). The chatroom id isn't known to Chatterino: the
/// browser extension looks it up and passes it on, see setChatroom().
///
/// Everything in here runs in the GUI thread, except the socket listener,
/// which only hands events over to it.
class KickChannel : public Channel
{
public:
    explicit KickChannel(const QString &slug);
    ~KickChannel() override;

    /// The Kick chat of the channel with this slug (the name in its kick.com
    /// URL, lowercase). Chats that nothing shows anymore are dropped, which
    /// closes their connection.
    static std::shared_ptr<KickChannel> getOrAdd(const QString &slug);

    /// Tells the chat of `slug` its chatroom id, which starts the connection.
    /// Without an id, the channel couldn't be found. Must be called from the
    /// GUI thread.
    static void setChatroom(const QString &slug,
                            std::optional<int64_t> chatroomId);

    /// Channel name of the chat with this slug, to tell it apart from the
    /// Twitch channel with the same name.
    static QString channelName(const QString &slug);

    const QString &getDisplayName() const override;
    const QString &getLocalizedName() const override;
    bool canReconnect() const override;
    void reconnect() override;
    void sendMessage(const QString &message) override;

    /// An event came in over the socket. `generation` tells which connection
    /// it came from, so events of a replaced one are ignored.
    /// Called from the GUI thread by the socket listener.
    void onSocketEvent(uint64_t generation, const QString &event,
                       const QString &data);
    void onSocketClosed(uint64_t generation);

private:
    void setChatroomId(std::optional<int64_t> chatroomId);
    void connect();
    void send(const QByteArray &json);
    void onChatMessage(const QJsonObject &data);

    const QString slug_;
    const QString displayName_;
    std::optional<int64_t> chatroomId_;

    WebSocketHandle socket_;
    uint64_t generation_ = 0;
    bool subscribed_ = false;
    ExponentialBackoff<6> backoff_{std::chrono::milliseconds(1000)};
};

}  // namespace chatterino
