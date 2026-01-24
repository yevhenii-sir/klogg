/*
 * Copyright (C) 2026
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

#include <sstream>

#include <QDir>
#include <QStringList>

#include "cli.h"

namespace {

struct ExitException {
    int code;
};

class StreamCapture {
  public:
    explicit StreamCapture( std::ostream& stream )
        : stream_( stream )
        , old_buf_( stream.rdbuf( buffer_.rdbuf() ) )
    {
    }

    ~StreamCapture()
    {
        stream_.rdbuf( old_buf_ );
    }

    std::string str() const
    {
        return buffer_.str();
    }

  private:
    std::ostream& stream_;
    std::stringstream buffer_;
    std::streambuf* old_buf_ = nullptr;
};

QStringList makeArgs( std::initializer_list<const char*> args )
{
    QStringList list;
    for ( const auto* arg : args ) {
        list.push_back( QString::fromUtf8( arg ) );
    }
    return list;
}

} // namespace

TEST_CASE( "CliParameters --version prints custom version output and exits" )
{
    StreamCapture capture( std::cout );

    try {
        CliParameters params(
            makeArgs( { "klogg", "--version" } ), false,
            []( int code ) { throw ExitException{ code }; } );
        FAIL( "Expected version path to request exit" );
    } catch ( const ExitException& ex ) {
        REQUIRE( ex.code == EXIT_SUCCESS );
    }

    const auto output = capture.str();
    REQUIRE( output.find( "klogg " ) != std::string::npos );
    REQUIRE( output.find( "Built " ) != std::string::npos );
}

TEST_CASE( "CliParameters handles invalid option by exiting with failure" )
{
    StreamCapture capture( std::cerr );

    try {
        CliParameters params(
            makeArgs( { "klogg", "--no-such-option" } ), false,
            []( int code ) { throw ExitException{ code }; } );
        FAIL( "Expected invalid option to request exit" );
    } catch ( const ExitException& ex ) {
        REQUIRE( ex.code == EXIT_FAILURE );
    }

    REQUIRE_FALSE( capture.str().empty() );
}

TEST_CASE( "CliParameters parses console mode pattern and filename" )
{
    bool exit_called = false;
    const auto expected_path = QDir::current().absoluteFilePath( "cli_test.log" );

    CliParameters params(
        makeArgs( { "klogg_grep", "--pattern", "error", "cli_test.log" } ), true,
        [ &exit_called ]( int ) { exit_called = true; } );

    REQUIRE_FALSE( exit_called );
    REQUIRE( params.pattern == "error" );
    REQUIRE( params.filenames.size() == 1 );
    REQUIRE( params.filenames.front() == expected_path );
}

TEST_CASE( "CliParameters parses GUI mode flags" )
{
    bool exit_called = false;
    const auto expected_path = QDir::current().absoluteFilePath( "gui_test.log" );

    CliParameters params(
        makeArgs( { "klogg", "-m", "-f", "--window-width", "800", "--window-height", "600",
                    "gui_test.log" } ),
        false, [ &exit_called ]( int ) { exit_called = true; } );

    REQUIRE_FALSE( exit_called );
    REQUIRE( params.multi_instance );
    REQUIRE( params.follow_file );
    REQUIRE( params.window_width == 800 );
    REQUIRE( params.window_height == 600 );
    REQUIRE( params.filenames.size() == 1 );
    REQUIRE( params.filenames.front() == expected_path );
}
