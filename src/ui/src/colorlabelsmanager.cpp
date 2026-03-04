/*
 * Copyright (C) 2021 Anton Filimonov and other contributors
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

#include "colorlabelsmanager.h"
#include "highlighterset.h"
#include <algorithm>
#include <vector>

namespace {

bool quickLabelMatchesText( const QuickLabelEntry& entry, const QString& text )
{
    const auto sensitivity
        = entry.ignoreCase ? Qt::CaseInsensitive : Qt::CaseSensitive;
    return QString::compare( entry.text, text, sensitivity ) == 0;
}

bool sameQuickLabelEntry( const QuickLabelEntry& lhs, const QuickLabelEntry& rhs )
{
    return lhs.text == rhs.text && lhs.ignoreCase == rhs.ignoreCase
           && lhs.wholeWord == rhs.wholeWord;
}

bool containsQuickLabelEntry( const AbstractLogView::QuickHighlighters& entries,
                              const QuickLabelEntry& entry )
{
    return std::any_of( entries.cbegin(), entries.cend(),
                        [ &entry ]( const auto& existingEntry ) {
                            return sameQuickLabelEntry( existingEntry, entry );
                        } );
}

} // namespace

ColorLabelsManager::QuickHighlightersCollection ColorLabelsManager::colorLabels() const
{
    return quickHighlighters_;
}

ColorLabelsManager::QuickHighlightersCollection ColorLabelsManager::clear()
{
    for ( auto& quickHighlighters : quickHighlighters_ ) {
        quickHighlighters.clear();
    }
    currentLabel_.reset();
    
    return quickHighlighters_;
}

ColorLabelsManager::QuickHighlightersCollection
ColorLabelsManager::setColorLabel( size_t label, const QString& text,
                                   QuickHighlighterDefaults defaults )
{
    return updateColorLabel( label, text, defaults );
}

ColorLabelsManager::QuickHighlightersCollection
ColorLabelsManager::setNextColorLabel( const QString& text, QuickHighlighterDefaults defaults )
{
    const auto& quickHighlightersConfiguration
        = HighlighterSetCollection::get().quickHighlighters();

    std::vector<size_t> cycle;
    cycle.reserve( static_cast<size_t>( quickHighlightersConfiguration.size() ) );

    for ( auto i = 0; i < quickHighlightersConfiguration.size(); ++i ) {
        if ( quickHighlightersConfiguration[ i ].useInCycle ) {
            cycle.push_back( static_cast<size_t>( i ) );
        }
    }

    if ( cycle.empty() ) {
        return quickHighlighters_;
    }

    auto nextLabel = cycle.front();

    auto currentLabel = currentLabel_;
    if ( !currentLabel ) {
        currentLabel = currentColorLabelForText( text );
    }

    if ( currentLabel.has_value() ) {
        auto nextIt = std::upper_bound( cycle.cbegin(), cycle.cend(), *currentLabel );
        if ( nextIt != cycle.cend() ) {
            nextLabel = *nextIt;
        }
    }

    currentLabel_ = nextLabel;

    return updateColorLabel( nextLabel, text, defaults );
}

ColorLabelsManager::QuickHighlightersCollection
ColorLabelsManager::removeColorLabel( const QString& text )
{
    if ( text.isEmpty() ) {
        return quickHighlighters_;
    }

    for ( auto& quickHighlighter : quickHighlighters_ ) {
        quickHighlighter.erase(
            std::remove_if( quickHighlighter.begin(), quickHighlighter.end(),
                            [ &text ]( const auto& entry ) {
                                return quickLabelMatchesText( entry, text );
                            } ),
            quickHighlighter.end() );
    }

    currentLabel_.reset();

    return quickHighlighters_;
}

std::optional<size_t> ColorLabelsManager::currentColorLabelForText( const QString& text ) const
{
    if ( text.isEmpty() ) {
        return {};
    }

    for ( auto i = 0u; i < quickHighlighters_.size(); ++i ) {
        if ( std::any_of( quickHighlighters_[ i ].cbegin(), quickHighlighters_[ i ].cend(),
                          [ &text ]( const auto& entry ) {
                              return quickLabelMatchesText( entry, text );
                          } ) ) {
            return i;
        }
    }

    return {};
}

ColorLabelsManager::QuickHighlightersCollection
ColorLabelsManager::updateColorLabel( size_t label, const QString& text,
                                      QuickHighlighterDefaults defaults )
{
    if ( text.isEmpty() || label >= quickHighlighters_.size() ) {
        return quickHighlighters_;
    }

    AbstractLogView::QuickHighlighters matchingEntries;
    for ( auto& quickHighlighter : quickHighlighters_ ) {
        quickHighlighter.erase(
            std::remove_if( quickHighlighter.begin(), quickHighlighter.end(),
                            [ &text, &matchingEntries ]( const auto& entry ) {
                                const auto isMatch = quickLabelMatchesText( entry, text );
                                if ( isMatch && !containsQuickLabelEntry( matchingEntries, entry ) ) {
                                    matchingEntries.append( entry );
                                }
                                return isMatch;
                            } ),
            quickHighlighter.end() );
    }

    if ( matchingEntries.empty() ) {
        matchingEntries.append( QuickLabelEntry{ text, defaults.ignoreCase, defaults.wholeWord } );
    }

    for ( const auto& entry : matchingEntries ) {
        if ( !containsQuickLabelEntry( quickHighlighters_[ label ], entry ) ) {
            quickHighlighters_[ label ].append( entry );
        }
    }

    currentLabel_ = label;

    return quickHighlighters_;
}
