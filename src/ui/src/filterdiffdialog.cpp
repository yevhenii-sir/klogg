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

#include "filterdiffdialog.h"

#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

FilterDiffDialog::FilterDiffDialog( const QString& filterName, const PredefinedFilter& existing,
                                    const QString& newPattern, bool newUseRegex, QWidget* parent )
    : QDialog( parent )
{
    setWindowTitle( tr( "Overwrite Filter: \"%1\"" ).arg( filterName ) );
    setMinimumSize( 500, 300 );
    setSizeGripEnabled( true );

    auto* mainLayout = new QVBoxLayout( this );

    // Comparison layout
    auto* comparisonLayout = new QHBoxLayout();

    // Existing filter group
    auto* existingGroup = new QGroupBox( tr( "Existing" ), this );
    auto* existingLayout = new QVBoxLayout( existingGroup );

    auto* existingPatternLabel = new QLabel( tr( "Pattern:" ), this );
    auto* existingPatternScroll = new QScrollArea( this );
    existingPatternScroll->setWidgetResizable( true );
    existingPatternScroll->setFrameShape( QFrame::StyledPanel );
    auto* existingPatternText = new QLabel( existing.pattern, this );
    existingPatternText->setWordWrap( true );
    existingPatternText->setTextInteractionFlags( Qt::TextSelectableByMouse );
    existingPatternText->setMargin( 4 );
    existingPatternScroll->setWidget( existingPatternText );
    existingPatternScroll->setMinimumHeight( 80 );

    auto* existingRegexLabel
        = new QLabel( tr( "Regex: %1" ).arg( existing.useRegex ? tr( "On" ) : tr( "Off" ) ), this );

    existingLayout->addWidget( existingPatternLabel );
    existingLayout->addWidget( existingPatternScroll, 1 );
    existingLayout->addWidget( existingRegexLabel );

    // New filter group
    auto* newGroup = new QGroupBox( tr( "New" ), this );
    auto* newLayout = new QVBoxLayout( newGroup );

    auto* newPatternLabel = new QLabel( tr( "Pattern:" ), this );
    auto* newPatternScroll = new QScrollArea( this );
    newPatternScroll->setWidgetResizable( true );
    newPatternScroll->setFrameShape( QFrame::StyledPanel );
    auto* newPatternText = new QLabel( newPattern, this );
    newPatternText->setWordWrap( true );
    newPatternText->setTextInteractionFlags( Qt::TextSelectableByMouse );
    newPatternText->setMargin( 4 );
    newPatternScroll->setWidget( newPatternText );
    newPatternScroll->setMinimumHeight( 80 );

    auto* newRegexLabel
        = new QLabel( tr( "Regex: %1" ).arg( newUseRegex ? tr( "On" ) : tr( "Off" ) ), this );

    newLayout->addWidget( newPatternLabel );
    newLayout->addWidget( newPatternScroll, 1 );
    newLayout->addWidget( newRegexLabel );

    comparisonLayout->addWidget( existingGroup );
    comparisonLayout->addWidget( newGroup );

    mainLayout->addLayout( comparisonLayout, 1 );

    // Button box
    auto* buttonBox = new QDialogButtonBox( this );
    buttonBox->addButton( QDialogButtonBox::Cancel );
    auto* overwriteButton = buttonBox->addButton( tr( "Overwrite" ), QDialogButtonBox::AcceptRole );
    overwriteButton->setDefault( true );

    mainLayout->addWidget( buttonBox );

    connect( buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept );
    connect( buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject );
}
