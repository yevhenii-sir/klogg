/*
 * Copyright (C) 2016 -- 2019 Anton Filimonov and other contributors
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

#include <QAction>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QKeySequence>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <QToolBar>

#include "test_utils.h"

#include "crawlerwidget.h"
#include "log.h"
#include "mainwindow.h"
#include "session.h"

SCENARIO( "Main window tests", "[ui]" )
{
    auto appSession = std::make_shared<Session>();
    WindowSession windowSession{ appSession, "Main", 0 };

    std::unique_ptr<MainWindow> mainWindow;
    std::unique_ptr<SafeQSignalSpy> activateSpy;
    std::unique_ptr<SafeQSignalSpy> exitSpy;
    QTimer::singleShot( 0, [&] {
        LOG_INFO << "Initialize main window";
        mainWindow.reset( new MainWindow( windowSession ) );
        exitSpy.reset( new SafeQSignalSpy( mainWindow.get(), SIGNAL( exitRequested() ) ) );
        activateSpy.reset( new SafeQSignalSpy( mainWindow.get(), SIGNAL( windowActivated() ) ) );
    } );

    QTest::qWait( 100 );
    mainWindow->show();
    QTest::qWait( 100 );
    REQUIRE( activateSpy->safeWait() );

    auto runInUiThread = [uiObject = mainWindow.get()]( auto&& func ) {
        QTimer::singleShot( 0, Qt::VeryCoarseTimer, uiObject,
                            std::forward<decltype( func )>( func ) );
        QTest::qWait( 100 );
    };

    GIVEN( "Opened main window" )
    {
        auto toolBar = mainWindow->findChild<QToolBar*>();
        REQUIRE( toolBar != nullptr );

        auto filePathLabel = toolBar->findChild<PathLine*>();
        REQUIRE( filePathLabel != nullptr );

        auto tabArea = mainWindow->findChild<TabbedCrawlerWidget*>();
        REQUIRE( tabArea != nullptr );

        QTemporaryDir tempDir;
        REQUIRE( tempDir.isValid() );
        const auto testFilePath = tempDir.filePath( "klogg.conf" );
        {
            QFile testFile( testFilePath );
            REQUIRE( testFile.open( QIODevice::WriteOnly | QIODevice::Truncate ) );
            testFile.write( "test\n" );
        }

        THEN( "Has no tabs" )
        {
            REQUIRE( tabArea->count() == 0 );
            AND_THEN( "Path label empty" )
            {
                REQUIRE( filePathLabel->text().isEmpty() );
            }
        }

        QAction* closeActionByShortcut = nullptr;
        const auto closeKeyBindings = QKeySequence::keyBindings( QKeySequence::Close );
        for ( auto* action : mainWindow->findChildren<QAction*>() ) {
            const auto shortcuts = action->shortcuts();
            for ( const auto& shortcut : shortcuts ) {
                for ( const auto& closeKey : closeKeyBindings ) {
                    if ( shortcut.matches( closeKey ) == QKeySequence::ExactMatch ) {
                        closeActionByShortcut = action;
                        break;
                    }
                }
                if ( closeActionByShortcut ) {
                    break;
                }
            }
            if ( closeActionByShortcut ) {
                break;
            }
        }
        REQUIRE( closeActionByShortcut != nullptr );

        WHEN( "Exit hotkey pressed" )
        {
            runInUiThread( [&mainWindow] {
                LOG_INFO << "ExitFromMainMenu";
                QTest::keyPress( mainWindow.get(), Qt::Key_Q, Qt::ControlModifier );
            } );

            THEN( "Exit signalled" )
            {
                REQUIRE( exitSpy->safeWait() );
            }
        }

        WHEN( "Load file" )
        {
            runInUiThread( [&mainWindow, testFilePath] {
                LOG_INFO << "Load file";
                mainWindow->loadInitialFile( testFilePath, false );
            } );

            THEN( "Path line has file name" )
            {
                REQUIRE(
                    waitUiState( [&] { return filePathLabel->text().contains( "klogg.conf" ); } ) );

                AND_THEN( "Has one tab" )
                {
                    REQUIRE( waitUiState( [&] { return tabArea->count() == 1; } ) );
                }
            }

            AND_WHEN( "Close tab hotkey pressed" )
            {
                runInUiThread( [closeActionByShortcut] {
                    LOG_INFO << "Close tab";
                    closeActionByShortcut->trigger();
                } );

                THEN( "Has no tabs" )
                {
                    REQUIRE( waitUiState( [&] { return tabArea->count() == 0; } ) );

                    AND_THEN( "Path label empty" )
                    {
                        REQUIRE( waitUiState( [&] { return filePathLabel->text().isEmpty(); } ) );
                    }
                }
            }
        }
    }
}

// Helper: write raw bytes to a temp file and return its path
static QString writeTestFile( const QTemporaryDir& dir, const QString& name,
                              const QByteArray& content )
{
    const auto path = dir.filePath( name );
    QFile f( path );
    REQUIRE( f.open( QIODevice::WriteOnly ) );
    f.write( content );
    f.close();
    return path;
}

// Helper: read the merged temp file produced by openMerged
static QByteArray readMergedFile( Session& session, const std::vector<QString>& sources,
                                  const QString& tempDir )
{
    auto* view = session.openMerged(
        sources, []() { return new CrawlerWidget(); }, tempDir );
    REQUIRE( view != nullptr );

    const auto mergedPath = session.getFilename( view );
    QFile mergedFile( mergedPath );
    REQUIRE( mergedFile.open( QIODevice::ReadOnly ) );
    const auto result = mergedFile.readAll();

    session.close( view );
    return result;
}

SCENARIO( "Session::openMerged produces correct merged file", "[session][merge]" )
{
    auto appSession = std::make_shared<Session>();
    QTemporaryDir tempDir;
    REQUIRE( tempDir.isValid() );

    GIVEN( "Two files that both end with newlines" )
    {
        const auto file1 = writeTestFile( tempDir, "a.log", "line1\nline2\n" );
        const auto file2 = writeTestFile( tempDir, "b.log", "line3\nline4\n" );

        WHEN( "They are merged" )
        {
            const auto merged = readMergedFile( *appSession, { file1, file2 }, tempDir.path() );

            THEN( "Content is concatenated without extra separator lines" )
            {
                REQUIRE( merged == QByteArray( "line1\nline2\nline3\nline4\n" ) );
            }
        }
    }

    GIVEN( "First file does not end with newline" )
    {
        const auto file1 = writeTestFile( tempDir, "no_nl.log", "line1\nline2" );
        const auto file2 = writeTestFile( tempDir, "with_nl.log", "line3\n" );

        WHEN( "They are merged" )
        {
            const auto merged = readMergedFile( *appSession, { file1, file2 }, tempDir.path() );

            THEN( "A newline is inserted between files" )
            {
                REQUIRE( merged == QByteArray( "line1\nline2\nline3\n" ) );
            }
        }
    }

    GIVEN( "Last file does not end with newline" )
    {
        const auto file1 = writeTestFile( tempDir, "first.log", "line1\n" );
        const auto file2 = writeTestFile( tempDir, "last.log", "line2" );

        WHEN( "They are merged" )
        {
            const auto merged = readMergedFile( *appSession, { file1, file2 }, tempDir.path() );

            THEN( "No trailing newline is appended to the last file" )
            {
                REQUIRE( merged == QByteArray( "line1\nline2" ) );
            }
        }
    }

    GIVEN( "Files containing non-UTF-8 bytes (Latin-1)" )
    {
        // Latin-1 bytes: 0xE9 = 'e with acute', 0xF1 = 'n with tilde'
        const QByteArray latin1Content = QByteArray( "caf\xe9\n", 5 );
        const QByteArray latin1Content2 = QByteArray( "ni\xf1o\n", 5 );
        const auto file1 = writeTestFile( tempDir, "latin1_a.log", latin1Content );
        const auto file2 = writeTestFile( tempDir, "latin1_b.log", latin1Content2 );

        WHEN( "They are merged" )
        {
            const auto merged = readMergedFile( *appSession, { file1, file2 }, tempDir.path() );

            THEN( "Raw bytes are preserved exactly" )
            {
                REQUIRE( merged == latin1Content + latin1Content2 );
            }
        }
    }

    GIVEN( "Files containing binary-like content with null bytes" )
    {
        const QByteArray binaryContent = QByteArray( "abc\x00\x01\x02\n", 7 );
        const QByteArray textContent = QByteArray( "text\n", 5 );
        const auto file1 = writeTestFile( tempDir, "binary.log", binaryContent );
        const auto file2 = writeTestFile( tempDir, "text.log", textContent );

        WHEN( "They are merged" )
        {
            const auto merged = readMergedFile( *appSession, { file1, file2 }, tempDir.path() );

            THEN( "Binary bytes including nulls are preserved" )
            {
                REQUIRE( merged == binaryContent + textContent );
            }
        }
    }

    GIVEN( "An empty file merged with a non-empty file" )
    {
        const auto file1 = writeTestFile( tempDir, "empty.log", QByteArray() );
        const auto file2 = writeTestFile( tempDir, "nonempty.log", "content\n" );

        WHEN( "They are merged" )
        {
            const auto merged = readMergedFile( *appSession, { file1, file2 }, tempDir.path() );

            THEN( "Only the non-empty content appears, no extra newlines" )
            {
                REQUIRE( merged == QByteArray( "content\n" ) );
            }
        }
    }

    GIVEN( "Three files with mixed newline endings" )
    {
        const auto file1 = writeTestFile( tempDir, "f1.log", "a\n" );
        const auto file2 = writeTestFile( tempDir, "f2.log", "b" );
        const auto file3 = writeTestFile( tempDir, "f3.log", "c\n" );

        WHEN( "They are merged" )
        {
            const auto merged
                = readMergedFile( *appSession, { file1, file2, file3 }, tempDir.path() );

            THEN( "Separator newline added only between f2 and f3" )
            {
                REQUIRE( merged == QByteArray( "a\nb\nc\n" ) );
            }
        }
    }

    GIVEN( "Merged file path is a valid file in tempDir" )
    {
        const auto file1 = writeTestFile( tempDir, "p1.log", "hello\n" );
        const auto file2 = writeTestFile( tempDir, "p2.log", "world\n" );

        WHEN( "openMerged is called" )
        {
            auto* view = appSession->openMerged(
                { file1, file2 }, []() { return new CrawlerWidget(); }, tempDir.path() );
            REQUIRE( view != nullptr );

            THEN( "getFilename returns a real file path inside tempDir" )
            {
                const auto mergedPath = appSession->getFilename( view );
                REQUIRE( QFileInfo::exists( mergedPath ) );
                REQUIRE( mergedPath.startsWith( tempDir.path() ) );
                REQUIRE( mergedPath.contains( "klogg_merged_" ) );
            }

            appSession->close( view );
        }
    }
}
