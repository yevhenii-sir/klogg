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

#include "tabgroupdropresolver.h"

TEST_CASE( "resolveTabDropTargetGroupId picks group when both neighbors are same group" )
{
    REQUIRE( resolveTabDropTargetGroupId( "group-A", "group-A", "group-A" ) == "group-A" );
}

TEST_CASE( "resolveTabDropTargetGroupId picks the only non-empty neighbor group" )
{
    REQUIRE( resolveTabDropTargetGroupId( QString{}, "left-group", QString{} ) == "left-group" );
    REQUIRE( resolveTabDropTargetGroupId( QString{}, QString{}, "right-group" ) == "right-group" );
}

TEST_CASE( "resolveTabDropTargetGroupId keeps current group for ambiguous cross-group boundaries" )
{
    REQUIRE( resolveTabDropTargetGroupId( "group-A", "group-A", "group-B" ) == "group-A" );
    REQUIRE( resolveTabDropTargetGroupId( "group-B", "group-A", "group-B" ) == "group-B" );
    REQUIRE( resolveTabDropTargetGroupId( "group-C", "group-A", "group-B" ) == "group-C" );
}

TEST_CASE( "resolveTabDropTargetGroupId keeps tab ungrouped when both boundaries are ambiguous" )
{
    REQUIRE( resolveTabDropTargetGroupId( QString{}, "group-A", "group-B" ).isEmpty() );
}

TEST_CASE( "resolveTabDropTargetGroupId returns empty when both neighbors have no group" )
{
    REQUIRE( resolveTabDropTargetGroupId( QString{}, QString{}, QString{} ).isEmpty() );
    REQUIRE( resolveTabDropTargetGroupId( "group-A", QString{}, QString{} ).isEmpty() );
}
