// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/MenuBarIcon.hpp"

#include "Application.hpp"
#include "singletons/Settings.hpp"
#include "singletons/WindowManager.hpp"
#include "util/MacOsHelpers.h"
#include "widgets/Window.hpp"

#include <QApplication>
#include <QIcon>
#include <QMenu>
#include <QSystemTrayIcon>

namespace chatterino {

MenuBarIcon::MenuBarIcon(Window &mainWindow)
    : QObject(&mainWindow)
    , mainWindow_(mainWindow)
{
    getSettings()->menuBarMode.connect(
        [this](bool enabled) {
            this->apply(enabled);
        },
        this->signalHolder_);
}

void MenuBarIcon::apply(bool enabled)
{
    // The icon exists exactly while the mode is on. This also skips the first
    // call when it's off, before the main window is shown.
    if (enabled == (this->icon_ != nullptr))
    {
        return;
    }

    // Without the main window, closing the last overlay or popup window would
    // quit Chatterino. Quitting from the menu closes the main window, which
    // quits either way.
    QApplication::setQuitOnLastWindowClosed(!enabled);

    // Changing the activation policy deactivates the app, which would leave
    // the settings dialog the mode was just toggled in behind other apps
    const bool wasActive = QApplication::activeWindow() != nullptr;
    setMacOSDockIconVisible(!enabled);
    if (wasActive)
    {
        activateMacOSApp();
    }

    if (enabled)
    {
        this->createIcon();
        this->icon_->show();
        return;
    }

    delete this->icon_;
    this->icon_ = nullptr;

    // The main window can't be reached anymore once the icon is gone
    if (!this->mainWindow_.isVisible())
    {
        this->showMainWindow();
    }
}

void MenuBarIcon::createIcon()
{
    // Drawn by macOS in the menu bar's text color
    QIcon image(":/menubar-icon.svg");
    image.setIsMask(true);

    this->icon_ = new QSystemTrayIcon(image, this);
    this->icon_->setToolTip("Chatterino");

    // QSystemTrayIcon doesn't own its menu
    auto *menu = new QMenu();
    QObject::connect(this->icon_, &QObject::destroyed, menu,
                     &QObject::deleteLater);

    auto *toggle = menu->addAction("Show Chatterino");
    QObject::connect(menu, &QMenu::aboutToShow, toggle, [this, toggle] {
        toggle->setText(this->mainWindow_.isVisible() ? "Hide Chatterino"
                                                      : "Show Chatterino");
    });
    QObject::connect(toggle, &QAction::triggered, this, [this] {
        if (this->mainWindow_.isVisible())
        {
            this->mainWindow_.hide();
        }
        else
        {
            this->showMainWindow();
        }
    });

    QObject::connect(
        menu->addAction("Settings..."), &QAction::triggered, this, [this] {
            activateMacOSApp();
            getApp()->getWindows()->showSettingsDialog(&this->mainWindow_);
        });

    menu->addSeparator();

    // Closing the main window without a click on its close button quits, see
    // Window::closeEvent
    QObject::connect(menu->addAction("Quit Chatterino"), &QAction::triggered,
                     this, [this] {
                         this->mainWindow_.close();
                     });

    this->icon_->setContextMenu(menu);
}

void MenuBarIcon::showMainWindow()
{
    // Choosing a menu item doesn't make Chatterino the active app
    activateMacOSApp();
    if (this->mainWindow_.isMinimized())
    {
        this->mainWindow_.showNormal();
    }
    else
    {
        this->mainWindow_.show();
    }
    this->mainWindow_.raise();
    this->mainWindow_.activateWindow();
}

}  // namespace chatterino
