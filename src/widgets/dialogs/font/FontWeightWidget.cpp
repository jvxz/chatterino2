// SPDX-FileCopyrightText: 2025 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "widgets/dialogs/font/FontWeightWidget.hpp"

#include "singletons/Fonts.hpp"
#include "widgets/dialogs/font/IntItem.hpp"

#include <QBoxLayout>
#include <QFontDatabase>
#include <QLabel>
#include <QList>
#include <QSignalBlocker>
#include <QString>
#include <QStringList>

namespace chatterino {

namespace {

/// Get a list of weights available for the given font family
QList<int> getWeights(const QString &family)
{
    QList<int> weights;
    QStringList styles = QFontDatabase::styles(
        family == SYSTEM_FONT_FAMILY
            ? QFontDatabase::systemFont(QFontDatabase::GeneralFont).family()
            : family);

    for (const QString &style : styles)
    {
        int weight = QFontDatabase::weight(family, style);
        if (!weights.contains(weight))
        {
            weights.append(weight);
        }
    }

    if (weights.isEmpty() && family == SYSTEM_FONT_FAMILY)
    {
        // The system font's private family can be missing from the database
        weights = {QFont::Light, QFont::Normal, QFont::Medium, QFont::DemiBold,
                   QFont::Bold};
    }

    return weights;
}

}  // namespace

FontWeightWidget::FontWeightWidget(const QFont &startFont, QWidget *parent)
    : QWidget(parent)
    , list(new QListWidget)
    , edit(new QSpinBox)
{
    auto *layout = new QVBoxLayout;
    auto *header = new QHBoxLayout;

    this->setLayout(layout);

    // Below 100, Qt reads weights as legacy ones (0-99)
    this->edit->setRange(100, 1000);
    this->edit->setSingleStep(10);

    this->setFamily(startFont.family());
    this->edit->setValue(startFont.weight());
    this->setListSelected(startFont.weight());

    layout->addLayout(header);
    layout->addWidget(this->list);
    layout->setContentsMargins(0, 0, 0, 0);

    header->addWidget(new QLabel("Weight"));
    header->addWidget(this->edit);

    QObject::connect(this->edit, &QSpinBox::valueChanged, this,
                     [this](int value) {
                         QSignalBlocker listSignalBlocker(this->list);
                         this->setListSelected(value);
                         Q_EMIT this->selectedChanged();
                     });

    QObject::connect(
        this->list, &QListWidget::currentRowChanged, this, [this](int row) {
            if (row < 0)
            {
                return;
            }
            auto *cast = dynamic_cast<IntItem *>(this->list->item(row));
            assert(cast);
            QSignalBlocker editSignalBlocker(this->edit);
            this->edit->setValue(cast->getValue());
            Q_EMIT this->selectedChanged();
        });
}

void FontWeightWidget::setFamily(const QString &family)
{
    QSignalBlocker listSignalBlocker{this->list};

    QList<int> weights = getWeights(family);
    int currentCount = this->list->count();
    int newCount = static_cast<int>(weights.count());
    int leastCount = std::min(currentCount, newCount);

    std::ranges::sort(weights);

    for (int i = 0; i < leastCount; ++i)
    {
        auto *cast = dynamic_cast<IntItem *>(this->list->item(i));
        assert(cast);
        cast->setValue(weights[i]);
        cast->setHidden(false);
    }
    for (int i = currentCount; i < newCount; ++i)
    {
        this->list->addItem(new IntItem(weights[i]));
    }
    for (int i = newCount; i < currentCount; ++i)
    {
        this->list->item(i)->setHidden(true);
    }

    int startRow = 0;
    for (int closest = INT_MAX, i = 0; i < weights.count(); ++i)
    {
        int diff = std::abs(weights[i] - QFont::Normal);
        if (diff < closest)
        {
            closest = diff;
            startRow = i;
        }
    }

    this->list->setCurrentRow(startRow);

    QSignalBlocker editSignalBlocker{this->edit};
    if (auto *cast = dynamic_cast<IntItem *>(this->list->currentItem()))
    {
        this->edit->setValue(cast->getValue());
    }
}

int FontWeightWidget::getSelected() const
{
    return this->edit->value();
}

void FontWeightWidget::setListSelected(int weight)
{
    for (int i = 0; i < this->list->count(); ++i)
    {
        auto *item = dynamic_cast<IntItem *>(this->list->item(i));
        if (item && !item->isHidden() && item->getValue() == weight)
        {
            this->list->setCurrentItem(item);
            return;
        }
    }

    // A weight in between the listed ones
    this->list->setCurrentRow(-1);
}

}  // namespace chatterino
