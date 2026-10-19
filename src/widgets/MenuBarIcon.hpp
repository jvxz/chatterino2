// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <pajlada/signals/signalholder.hpp>
#include <QObject>

#include <memory>

namespace chatterino {

class MacOSStatusItem;
class Window;

/// macOS menu bar mode: while the `menuBarMode` setting is on, Chatterino has
/// an icon in the menu bar instead of the Dock, and closing the main window
/// only hides it. The browser overlays keep working while it's hidden.
class MenuBarIcon : public QObject
{
public:
    /// Follows the setting for as long as `mainWindow` exists
    explicit MenuBarIcon(Window &mainWindow);
    ~MenuBarIcon() override;
    MenuBarIcon(const MenuBarIcon &) = delete;
    MenuBarIcon &operator=(const MenuBarIcon &) = delete;
    MenuBarIcon(MenuBarIcon &&) = delete;
    MenuBarIcon &operator=(MenuBarIcon &&) = delete;

private:
    void apply(bool enabled);
    void createIcon();
    void showMainWindow();

    Window &mainWindow_;
    /// Exists exactly while the mode is on
    std::unique_ptr<MacOSStatusItem> item_;
    pajlada::Signals::SignalHolder signalHolder_;
};

}  // namespace chatterino
