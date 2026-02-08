/*
 * Copyright (C) 2024 -- 2026 Anton Filimonov and other contributors
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

#include "concatenatedlogdata.h"
#include "logdata.h"
#include "log.h"

#include <algorithm>

ConcatenatedLogData::ConcatenatedLogData()
    : AbstractLogData()
{
}

void ConcatenatedLogData::addSource( std::shared_ptr<LogData> source )
{
    sources_.push_back( { source, 0_lcount } );

    // Connect to source signals for live updates
    connect( source.get(), &LogData::loadingFinished, this, [ this ]( LoadingStatus status ) {
        rebuildIndex();
        Q_EMIT loadingFinished( status );
    } );
    connect( source.get(), &LogData::fileChanged, this, [ this ]( MonitoredFileStatus status ) {
        rebuildIndex();
        Q_EMIT fileChanged( status );
    } );

    rebuildIndex();
}

void ConcatenatedLogData::rebuildIndex()
{
    totalLines_ = 0_lcount;
    maxLength_ = 0_length;

    for ( auto& info : sources_ ) {
        totalLines_ += info.logData->getNbLine();
        info.cumulativeLines = totalLines_;
        maxLength_ = qMax( maxLength_, info.logData->getMaxLength() );
    }
}

int ConcatenatedLogData::sourceCount() const
{
    return static_cast<int>( sources_.size() );
}

std::pair<int, LineNumber> ConcatenatedLogData::mapToSource( LineNumber globalLine ) const
{
    LineNumber::UnderlyingType offset = 0;
    for ( size_t i = 0; i < sources_.size(); ++i ) {
        const auto sourceLines = sources_[ i ].cumulativeLines.get();
        if ( globalLine.get() < sourceLines ) {
            return { static_cast<int>( i ), LineNumber( globalLine.get() - offset ) };
        }
        offset = sourceLines;
    }
    // Should not happen, but return last source's last line
    if ( !sources_.empty() ) {
        const auto lastIdx = sources_.size() - 1;
        const auto prevCum = lastIdx > 0 ? sources_[ lastIdx - 1 ].cumulativeLines.get() : 0;
        return { static_cast<int>( lastIdx ), LineNumber( globalLine.get() - prevCum ) };
    }
    return { 0, 0_lnum };
}

std::shared_ptr<LogData> ConcatenatedLogData::sourceAt( int index ) const
{
    return sources_.at( static_cast<size_t>( index ) ).logData;
}

QString ConcatenatedLogData::doGetLineString( LineNumber line ) const
{
    const auto [ srcIdx, localLine ] = mapToSource( line );
    return sources_[ static_cast<size_t>( srcIdx ) ].logData->getLineString( localLine );
}

QString ConcatenatedLogData::doGetExpandedLineString( LineNumber line ) const
{
    const auto [ srcIdx, localLine ] = mapToSource( line );
    return sources_[ static_cast<size_t>( srcIdx ) ].logData->getExpandedLineString( localLine );
}

klogg::vector<QString> ConcatenatedLogData::doGetLines( LineNumber first, LinesCount number ) const
{
    klogg::vector<QString> result;
    result.reserve( number.get() );

    for ( LinesCount::UnderlyingType i = 0; i < number.get(); ++i ) {
        result.push_back( doGetLineString( first + LinesCount( i ) ) );
    }
    return result;
}

klogg::vector<QString> ConcatenatedLogData::doGetExpandedLines( LineNumber first,
                                                                 LinesCount number ) const
{
    klogg::vector<QString> result;
    result.reserve( number.get() );

    for ( LinesCount::UnderlyingType i = 0; i < number.get(); ++i ) {
        result.push_back( doGetExpandedLineString( first + LinesCount( i ) ) );
    }
    return result;
}

LineNumber ConcatenatedLogData::doGetLineNumber( LineNumber index ) const
{
    return index;
}

LinesCount ConcatenatedLogData::doGetNbLine() const
{
    return totalLines_;
}

LineLength ConcatenatedLogData::doGetMaxLength() const
{
    return maxLength_;
}

LineLength ConcatenatedLogData::doGetLineLength( LineNumber line ) const
{
    const auto [ srcIdx, localLine ] = mapToSource( line );
    return sources_[ static_cast<size_t>( srcIdx ) ].logData->getLineLength( localLine );
}

void ConcatenatedLogData::doSetDisplayEncoding( const char* encoding )
{
    for ( auto& info : sources_ ) {
        info.logData->setDisplayEncoding( encoding );
    }
}

QTextCodec* ConcatenatedLogData::doGetDisplayEncoding() const
{
    if ( !sources_.empty() ) {
        return sources_.front().logData->getDisplayEncoding();
    }
    return nullptr;
}

void ConcatenatedLogData::doAttachReader() const
{
    for ( const auto& info : sources_ ) {
        info.logData->attachReader();
    }
}

void ConcatenatedLogData::doDetachReader() const
{
    for ( const auto& info : sources_ ) {
        info.logData->detachReader();
    }
}
