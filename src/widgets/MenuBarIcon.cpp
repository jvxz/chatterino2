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
#include <QTimer>

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

MenuBarIcon::~MenuBarIcon() = default;

void MenuBarIcon::apply(bool enabled)
{
    // This also skips the first call when the mode is off, before the main
    // window is shown
    if (enabled == (this->item_ != nullptr))
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
        return;
    }

    // Deferred: this can run from the icon's own menu
    QTimer::singleShot(0, [item = this->item_.release()] {
        delete item;
    });

    // The main window can't be reached anymore once the icon is gone
    if (!this->mainWindow_.isVisible())
    {
        this->showMainWindow();
    }
}

void MenuBarIcon::createIcon()
{
    // Rendered at 2x for Retina displays
    const auto image =
        QIcon(":/menubar-icon.svg").pixmap(QSize(36, 36)).toImage();
    this->item_ = std::make_unique<MacOSStatusItem>(image, "Chatterino");

    // Always shows and raises: the window may be open behind other apps
    this->item_->addAction("Show Chatterino", [this] {
        this->showMainWindow();
    });
    this->item_->addAction("Hide Chatterino", [this] {
        this->mainWindow_.hide();
    });
    this->item_->addAction("Settings", [this] {
        activateMacOSApp();
        getApp()->getWindows()->showSettingsDialog(&this->mainWindow_);
    });
    this->item_->addAction("Show in the Dock instead", [] {
        getSettings()->menuBarMode.setValue(false);
    });
    this->item_->addSeparator();
    // Closing the main window without a click on its close button quits, see
    // Window::closeEvent
    this->item_->addAction("Quit Chatterino", [this] {
        this->mainWindow_.close();
    });
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
