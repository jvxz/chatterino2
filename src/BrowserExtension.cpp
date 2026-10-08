// SPDX-FileCopyrightText: 2018 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "BrowserExtension.hpp"

#include "singletons/MultiPovSync.hpp"
#include "singletons/NativeMessaging.hpp"
#include "singletons/Paths.hpp"
#include "util/RenameThread.hpp"

#include <QColor>
#include <QFile>

#include <iostream>
#include <memory>
#include <thread>

#ifdef Q_OS_WIN
#    include <fcntl.h>
#    include <io.h>

#    include <cstdio>

#endif

namespace {

using namespace chatterino;

void initFileMode()
{
#ifdef Q_OS_WIN
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
}

// TODO(Qt6): Use QUtf8String
void sendToBrowser(QLatin1String str)
{
    auto len = static_cast<uint32_t>(str.size());
    std::cout.write(reinterpret_cast<const char *>(&len), sizeof(len));
    std::cout.write(str.data(), str.size());
    std::cout.flush();
}

QByteArray receiveFromBrowser()
{
    uint32_t size = 0;
    std::cin.read(reinterpret_cast<char *>(&size), sizeof(size));

    if (std::cin.eof())
    {
        return {};
    }

    QByteArray buffer{static_cast<QByteArray::size_type>(size),
                      Qt::Uninitialized};
    std::cin.read(buffer.data(), size);

    return buffer;
}

/// Sends the overlay's chat background to the browser when it changed, so the
/// lofi-nopixel page can match it under the overlay. Chatterino writes it when
/// it shows the overlay, see multipov::showOverlay.
void sendOverlayBackground(const QString &path, QByteArray &sent)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return;
    }
    const QColor color(QString::fromLatin1(file.read(16).trimmed()));
    if (!color.isValid())
    {
        return;
    }
    auto name = color.name().toLatin1();
    if (name == sent)
    {
        return;
    }
    sent = name;

    QByteArray message = R"({"type":"theme","background":")" + name + R"("})";
    sendToBrowser(QLatin1String{message});
}

void runLoop(const QString &overlayBackgroundPath)
{
    QByteArray sentBackground;
    auto receivedMessage = std::make_shared<std::atomic_bool>(true);

    auto thread = std::thread([=]() {
        while (true)
        {
            using namespace std::chrono_literals;
            if (!receivedMessage->exchange(false))
            {
                sendToBrowser(QLatin1String{
                    R"({"type":"status","status":"exiting-host","reason":"no message was received in 10s"})"});
                _Exit(1);
            }
            std::this_thread::sleep_for(10s);
        }
    });
    renameThread(thread, "BrowserPingCheck");

    while (true)
    {
        auto buffer = receiveFromBrowser();
        if (buffer.isNull())
        {
            break;
        }

        receivedMessage->store(true);

        nm::client::sendMessage(buffer);

        if (buffer.contains(R"("action":"overlay","pov")"))
        {
            sendOverlayBackground(overlayBackgroundPath, sentBackground);
        }
    }

    sendToBrowser(QLatin1String{
        R"({"type":"status","status":"exiting-host","reason":"received EOF"})"});
    _Exit(0);
}
}  // namespace

namespace chatterino {

void runBrowserExtensionHost(const Paths &paths)
{
    initFileMode();

    runLoop(multipov::overlayBackgroundPath(paths));
}

}  // namespace chatterino
