/*
 * Copyright (C) 2024 Anton Filimonov and other contributors
 *
 * This file is part of klogg.
 *
 * klogg is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * klogg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with klogg.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SAVEFAVORITEDIALOG_H
#define SAVEFAVORITEDIALOG_H

#include <QButtonGroup>
#include <QComboBox>
#include <QDialog>
#include <QLineEdit>
#include <QRadioButton>

#include "predefinedfilters.h"

class SaveFavoriteDialog : public QDialog {
    Q_OBJECT

  public:
    explicit SaveFavoriteDialog( const QString& defaultName,
                                 const PredefinedFiltersCollection::Collection& existingFilters,
                                 QWidget* parent = nullptr );

    bool isCreateNew() const;
    QString favoriteName() const;
    int selectedExistingIndex() const;

  private Q_SLOTS:
    void updateOkButtonState();
    void onModeChanged();

  private:
    QButtonGroup* modeButtonGroup_;
    QRadioButton* createNewRadio_;
    QRadioButton* overwriteRadio_;
    QLineEdit* newNameEdit_;
    QComboBox* existingCombo_;

    PredefinedFiltersCollection::Collection existingFilters_;
};

#endif // SAVEFAVORITEDIALOG_H
