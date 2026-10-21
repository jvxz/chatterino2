// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "providers/kick/KickChannel.hpp"

#include "common/LinkParser.hpp"
#include "common/QLogging.hpp"
#include "debug/AssertInGuiThread.hpp"
#include "messages/Emote.hpp"
#include "messages/Image.hpp"
#include "messages/Message.hpp"
#include "messages/MessageBuilder.hpp"
#include "messages/MessageElement.hpp"
#include "util/PostToThread.hpp"

#include <QDateTime>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>

#include <iterator>

namespace {

using namespace chatterino;
using namespace Qt::StringLiterals;

// The Pusher app kick.com's own chat connects with
const QUrl SOCKET_URL(u"wss://ws-us2.pusher.com/app/eb1d5f283081a78b932c"
                      u"?protocol=7&client=js&version=8.4.0&flash=false"_s);

// Chats nothing shows anymore are dropped once there are more than this
constexpr size_t MAX_UNUSED_CHATS = 8;

// Kick emotes are sent in the text as [emote:<id>:<name>]
const QRegularExpression &emoteRegex()
{
    static const QRegularExpression regex(R"(\[emote:(\d+):([^\]]+)\])");
    return regex;
}

// Deliberately never destroyed: the chats and the socket thread would be torn
// down during static destruction, after the application is gone.
WebSocketPool &socketPool()
{
    static auto *pool = new WebSocketPool(u"kick"_s);  // NOLINT
    return *pool;
}

struct Registry {
    QHash<QString, std::shared_ptr<KickChannel>> channels;
    // Chatroom ids the extension sent before the chat was opened. A missing
    // value means the channel doesn't exist.
    QHash<QString, std::optional<int64_t>> chatrooms;
};

Registry &registry()
{
    static auto *registry = new Registry;  // NOLINT
    return *registry;
}

class Listener : public WebSocketListener
{
public:
    Listener(std::weak_ptr<KickChannel> channel, uint64_t generation)
        : channel_(std::move(channel))
        , generation_(generation)
    {
    }

    void onOpen() override
    {
        // The server greets first, see KickChannel::onSocketEvent
    }

    void onTextMessage(QByteArray data) override
    {
        const auto root = QJsonDocument::fromJson(data).object();
        const auto event = root["event"_L1].toString();
        if (event.isEmpty())
        {
            return;
        }

        // Pusher puts the payload in a string
        const auto payload = root["data"_L1];
        const auto text =
            payload.isString()
                ? payload.toString()
                : QString::fromUtf8(QJsonDocument(payload.toObject())
                                        .toJson(QJsonDocument::Compact));

        postToThread([channel = this->channel_, generation = this->generation_,
                      event, text] {
            if (auto strong = channel.lock())
            {
                strong->onSocketEvent(generation, event, text);
            }
        });
    }

    void onBinaryMessage(QByteArray /*data*/) override
    {
    }

    void onClose(std::unique_ptr<WebSocketListener> /*self*/) override
    {
        postToThread(
            [channel = this->channel_, generation = this->generation_] {
                if (auto strong = channel.lock())
                {
                    strong->onSocketClosed(generation);
                }
            });
    }

private:
    std::weak_ptr<KickChannel> channel_;
    uint64_t generation_;
};

void appendText(MessageBuilder &builder, const QString &text)
{
    for (const auto &word : text.split(u' ', Qt::SkipEmptyParts))
    {
        if (auto link = linkparser::parse(word))
        {
            builder.addLink(*link, word);
            continue;
        }
        builder.appendOrEmplaceText(word, MessageColor::Text);
    }
}

EmotePtr makeEmote(const QString &id, const QString &name)
{
    // Kick only has one size, which is 4x the size emotes are shown at
    const Url url{u"https://files.kick.com/emotes/%1/fullsize"_s.arg(id)};
    return std::make_shared<Emote>(Emote{
        .name = {name},
        .images = ImageSet{Image::fromUrl(url, 0.25, QSize(112, 112))},
        .tooltip = {name + u"<br>Kick Emote"_s},
        .homePage = {},
        .id = {id},
    });
}

}  // namespace

namespace chatterino {

KickChannel::KickChannel(const QString &slug)
    : Channel(channelName(slug), Channel::Type::None)
    , slug_(slug)
    , displayName_(slug + u" (Kick)"_s)
{
    this->platform_ = u"kick"_s;
}

KickChannel::~KickChannel() = default;

QString KickChannel::channelName(const QString &slug)
{
    return u"kick:"_s + slug;
}

std::shared_ptr<KickChannel> KickChannel::getOrAdd(const QString &slug)
{
    assertInGuiThread();

    auto &reg = registry();
    if (auto it = reg.channels.find(slug); it != reg.channels.end())
    {
        return *it;
    }

    // Drop chats nothing uses anymore, which closes their connections
    if (static_cast<size_t>(reg.channels.size()) >= MAX_UNUSED_CHATS)
    {
        for (auto it = reg.channels.begin(); it != reg.channels.end();)
        {
            it = it->use_count() == 1 ? reg.channels.erase(it) : std::next(it);
        }
    }

    auto channel = std::make_shared<KickChannel>(slug);
    reg.channels.insert(slug, channel);
    if (auto known = reg.chatrooms.find(slug); known != reg.chatrooms.end())
    {
        channel->setChatroomId(*known);
    }
    else
    {
        channel->addSystemMessage(
            u"Looking up the Kick chat of %1..."_s.arg(channel->slug_));
    }
    return channel;
}

void KickChannel::setChatroom(const QString &slug,
                              std::optional<int64_t> chatroomId)
{
    assertInGuiThread();

    auto &reg = registry();
    reg.chatrooms.insert(slug, chatroomId);
    if (auto it = reg.channels.find(slug); it != reg.channels.end())
    {
        (*it)->setChatroomId(chatroomId);
    }
}

const QString &KickChannel::getDisplayName() const
{
    return this->displayName_;
}

const QString &KickChannel::getLocalizedName() const
{
    return this->displayName_;
}

bool KickChannel::canReconnect() const
{
    return true;
}

void KickChannel::reconnect()
{
    this->connect();
}

void KickChannel::sendMessage(const QString & /*message*/)
{
    this->addSystemMessage(u"Kick chat is read-only."_s);
}

void KickChannel::setChatroomId(std::optional<int64_t> chatroomId)
{
    if (!chatroomId)
    {
        this->chatroomId_.reset();
        this->socket_ = {};
        this->generation_++;
        this->addSystemMessage(
            u"Couldn't find the Kick chat of %1."_s.arg(this->slug_));
        return;
    }

    if (this->chatroomId_ == chatroomId)
    {
        // The extension sends it again whenever the overlay moves
        return;
    }
    this->chatroomId_ = chatroomId;
    this->connect();
}

void KickChannel::connect()
{
    if (!this->chatroomId_)
    {
        return;
    }

    this->subscribed_ = false;
    // Replacing the handle closes the old connection, whose events are
    // ignored from now on
    this->socket_ = socketPool().createSocket(
        {.url = SOCKET_URL, .headers = {}},
        std::make_unique<Listener>(
            std::static_pointer_cast<KickChannel>(this->shared_from_this()),
            ++this->generation_));
}

void KickChannel::send(const QByteArray &json)
{
    this->socket_.sendText(json);
}

void KickChannel::onSocketEvent(uint64_t generation, const QString &event,
                                const QString &data)
{
    assertInGuiThread();
    if (generation != this->generation_ || !this->chatroomId_)
    {
        return;
    }

    if (event == u"pusher:connection_established")
    {
        QJsonObject subscription{
            {"event"_L1, "pusher:subscribe"_L1},
            {"data"_L1,
             QJsonObject{
                 {"auth"_L1, ""_L1},
                 {"channel"_L1, u"chatrooms.%1.v2"_s.arg(*this->chatroomId_)},
             }},
        };
        this->send(QJsonDocument(subscription).toJson(QJsonDocument::Compact));
    }
    else if (event == u"pusher:ping")
    {
        this->send(R"({"event":"pusher:pong","data":{}})");
    }
    else if (event == u"pusher_internal:subscription_succeeded")
    {
        this->backoff_.reset();
        this->subscribed_ = true;
        this->addSystemMessage(
            u"Connected to the Kick chat of %1."_s.arg(this->slug_));
    }
    else if (event == u"pusher:error")
    {
        qCWarning(chatterinoNativeMessage) << "Kick chat error:" << data;
    }
    else if (event == u"App\\Events\\ChatMessageEvent")
    {
        this->onChatMessage(QJsonDocument::fromJson(data.toUtf8()).object());
    }
    else if (event == u"App\\Events\\MessageDeletedEvent")
    {
        const auto id = QJsonDocument::fromJson(data.toUtf8())
                            .object()["message"_L1]
                            .toObject()["id"_L1]
                            .toString();
        if (!id.isEmpty())
        {
            this->disableMessage(id);
        }
    }
}

void KickChannel::onSocketClosed(uint64_t generation)
{
    assertInGuiThread();
    if (generation != this->generation_)
    {
        return;
    }

    this->socket_ = {};
    if (this->subscribed_)
    {
        this->subscribed_ = false;
        this->addSystemMessage(u"Disconnected from the Kick chat. "
                               u"Reconnecting..."_s);
    }

    QTimer::singleShot(
        this->backoff_.next(),
        [channel = std::weak_ptr<KickChannel>(
             std::static_pointer_cast<KickChannel>(this->shared_from_this())),
         generation] {
            if (auto strong = channel.lock();
                strong && strong->generation_ == generation)
            {
                strong->connect();
            }
        });
}

void KickChannel::onChatMessage(const QJsonObject &data)
{
    const auto sender = data["sender"_L1].toObject();
    const auto username = sender["username"_L1].toString();
    const auto content = data["content"_L1].toString();
    if (username.isEmpty() || content.isEmpty())
    {
        return;
    }

    const QColor color(sender["identity"_L1].toObject()["color"_L1].toString());
    const MessageColor usernameColor = color.isValid()
                                           ? MessageColor(color)
                                           : MessageColor(MessageColor::Text);

    MessageBuilder builder;
    builder.emplace<TimestampElement>(QTime::currentTime());
    builder.emplace<TextElement>(username + QChar(u':'),
                                 MessageElementFlag::Username, usernameColor,
                                 FontStyle::ChatMediumBold);

    // Replies start with the user they reply to
    const auto original = data["metadata"_L1]
                              .toObject()["original_sender"_L1]
                              .toObject()["username"_L1]
                              .toString();
    if (!original.isEmpty())
    {
        builder.appendOrEmplaceText(QChar(u'@') + original, MessageColor::Text);
    }

    // The text with emotes written out, for searching and copying
    QString plain;
    qsizetype last = 0;
    auto matches = emoteRegex().globalMatch(content);
    while (matches.hasNext())
    {
        const auto match = matches.next();
        const auto before = content.mid(last, match.capturedStart() - last);
        appendText(builder, before);
        plain += before + match.captured(2);

        builder.emplace<EmoteElement>(
            makeEmote(match.captured(1), match.captured(2)),
            MessageElementFlag::Emote);
        last = match.capturedEnd();
    }
    const auto rest = content.mid(last);
    appendText(builder, rest);
    plain += rest;

    auto &message = builder.message();
    message.channelName = this->getName();
    message.id = data["id"_L1].toString();
    message.loginName = sender["slug"_L1].toString();
    message.displayName = username;
    message.messageText = plain;
    message.searchText = username + u": " + plain;
    message.serverReceivedTime = QDateTime::currentDateTime();

    this->addMessage(builder.release(), MessageContext::Original);
}

}  // namespace chatterino
