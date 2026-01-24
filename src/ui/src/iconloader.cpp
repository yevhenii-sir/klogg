/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2007 QMUL.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include <QApplication>
#include <algorithm>
#include <cmath>
#include <QFile>
#include <QPainter>
#include <QPixmap>
#include <QStyleOption>
#include <QSvgRenderer>
#include <QWidget>

#include "configuration.h"
#include "iconloader.h"
#include "log.h"
#include "styles.h"

#include <array>

constexpr std::array<int, 5> IconSizes{ 0, 16, 20, 24, 32 };

IconLoader::IconLoader( QWidget* widget )
    : widget_{ widget }
{
}

QIcon IconLoader::load( QString name )
{
    QIcon icon;
    for ( int sz : IconSizes ) {
        QPixmap pmap( loadPixmap( name, sz ) );
        if ( !pmap.isNull() )
            icon.addPixmap( pmap );
    }
    return icon;
}
bool IconLoader::shouldInvert() const
{
    QStyleOption style;
    style.initFrom( widget_ );
    auto bg = style.palette.window().color();
    bool darkBackground = ( bg.red() + bg.green() + bg.blue() <= 384 );
    return darkBackground;
}

bool IconLoader::shouldAutoInvert( QString /*name*/ ) const
{
    return true;
}

bool IconLoader::shouldTint() const
{
    return true;
}

QColor IconLoader::iconTintColor() const
{
    QStyleOption style;
    style.initFrom( widget_ );
    return style.palette.buttonText().color();
}

bool IconLoader::isMostlyMonochrome( const QImage& img ) const
{
    if ( img.isNull() )
        return false;

    const int step = 4;
    int samples = 0;
    int mono = 0;

    for ( int y = 0; y < img.height(); y += step ) {
        for ( int x = 0; x < img.width(); x += step ) {
            const QRgb rgba = img.pixel( x, y );
            const int alpha = qAlpha( rgba );
            if ( alpha < 10 )
                continue;

            const int r = qRed( rgba );
            const int g = qGreen( rgba );
            const int b = qBlue( rgba );
            const int maxc = std::max( { r, g, b } );
            const int minc = std::min( { r, g, b } );
            if ( ( maxc - minc ) < 18 ) {
                ++mono;
            }
            ++samples;
        }
    }

    if ( samples == 0 )
        return false;

    return ( static_cast<double>( mono ) / static_cast<double>( samples ) ) > 0.75;
}

QPixmap IconLoader::loadPixmap( QString name, int size ) const
{
    bool invert = shouldInvert();
    QString nonScalableName;
    QPixmap pmap;
    bool svgLoaded = false;

    const int targetSize = size > 0 ? size : 16;
    const bool highDpi = widget_ ? widget_->devicePixelRatioF() >= 2.0 : false;
    const bool tintSvg = true;
    const QColor tintColor = iconTintColor();

    const auto tryLoad = [&]( bool useHighDpi, bool useInvert ) {
        nonScalableName = makeNonScalableFilename( name, size, useInvert, useHighDpi );
        pmap = QPixmap( nonScalableName );
        if ( !pmap.isNull() && useHighDpi ) {
            pmap.setDevicePixelRatio( 2.0 );
        }
        return !pmap.isNull();
    };

    const auto tryLoadSvg = [&]( bool useInvert ) {
        const auto svgName = makeScalableFilename( name, useInvert );
        pmap = renderSvg( svgName, targetSize, tintColor, tintSvg );
        svgLoaded = !pmap.isNull();
        return !pmap.isNull();
    };

    // attempt to load a pixmap with the right size and inversion
    if ( !tryLoadSvg( invert ) && invert ) {
        tryLoadSvg( false );
    }

    if ( pmap.isNull() ) {
        if ( !tryLoad( highDpi, invert ) && highDpi ) {
            tryLoad( false, invert );
        }
    }

    if ( pmap.isNull() && invert ) {
        // if that failed, and we were asking for an inverted pixmap,
        // that may mean we don't have an inverted version of it. We
        // could either auto-invert or use the uninverted version
        if ( !tryLoad( highDpi, false ) && highDpi ) {
            tryLoad( false, false );
        }

        if ( !pmap.isNull() && shouldAutoInvert( name ) ) {
            pmap = invertPixmap( pmap );
        }
    }

    if ( pmap.isNull() && size > 0 ) {
        // fallback to the base icon and scale if needed
        nonScalableName = makeNonScalableFilename( name, 0, invert, highDpi );
        pmap = QPixmap( nonScalableName );
        if ( !pmap.isNull() ) {
            const QSize targetSizePx( size, size );
            pmap = pmap.scaled( targetSizePx, Qt::KeepAspectRatio, Qt::SmoothTransformation );
        }
    }

    if ( shouldTint() && !pmap.isNull() && !svgLoaded ) {
        pmap = tintPixmap( pmap, iconTintColor() );
    }
    return pmap;
}

QString IconLoader::makeScalableFilename( QString name, bool invert ) const
{
    if ( invert ) {
        return QString( ":/images/%1_inverse.svg" ).arg( name );
    }
    return QString( ":/images/%1.svg" ).arg( name );
}

QString IconLoader::makeNonScalableFilename( QString name, int size, bool invert, bool highDpi ) const
{
    const QString scaleSuffix = highDpi ? QStringLiteral( "@2x" ) : QString();
    if ( invert ) {
        if ( size == 0 ) {
            return QString( ":/images/%1_inverse%2.png" ).arg( name ).arg( scaleSuffix );
        }
        else {
            return QString( ":/images/%1-%2_inverse%3.png" ).arg( name ).arg( size ).arg( scaleSuffix );
        }
    }
    else {
        if ( size == 0 ) {
            return QString( ":/images/%1%2.png" ).arg( name ).arg( scaleSuffix );
        }
        else {
            return QString( ":/images/%1-%2%3.png" ).arg( name ).arg( size ).arg( scaleSuffix );
        }
    }
}

QPixmap IconLoader::invertPixmap( QPixmap pmap ) const
{
    // No suitable inverted icon found for black background; try to
    // auto-invert the default one
    const qreal dpr = pmap.devicePixelRatio();
    QImage img = pmap.toImage().convertToFormat( QImage::Format_ARGB32 );
    for ( int y = 0; y < img.height(); ++y ) {
        for ( int x = 0; x < img.width(); ++x ) {
            QRgb rgba = img.pixel( x, y );
            QColor colour = QColor( qRed( rgba ), qGreen( rgba ), qBlue( rgba ), qAlpha( rgba ) );
            int alpha = colour.alpha();
            if ( colour.saturation() < 5 && colour.alpha() > 10 ) {
                colour.setHsv( colour.hue(), colour.saturation(), 255 - colour.value() );
                colour.setAlpha( alpha );
                img.setPixel( x, y, colour.rgba() );
            }
        }
    }
    pmap = QPixmap::fromImage( img );
    pmap.setDevicePixelRatio( dpr );
    return pmap;
}

QPixmap IconLoader::renderSvg( const QString& filename, int size, const QColor& tintColor,
                               bool tint ) const
{
    if ( !QFile::exists( filename ) ) {
        return QPixmap();
    }

    QSvgRenderer renderer( filename );
    if ( !renderer.isValid() ) {
        return QPixmap();
    }

    const qreal dpr = widget_ ? widget_->devicePixelRatioF() : 1.0;
    const int pixelSize = std::max( 1, static_cast<int>( std::round( size * dpr ) ) );
    QPixmap pixmap( QSize( pixelSize, pixelSize ) );
    pixmap.fill( Qt::transparent );

    QPainter painter( &pixmap );
    renderer.render( &painter );
    painter.end();

    pixmap.setDevicePixelRatio( dpr );

    if ( tint ) {
        pixmap = tintPixmap( pixmap, tintColor );
    }

    return pixmap;
}

QPixmap IconLoader::tintPixmap( QPixmap pmap, const QColor& color ) const
{
    const qreal dpr = pmap.devicePixelRatio();
    QImage img = pmap.toImage().convertToFormat( QImage::Format_ARGB32 );
    if ( !isMostlyMonochrome( img ) ) {
        return pmap;
    }

    QPainter painter( &img );
    painter.setCompositionMode( QPainter::CompositionMode_SourceIn );
    painter.fillRect( img.rect(), color );
    painter.end();

    QPixmap tinted = QPixmap::fromImage( img );
    tinted.setDevicePixelRatio( dpr );
    return tinted;
}
