/*
 * Copyright (C) 2014, 2015 Nicolas Bonnefon and other contributors
 *
 * This file is part of glogg.
 *
 * glogg is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * glogg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with glogg.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "tabbedcrawlerwidget.h"

#include <QApplication>
#include <QClipboard>
#include <QColorDialog>
#include <QDir>
#include <QPalette>
#include <QFileInfo>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>
#include <qobjectdefs.h>
#include <qpoint.h>

#include "crawlerwidget.h"

#include "clipboard.h"
#include "configuration.h"
#include "dispatch_to.h"
#include "iconloader.h"
#include "log.h"
#include "openfilehelper.h"
#include "styles.h"
#include "tabgroup.h"
#include "tabnamemapping.h"

namespace {
constexpr QLatin1String PathKey = QLatin1String( "path", 4 );
constexpr QLatin1String StatusKey = QLatin1String( "status", 6 );

bool shouldUseDarkTabIcons( const QWidget* widget, const Configuration& config )
{
    if ( config.style() == StyleManager::DarkStyleKey
         || config.style() == StyleManager::DarkWindowsStyleKey ) {
        return true;
    }
    if ( !widget ) {
        return false;
    }
    const QColor windowColor = widget->palette().color( QPalette::Window );
    return ( windowColor.red() + windowColor.green() + windowColor.blue() ) <= 384;
}
} // namespace

TabbedCrawlerWidget::TabbedCrawlerWidget()
    : QTabWidget()
    , newdata_icon_()
    , newfiltered_icon_()
{
    updateTabBarStyle();

    setTabBar( &myTabBar_ );
    myTabBar_.hide();

    myTabBar_.setContextMenuPolicy( Qt::CustomContextMenu );
    connect( &myTabBar_, &CrawlerTabBar::showTabContextMenu, this,
             &TabbedCrawlerWidget::showContextMenu );

    // Connect to group manager for tab appearance updates
    auto& groupManager = TabGroupManager::getSynced();
    connect( &groupManager, &TabGroupManager::groupsChanged, this,
             &TabbedCrawlerWidget::onGroupsChanged );

    dispatchToMainThread( [ this ] { loadIcons(); } );
}

void TabbedCrawlerWidget::loadIcons()
{
    IconLoader iconLoader{ this };
    olddata_icon_ = iconLoader.load( "olddata_icon" );
    newdata_icon_ = iconLoader.load( "newdata_icon" );
    newfiltered_icon_ = iconLoader.load( "newfiltered_icon" );
    for ( int tab = 0; tab < count(); ++tab ) {
        updateIcon( tab );
    }
}

void TabbedCrawlerWidget::updateTabBarStyle()
{
    const auto& config = Configuration::get();
    if ( config.style() == StyleManager::ModernKey ) {
        myTabBar_.setStyleSheet( "" );
        return;
    }

    QString tabStyle = "QTabBar::tab { height: 24px; }";
    QString tabCloseButtonStyle = " QTabBar::close-button {\
              height: 12px; width: 12px;\
              subcontrol-origin: padding;\
              subcontrol-position: right;\
              %1}";

    QString backgroundImage;
    QString backgroundHoverImage;

    const bool useDarkIcons = shouldUseDarkTabIcons( this, config );
    if ( useDarkIcons ) {
        backgroundImage = ":/images/icons8-close-window_inverse.svg";
        backgroundHoverImage = ":/images/icons8-close-window-hover_inverse.svg";
    }

#if defined( Q_OS_MAC )
    // work around Qt MacOSX bug missing tab close icons
    // see: https://bugreports.qt.io/browse/QTBUG-61092
    // still broken in document mode in Qt.5.12.2 !!!!
    if ( !useDarkIcons ) {
        backgroundImage
            = ":/qt-project.org/styles/commonstyle/images/standardbutton-closetab-16.png";
        backgroundHoverImage
            = ":/qt-project.org/styles/commonstyle/images/standardbutton-closetab-hover-16.png";
    }
#elif defined( Q_OS_WIN )
    if ( !useDarkIcons && config.style() == StyleManager::FusionKey ) {
        backgroundImage = ":/images/icons8-close-window.svg";
        backgroundHoverImage = ":/images/icons8-close-window-hover.svg";
    }
#endif

    if ( !backgroundImage.isEmpty() ) {
        const QString backgroundImageTemplate = " image: url(%1);";
        QString tabCloseButtonHoverStyle = " QTabBar::close-button:hover { %1 }";
        backgroundImage = backgroundImageTemplate.arg( backgroundImage );
        backgroundHoverImage = backgroundImageTemplate.arg( backgroundHoverImage );
        tabCloseButtonHoverStyle = tabCloseButtonHoverStyle.arg( backgroundHoverImage );
        tabCloseButtonStyle = tabCloseButtonStyle.arg( backgroundImage );
        tabCloseButtonStyle.append( tabCloseButtonHoverStyle );
    }
    else {
        tabCloseButtonStyle = tabCloseButtonStyle.arg( "" );
    }

    myTabBar_.setStyleSheet( tabStyle.append( tabCloseButtonStyle ) );
}

void TabbedCrawlerWidget::changeEvent( QEvent* event )
{
    if ( event->type() == QEvent::StyleChange || event->type() == QEvent::PaletteChange ) {
        updateTabBarStyle();
        dispatchToMainThread( [ this ] { loadIcons(); } );
    }

    QWidget::changeEvent( event );
}

void TabbedCrawlerWidget::addTabBarItem( int index, const QString& fileName )
{
    const auto tabLabel = QFileInfo( fileName ).fileName();
    const auto tabName = TabNameMapping::getSynced().tabName( fileName );

    myTabBar_.setTabIcon( index, olddata_icon_ );
    myTabBar_.setTabText( index, tabName.isEmpty() ? tabLabel : tabName );
    myTabBar_.setTabToolTip( index, QDir::toNativeSeparators( fileName ) );

    QVariantMap tabData;
    tabData[ PathKey ] = fileName;
    tabData[ StatusKey ] = static_cast<int>( DataStatus::OLD_DATA );

    myTabBar_.setTabData( index, tabData );

    setCurrentIndex( index );

    if ( count() > 1 )
        myTabBar_.show();
}

void TabbedCrawlerWidget::removeCrawler( int index )
{
    QTabWidget::removeTab( index );

    if ( count() <= 1 )
        myTabBar_.hide();
}

void TabbedCrawlerWidget::mouseReleaseEvent( QMouseEvent* event )
{
    LOG_DEBUG << "TabbedCrawlerWidget::mouseReleaseEvent";

    if ( event->button() == Qt::MiddleButton ) {
        int tab = this->myTabBar_.tabAt( event->pos() );
        if ( -1 != tab ) {
            Q_EMIT tabCloseRequested( tab );
            event->accept();
        }
    }

    event->ignore();
}

QString TabbedCrawlerWidget::tabPathAt( int index ) const
{
    return myTabBar_.tabData( index ).toMap()[ PathKey ].toString();
}

void CrawlerTabBar::mouseReleaseEvent( QMouseEvent* mouseEvent )
{
    if ( mouseEvent->button() == Qt::RightButton ) {
        int tab = tabAt( mouseEvent->pos() );
        if ( tab != -1 ) {
            Q_EMIT showTabContextMenu( tab, mapToGlobal( mouseEvent->pos() ) );
            mouseEvent->accept();
        }
    }

    mouseEvent->ignore();
}

void TabbedCrawlerWidget::showContextMenu( int tab, QPoint globalPoint )
{
    QMenu menu( this );
    auto closeThis = menu.addAction( tr( "Close this" ) );
    auto closeOthers = menu.addAction( tr( "Close others" ) );
    auto closeLeft = menu.addAction( tr( "Close to the left" ) );
    auto closeRight = menu.addAction( tr( "Close to the right" ) );
    auto closeAll = menu.addAction( tr( "Close all" ) );
    menu.addSeparator();
    auto copyFullPath = menu.addAction( tr( "Copy full path" ) );
    auto openContainingFolder = menu.addAction( tr( "Open containing folder" ) );
    menu.addSeparator();

    // Tab grouping submenu
    auto groupMenu = menu.addMenu( tr( "Add to Group" ) );
    buildGroupSubmenu( groupMenu, tab );

    const auto tabPath = tabPathAt( tab );
    auto& groupManager = TabGroupManager::getSynced();
    const auto currentGroupId = groupManager.groupIdForTab( tabPath );
    if ( !currentGroupId.isEmpty() ) {
        auto removeFromGroup = menu.addAction( tr( "Remove from Group" ) );
        connect( removeFromGroup, &QAction::triggered, this, [ tabPath, &groupManager ] {
            groupManager.removeTabFromGroup( tabPath );
            groupManager.save();
        } );
    }

    menu.addSeparator();
    auto renameTab = menu.addAction( tr( "Rename tab" ) );
    auto resetTabName = menu.addAction( tr( "Reset tab name" ) );

    connect( closeThis, &QAction::triggered, [ tab, this ] { Q_EMIT tabCloseRequested( tab ); } );

    connect( closeOthers, &QAction::triggered, [ tabWidget = widget( tab ), this ] {
        while ( count() != 1 ) {
            for ( int i = 0; i < count(); ++i ) {
                if ( i != indexOf( tabWidget ) ) {
                    Q_EMIT tabCloseRequested( i );
                    break;
                }
            }
        }
    } );

    connect( closeLeft, &QAction::triggered, [ tabWidget = widget( tab ), this ] {
        while ( indexOf( tabWidget ) != 0 ) {
            Q_EMIT tabCloseRequested( 0 );
        }
    } );

    connect( closeRight, &QAction::triggered, [ tab, this ] {
        while ( count() > tab + 1 ) {
            Q_EMIT tabCloseRequested( tab + 1 );
        }
    } );

    connect( closeAll, &QAction::triggered, [ this ] {
        while ( count() ) {
            Q_EMIT tabCloseRequested( 0 );
        }
    } );

    if ( tab == 0 ) {
        closeLeft->setDisabled( true );
    }
    else if ( tab == count() - 1 ) {
        closeRight->setDisabled( true );
    }

    connect( copyFullPath, &QAction::triggered, this,
             [ this, tab ] { sendTextToClipboard( tabToolTip( tab ) ); } );

    connect( openContainingFolder, &QAction::triggered, this,
             [ this, tab ] { showPathInFileExplorer( tabToolTip( tab ) ); } );

    connect( renameTab, &QAction::triggered, this, [ this, tab, tabPath ] {
        bool isNameEntered = false;
        auto newName = QInputDialog::getText( this, "Rename tab", "Tab name", QLineEdit::Normal,
                                              myTabBar_.tabText( tab ), &isNameEntered );
        if ( isNameEntered ) {
            TabNameMapping::getSynced().setTabName( tabPath, newName ).save();

            if ( newName.isEmpty() ) {
                myTabBar_.setTabText( tab, QFileInfo( tabPath ).fileName() );
            }
            else {
                myTabBar_.setTabText( tab, std::move( newName ) );
            }
        }
    } );

    connect( resetTabName, &QAction::triggered, this, [ this, tab, tabPath ] {
        TabNameMapping::getSynced().setTabName( tabPath, "" ).save();
        myTabBar_.setTabText( tab, QFileInfo( tabPath ).fileName() );
    } );

    menu.exec( globalPoint );
}

void TabbedCrawlerWidget::keyPressEvent( QKeyEvent* event )
{
    const auto mod = event->modifiers();
    const auto key = event->key();

    LOG_DEBUG << "TabbedCrawlerWidget::keyPressEvent";

    // Ctrl + tab
    if ( ( mod == Qt::ControlModifier && key == Qt::Key_Tab )
         || ( mod == Qt::ControlModifier && key == Qt::Key_PageDown )
         || ( mod == ( Qt::ControlModifier | Qt::AltModifier | Qt::KeypadModifier )
              && key == Qt::Key_Right ) ) {
        setCurrentIndex( ( currentIndex() + 1 ) % count() );
    }
    // Ctrl + shift + tab
    else if ( ( mod == ( Qt::ControlModifier | Qt::ShiftModifier ) && key == Qt::Key_Tab )
              || ( mod == Qt::ControlModifier && key == Qt::Key_PageUp )
              || ( mod == ( Qt::ControlModifier | Qt::AltModifier | Qt::KeypadModifier )
                   && key == Qt::Key_Left ) ) {
        setCurrentIndex( ( currentIndex() - 1 >= 0 ) ? currentIndex() - 1 : count() - 1 );
    }
    // Ctrl + numbers
    else if ( mod == Qt::ControlModifier && ( key >= Qt::Key_1 && key <= Qt::Key_8 ) ) {
        int newIndex = key - Qt::Key_0;
        if ( newIndex <= count() )
            setCurrentIndex( newIndex - 1 );
    }
    // Ctrl + 9
    else if ( mod == Qt::ControlModifier && key == Qt::Key_9 ) {
        setCurrentIndex( count() - 1 );
    }
    else if ( mod == Qt::ControlModifier && ( key == Qt::Key_Q || key == Qt::Key_W ) ) {
        Q_EMIT tabCloseRequested( currentIndex() );
    }
    else {
        QTabWidget::keyPressEvent( event );
    }
}

void TabbedCrawlerWidget::updateIcon( int index )
{
    auto tabData = myTabBar_.tabData( index ).toMap();

    const QIcon* icon;
    switch ( static_cast<DataStatus>( tabData[ StatusKey ].toInt() ) ) {
    case DataStatus::OLD_DATA:
        icon = &olddata_icon_;
        break;
    case DataStatus::NEW_DATA:
        icon = &newdata_icon_;
        break;
    case DataStatus::NEW_FILTERED_DATA:
        icon = &newfiltered_icon_;
        break;
    default:
        return;
    }

    myTabBar_.setTabIcon( index, *icon );
}

void TabbedCrawlerWidget::setTabDataStatus( int index, DataStatus status )
{
    LOG_DEBUG << "TabbedCrawlerWidget::setTabDataStatus " << index;

    auto tabData = myTabBar_.tabData( index ).toMap();
    tabData[ StatusKey ] = static_cast<int>( status );
    myTabBar_.setTabData( index, tabData );

    updateIcon( index );
}

void TabbedCrawlerWidget::buildGroupSubmenu( QMenu* menu, int tabIndex )
{
    auto& groupManager = TabGroupManager::getSynced();
    const auto tabPath = tabPathAt( tabIndex );
    const auto currentGroupId = groupManager.groupIdForTab( tabPath );

    // Add existing groups
    for ( const auto& group : groupManager.groups() ) {
        QPixmap colorIcon( 12, 12 );
        colorIcon.fill( group.color );

        auto* action = menu->addAction( QIcon( colorIcon ), group.name );
        action->setCheckable( true );
        action->setChecked( group.id == currentGroupId );

        connect( action, &QAction::triggered, this, [ tabPath, groupId = group.id, &groupManager ] {
            groupManager.addTabToGroup( groupId, tabPath );
            groupManager.save();
        } );
    }

    if ( !groupManager.groups().isEmpty() ) {
        menu->addSeparator();
    }

    // Add "New Group..." action
    auto* newGroupAction = menu->addAction( tr( "New Group..." ) );
    connect( newGroupAction, &QAction::triggered, this,
             [ this, tabIndex ] { createNewGroupDialog( tabIndex ); } );
}

void TabbedCrawlerWidget::createNewGroupDialog( int tabIndex )
{
    bool ok = false;
    const auto groupName
        = QInputDialog::getText( this, tr( "New Tab Group" ), tr( "Group name:" ),
                                 QLineEdit::Normal, tr( "New Group" ), &ok );
    if ( !ok || groupName.trimmed().isEmpty() ) {
        return;
    }

    // Show color picker
    const QColor defaultColor( "#5B8CFF" );
    const QColor color = QColorDialog::getColor( defaultColor, this, tr( "Choose Group Color" ) );
    if ( !color.isValid() ) {
        return;
    }

    auto& groupManager = TabGroupManager::getSynced();
    groupManager.createGroup( groupName.trimmed(), color );

    // Add the current tab to the new group
    const auto tabPath = tabPathAt( tabIndex );
    const auto& groups = groupManager.groups();
    if ( !groups.isEmpty() ) {
        groupManager.addTabToGroup( groups.last().id, tabPath );
    }

    groupManager.save();
}

void TabbedCrawlerWidget::onGroupsChanged()
{
    // Update tab appearance based on groups
    for ( int i = 0; i < count(); ++i ) {
        const auto tabPath = tabPathAt( i );
        auto& groupManager = TabGroupManager::get();
        const auto* group = groupManager.groupForTab( tabPath );

        // Get original tab name (without any group prefix)
        const auto tabName = TabNameMapping::getSynced().tabName( tabPath );
        const auto tabLabel = tabName.isEmpty() ? QFileInfo( tabPath ).fileName() : tabName;

        if ( group ) {
            // Update tooltip to show group membership
            const auto originalTooltip = QDir::toNativeSeparators( tabPath );
            myTabBar_.setTabToolTip( i, tr( "%1\nGroup: %2" ).arg( originalTooltip, group->name ) );

            // Set tab text color to group color for visual indicator
            myTabBar_.setTabTextColor( i, group->color );

            // Add group indicator prefix to tab text
            myTabBar_.setTabText( i, QString::fromUtf8( "\xE2\x97\x8F " ) + tabLabel ); // Circle prefix
        }
        else {
            myTabBar_.setTabToolTip( i, QDir::toNativeSeparators( tabPath ) );

            // Reset to default text color
            myTabBar_.setTabTextColor( i, palette().color( QPalette::WindowText ) );

            // Remove any group prefix
            myTabBar_.setTabText( i, tabLabel );
        }
    }
}

void TabbedCrawlerWidget::showGroupContextMenu( const QString& groupId, QPoint globalPoint )
{
    auto& groupManager = TabGroupManager::getSynced();
    auto* group = groupManager.groupById( groupId );
    if ( !group ) {
        return;
    }

    QMenu menu( this );

    auto* collapseAction = menu.addAction( group->collapsed ? tr( "Expand Group" ) : tr( "Collapse Group" ) );
    connect( collapseAction, &QAction::triggered, this, [ groupId, &groupManager ] {
        groupManager.toggleCollapsed( groupId );
        groupManager.save();
    } );

    menu.addSeparator();

    auto* renameAction = menu.addAction( tr( "Rename Group..." ) );
    connect( renameAction, &QAction::triggered, this, [ this, groupId, groupName = group->name, &groupManager ] {
        bool ok = false;
        const auto newName = QInputDialog::getText( this, tr( "Rename Group" ), tr( "Group name:" ),
                                                    QLineEdit::Normal, groupName, &ok );
        if ( ok && !newName.trimmed().isEmpty() ) {
            groupManager.renameGroup( groupId, newName.trimmed() );
            groupManager.save();
        }
    } );

    auto* colorAction = menu.addAction( tr( "Change Color..." ) );
    connect( colorAction, &QAction::triggered, this, [ this, groupId, groupColor = group->color, &groupManager ] {
        const QColor newColor = QColorDialog::getColor( groupColor, this, tr( "Choose Group Color" ) );
        if ( newColor.isValid() ) {
            groupManager.setGroupColor( groupId, newColor );
            groupManager.save();
        }
    } );

    menu.addSeparator();

    auto* ungroupAction = menu.addAction( tr( "Ungroup All" ) );
    connect( ungroupAction, &QAction::triggered, this, [ groupId, &groupManager ] {
        groupManager.ungroupAll( groupId );
        groupManager.save();
    } );

    auto* closeAllAction = menu.addAction( tr( "Close All in Group" ) );
    connect( closeAllAction, &QAction::triggered, this, [ this, groupId, &groupManager ] {
        const auto* targetGroup = groupManager.groupById( groupId );
        if ( targetGroup ) {
            // Collect tabs to close
            QList<int> tabsToClose;
            for ( int i = 0; i < count(); ++i ) {
                if ( targetGroup->tabPaths.contains( tabPathAt( i ) ) ) {
                    tabsToClose.prepend( i ); // Prepend to close from end
                }
            }
            for ( int tabIndex : tabsToClose ) {
                Q_EMIT tabCloseRequested( tabIndex );
            }
        }
    } );

    menu.exec( globalPoint );
}
