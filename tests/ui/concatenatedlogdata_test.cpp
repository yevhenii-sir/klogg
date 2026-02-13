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

#include <catch2/catch.hpp>

#include <QCoreApplication>
#include <QEventLoop>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QTest>
#include <QTimer>

#include "test_utils.h"

#include "concatenatedlogdata.h"
#include "logdata.h"

namespace {

// Helper: create a temp file with specified lines, return (file, shared LogData)
struct TestLogSource {
    // QTemporaryFile auto-remove can race with background file watcher teardown
    // on Windows, so keep cleanup under test lifecycle control.
    QTemporaryFile file{ "concat_test_XXXXXX" };
    QString filePath;
    std::shared_ptr<LogData> logData = std::make_shared<LogData>();
    bool attached = false;

    TestLogSource()
    {
        file.setAutoRemove( false );
    }

    ~TestLogSource()
    {
        shutdown();
    }

    void shutdown()
    {
        if ( logData ) {
            logData->interruptLoading();
            if ( attached ) {
                logData->detachReader();
            }
            QCoreApplication::processEvents( QEventLoop::AllEvents, 20 );
            logData.reset();
            QCoreApplication::processEvents( QEventLoop::AllEvents, 20 );
            attached = false;
        }

        if ( file.isOpen() ) {
            file.close();
        }
    }

    bool create( const QStringList& lines )
    {
        if ( !file.open() ) {
            return false;
        }
        for ( const auto& line : lines ) {
            file.write( line.toUtf8() );
            file.write( "\n" );
        }
        file.flush();
        filePath = QFileInfo{ file }.absoluteFilePath();
        file.close();

        QEventLoop loop;
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot( true );

        auto receivedSignal = false;
        auto status = LoadingStatus::Interrupted;

        auto loadingFinishedConnection
            = QObject::connect( logData.get(), &LogData::loadingFinished, &loop,
                                [ & ]( LoadingStatus loadingStatus ) {
                                    status = loadingStatus;
                                    receivedSignal = true;
                                    loop.quit();
                                } );
        auto timeoutConnection
            = QObject::connect( &timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit );

        logData->attachFile( filePath );
        attached = true;

        timeoutTimer.start( 10000 );
        if ( !receivedSignal ) {
            loop.exec();
        }

        QObject::disconnect( loadingFinishedConnection );
        QObject::disconnect( timeoutConnection );

        if ( !receivedSignal || status != LoadingStatus::Successful ) {
            return false;
        }

        const auto expectedLines
            = LinesCount( static_cast<LinesCount::UnderlyingType>( lines.size() ) );
        return waitUiState( [ & ] { return logData->getNbLine() == expectedLines; } );
    }
};

} // namespace

SCENARIO( "empty concatenated log data", "[concatenatedlogdata]" )
{
    GIVEN( "a default-constructed ConcatenatedLogData" )
    {
        ConcatenatedLogData concat;

        THEN( "it has zero lines" )
        {
            REQUIRE( concat.getNbLine() == 0_lcount );
        }

        THEN( "it has zero max length" )
        {
            REQUIRE( concat.getMaxLength() == 0_length );
        }

        THEN( "sourceCount is zero" )
        {
            REQUIRE( concat.sourceCount() == 0 );
        }

        THEN( "mapToSource returns default for any line" )
        {
            auto [idx, localLine] = concat.mapToSource( 0_lnum );
            REQUIRE( idx == 0 );
            REQUIRE( localLine == 0_lnum );
        }
    }
}

SCENARIO( "single source concatenated log data", "[concatenatedlogdata]" )
{
    GIVEN( "a ConcatenatedLogData with one source" )
    {
        TestLogSource src;
        REQUIRE( src.create( { "alpha", "bravo", "charlie" } ) );

        ConcatenatedLogData concat;
        concat.addSource( src.logData );

        THEN( "sourceCount is 1" )
        {
            REQUIRE( concat.sourceCount() == 1 );
        }

        THEN( "total lines match source" )
        {
            REQUIRE( concat.getNbLine() == 3_lcount );
        }

        THEN( "maxLength matches source" )
        {
            REQUIRE( concat.getMaxLength() == src.logData->getMaxLength() );
        }

        THEN( "getLineString returns correct content" )
        {
            REQUIRE( concat.getLineString( 0_lnum ) == "alpha" );
            REQUIRE( concat.getLineString( 1_lnum ) == "bravo" );
            REQUIRE( concat.getLineString( 2_lnum ) == "charlie" );
        }

        THEN( "getExpandedLineString returns correct content" )
        {
            REQUIRE( concat.getExpandedLineString( 0_lnum ) == "alpha" );
        }

        THEN( "getLineLength returns correct lengths" )
        {
            REQUIRE( concat.getLineLength( 0_lnum ) == LineLength( 5 ) );
            REQUIRE( concat.getLineLength( 2_lnum ) == LineLength( 7 ) );
        }

        THEN( "mapToSource maps correctly" )
        {
            auto [idx, localLine] = concat.mapToSource( 0_lnum );
            REQUIRE( idx == 0 );
            REQUIRE( localLine == 0_lnum );

            auto [idx2, localLine2] = concat.mapToSource( 2_lnum );
            REQUIRE( idx2 == 0 );
            REQUIRE( localLine2 == 2_lnum );
        }

        THEN( "sourceAt returns the correct source" )
        {
            REQUIRE( concat.sourceAt( 0 ) == src.logData );
        }

        THEN( "getLines returns multiple lines" )
        {
            auto lines = concat.getLines( 0_lnum, 3_lcount );
            REQUIRE( lines.size() == 3 );
            REQUIRE( lines[ 0 ] == "alpha" );
            REQUIRE( lines[ 1 ] == "bravo" );
            REQUIRE( lines[ 2 ] == "charlie" );
        }

        THEN( "getExpandedLines returns multiple lines" )
        {
            auto lines = concat.getExpandedLines( 1_lnum, 2_lcount );
            REQUIRE( lines.size() == 2 );
            REQUIRE( lines[ 0 ] == "bravo" );
            REQUIRE( lines[ 1 ] == "charlie" );
        }
    }
}

SCENARIO( "multi-source concatenated log data", "[concatenatedlogdata]" )
{
    GIVEN( "a ConcatenatedLogData with three sources" )
    {
        TestLogSource src1, src2, src3;
        REQUIRE( src1.create( { "aaa", "bbb" } ) );
        REQUIRE( src2.create( { "ccc", "ddd", "eee" } ) );
        REQUIRE( src3.create( { "fff" } ) );

        ConcatenatedLogData concat;
        concat.addSource( src1.logData );
        concat.addSource( src2.logData );
        concat.addSource( src3.logData );

        THEN( "sourceCount is 3" )
        {
            REQUIRE( concat.sourceCount() == 3 );
        }

        THEN( "total lines is sum of all sources" )
        {
            REQUIRE( concat.getNbLine() == 6_lcount );
        }

        WHEN( "reading lines across source boundaries" )
        {
            THEN( "lines from first source are correct" )
            {
                REQUIRE( concat.getLineString( 0_lnum ) == "aaa" );
                REQUIRE( concat.getLineString( 1_lnum ) == "bbb" );
            }

            THEN( "lines from second source are correct" )
            {
                REQUIRE( concat.getLineString( 2_lnum ) == "ccc" );
                REQUIRE( concat.getLineString( 3_lnum ) == "ddd" );
                REQUIRE( concat.getLineString( 4_lnum ) == "eee" );
            }

            THEN( "lines from third source are correct" )
            {
                REQUIRE( concat.getLineString( 5_lnum ) == "fff" );
            }
        }

        WHEN( "mapping lines to sources" )
        {
            THEN( "first source lines map correctly" )
            {
                auto [idx, local] = concat.mapToSource( 0_lnum );
                REQUIRE( idx == 0 );
                REQUIRE( local == 0_lnum );

                auto [idx2, local2] = concat.mapToSource( 1_lnum );
                REQUIRE( idx2 == 0 );
                REQUIRE( local2 == 1_lnum );
            }

            THEN( "second source lines map correctly" )
            {
                auto [idx, local] = concat.mapToSource( 2_lnum );
                REQUIRE( idx == 1 );
                REQUIRE( local == 0_lnum );

                auto [idx2, local2] = concat.mapToSource( 4_lnum );
                REQUIRE( idx2 == 1 );
                REQUIRE( local2 == 2_lnum );
            }

            THEN( "third source lines map correctly" )
            {
                auto [idx, local] = concat.mapToSource( 5_lnum );
                REQUIRE( idx == 2 );
                REQUIRE( local == 0_lnum );
            }
        }

        WHEN( "reading lines spanning source boundaries" )
        {
            auto lines = concat.getLines( 1_lnum, 3_lcount );

            THEN( "returns lines from multiple sources" )
            {
                REQUIRE( lines.size() == 3 );
                REQUIRE( lines[ 0 ] == "bbb" );
                REQUIRE( lines[ 1 ] == "ccc" );
                REQUIRE( lines[ 2 ] == "ddd" );
            }
        }

        WHEN( "checking maxLength" )
        {
            THEN( "returns max across all sources" )
            {
                // All lines are 3 chars, so maxLength should be 3
                REQUIRE( concat.getMaxLength() == LineLength( 3 ) );
            }
        }

        WHEN( "checking line lengths" )
        {
            THEN( "returns correct length for each line" )
            {
                for ( LineNumber::UnderlyingType i = 0; i < 6; ++i ) {
                    REQUIRE( concat.getLineLength( LineNumber( i ) ) == LineLength( 3 ) );
                }
            }
        }

        WHEN( "sourceAt returns correct sources" )
        {
            REQUIRE( concat.sourceAt( 0 ) == src1.logData );
            REQUIRE( concat.sourceAt( 1 ) == src2.logData );
            REQUIRE( concat.sourceAt( 2 ) == src3.logData );
        }
    }
}

SCENARIO( "concatenated log data with varying line lengths", "[concatenatedlogdata]" )
{
    GIVEN( "sources with different line lengths" )
    {
        TestLogSource src1, src2;
        REQUIRE( src1.create( { "short", "a]" } ) );
        REQUIRE( src2.create(
            { "this is a much longer line for testing max length calculation" } ) );

        ConcatenatedLogData concat;
        concat.addSource( src1.logData );
        concat.addSource( src2.logData );

        THEN( "maxLength reflects the longest line across all sources" )
        {
            auto longestLen = src2.logData->getMaxLength();
            REQUIRE( concat.getMaxLength() == longestLen );
        }

        THEN( "individual line lengths are correct" )
        {
            REQUIRE( concat.getLineLength( 0_lnum ) == LineLength( 5 ) );   // "short"
            REQUIRE( concat.getLineLength( 1_lnum ) == LineLength( 2 ) );   // "a]"
            REQUIRE( concat.getLineLength( 2_lnum ) == src2.logData->getLineLength( 0_lnum ) );
        }
    }
}

SCENARIO( "concatenated log data encoding delegation", "[concatenatedlogdata]" )
{
    GIVEN( "a ConcatenatedLogData with sources" )
    {
        TestLogSource src1, src2;
        REQUIRE( src1.create( { "line1" } ) );
        REQUIRE( src2.create( { "line2" } ) );

        ConcatenatedLogData concat;
        concat.addSource( src1.logData );
        concat.addSource( src2.logData );

        THEN( "getDisplayEncoding returns first source encoding" )
        {
            auto encoding = concat.getDisplayEncoding();
            REQUIRE( encoding != nullptr );
            REQUIRE( encoding == src1.logData->getDisplayEncoding() );
        }
    }
}

SCENARIO( "concatenated log data attach/detach reader", "[concatenatedlogdata]" )
{
    GIVEN( "a ConcatenatedLogData with sources" )
    {
        TestLogSource src1, src2;
        REQUIRE( src1.create( { "line1" } ) );
        REQUIRE( src2.create( { "line2" } ) );

        ConcatenatedLogData concat;
        concat.addSource( src1.logData );
        concat.addSource( src2.logData );

        WHEN( "attaching and detaching readers" )
        {
            THEN( "does not crash" )
            {
                concat.attachReader();
                concat.detachReader();
                // If we get here without crashing, the test passes
                REQUIRE( true );
            }
        }
    }
}

SCENARIO( "concatenated log data repeated lifecycle", "[concatenatedlogdata][stability]" )
{
    constexpr int Iterations = 20;

    for ( int iteration = 0; iteration < Iterations; ++iteration ) {
        TestLogSource src1, src2, src3;
        REQUIRE( src1.create( { "aaa", "bbb" } ) );
        REQUIRE( src2.create( { "ccc", "ddd", "eee" } ) );
        REQUIRE( src3.create( { "fff" } ) );

        {
            ConcatenatedLogData concat;
            concat.addSource( src1.logData );
            concat.addSource( src2.logData );
            concat.addSource( src3.logData );

            REQUIRE( concat.sourceCount() == 3 );
            REQUIRE( concat.getNbLine() == 6_lcount );
            REQUIRE( concat.getLineString( 0_lnum ) == "aaa" );
            REQUIRE( concat.getLineString( 5_lnum ) == "fff" );

            concat.attachReader();
            concat.detachReader();
        }

        QCoreApplication::processEvents( QEventLoop::AllEvents, 20 );
        QTest::qWait( 1 );
    }
}
