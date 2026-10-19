// SPDX-FileCopyrightText: 2025 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QDialog>
#include <QDoubleSpinBox>
#include <QFont>
#include <QWidget>

namespace chatterino {

class PreviewWidget;
class FontFamilyWidget;
class FontSizeWidget;
class FontWeightWidget;

class FontDialog : public QDialog
{
    Q_OBJECT

public:
    FontDialog(const QFont &startFont, double startLetterSpacing,
               QWidget *parent = nullptr);

    /// Gets the currently selected font.
    QFont getSelected() const;

    /// Gets the selected letter spacing, as a percentage of the font size.
    double getSelectedLetterSpacing() const;

Q_SIGNALS:
    void applied();

private:
    void updatePreview();

    PreviewWidget *preview;
    FontFamilyWidget *familyW;
    FontSizeWidget *sizeW;
    FontWeightWidget *weightW;
    QDoubleSpinBox *letterSpacingW;
};

}  // namespace chatterino
