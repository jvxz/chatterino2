// SPDX-FileCopyrightText: 2019 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "pajlada/settings/settinglistener.hpp"

#include <pajlada/signals/signal.hpp>
#include <QFont>
#include <QFontMetrics>

#include <unordered_map>
#include <vector>

namespace chatterino {

class Settings;
class Paths;

/** @exposeenum c2.FontStyle */
enum class FontStyle : uint8_t {
    Tiny,
    ChatSmall,
    ChatMediumSmall,
    ChatMedium,
    ChatMediumBold,
    ChatMediumItalic,
    ChatLarge,
    ChatVeryLarge,

    TimestampMedium,

    UiMedium,
    UiMediumBold,
    UiTabs,

    // don't remove this value
    EndType,

    // make sure to update these values accordingly!
    ChatStart = ChatSmall,
    ChatEnd = ChatVeryLarge,
};

/// Stands in for the platform's UI font (San Francisco on macOS) wherever a
/// font family is stored, since that font's real name is a private one.
inline const QString SYSTEM_FONT_FAMILY = QStringLiteral("System Font");

/// Makes a font from the font settings.
///
/// @param family A font family, or SYSTEM_FONT_FAMILY
/// @param letterSpacing Extra space after each letter, as a percentage of the
///                      font size
QFont makeFont(const QString &family, int pointSize, int weight, bool italic,
               double letterSpacing);

class Fonts final
{
public:
    explicit Fonts(Settings &settings);

    // font data gets set in createFontData(...)

    QFont getFont(FontStyle type, float scale);
    QFontMetricsF getFontMetrics(FontStyle type, float scale);

    pajlada::Signals::NoArgSignal fontChanged;

private:
    struct FontData {
        FontData(const QFont &_font)
            : font(_font)
            , metrics(_font)
        {
        }

        const QFont font;
        const QFontMetricsF metrics;
    };

    struct ChatFontData {
        float scale;
        bool italic;
    };

    struct UiFontData {
        float size;
        const char *name;
        bool italic;
        int weight;
    };

    FontData &getOrCreateFontData(FontStyle type, float scale);
    static FontData createFontData(FontStyle type, float scale);

    std::vector<std::unordered_map<float, FontData>> fontsByType_;

    pajlada::SettingListener fontChangedListener;
};

}  // namespace chatterino
