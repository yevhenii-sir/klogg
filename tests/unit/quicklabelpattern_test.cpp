#include <catch2/catch.hpp>

#include <QRegularExpression>

#include "highlighterset.h"
#include "quicklabelpattern.h"

namespace {

bool hasMatch( const QuickLabelEntry& entry, const QString& line )
{
    QRegularExpression::PatternOptions options
        = QRegularExpression::UseUnicodePropertiesOption;
    if ( entry.ignoreCase ) {
        options |= QRegularExpression::CaseInsensitiveOption;
    }

    const QRegularExpression regex( quicklabel::pattern( entry ), options );
    REQUIRE( regex.isValid() );
    return regex.match( line ).hasMatch();
}

} // namespace

TEST_CASE( "Whole-word quick label matches punctuation-bounded selections",
           "[colorlabels][quicklabel]" )
{
    const QuickLabelEntry bracketedError{ QStringLiteral( "[ERROR]" ), false, true };
    CHECK( hasMatch( bracketedError, QStringLiteral( "2026-03-05 [ERROR] boot failed" ) ) );
    CHECK_FALSE( hasMatch( bracketedError, QStringLiteral( "2026-03-05 A[ERROR] boot failed" ) ) );
    CHECK_FALSE( hasMatch( bracketedError, QStringLiteral( "2026-03-05 [ERROR]A boot failed" ) ) );

    const QuickLabelEntry trailingPunctuation{ QStringLiteral( "foo:" ), false, true };
    CHECK( hasMatch( trailingPunctuation, QStringLiteral( "foo: value=1" ) ) );
    CHECK_FALSE( hasMatch( trailingPunctuation, QStringLiteral( "xfoo: value=1" ) ) );
}

TEST_CASE( "Whole-word quick label still matches plain word tokens",
           "[colorlabels][quicklabel]" )
{
    const QuickLabelEntry plainWord{ QStringLiteral( "WARN" ), true, true };
    CHECK( hasMatch( plainWord, QStringLiteral( "warn disk pressure high" ) ) );
    CHECK_FALSE( hasMatch( plainWord, QStringLiteral( "prewarning from daemon" ) ) );
}
