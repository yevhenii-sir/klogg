/*
 * Copyright (C) 2025 klogg contributors
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

#include "ios_device_manager.h"

#ifdef KLOGG_WITH_IMOBILEDEVICE

#include "log.h"

#include <QCoreApplication>
#include <QMutex>
#include <QtConcurrent>

#include <cstring>

namespace {

// Query device info off the GUI thread to avoid blocking the UI
// for 5-10 seconds per device (lockdown calls are slow).
IosDeviceManager::DeviceInfo queryDeviceInfoOffThread( const QString& udid )
{
    IosDeviceManager::DeviceInfo info;
    info.udid = udid;

    idevice_t device = nullptr;
    auto idevResult
        = idevice_new_with_options( &device, udid.toUtf8().constData(),
                                    (idevice_options)( IDEVICE_LOOKUP_USBMUX ) );
    if ( idevResult != IDEVICE_E_SUCCESS ) {
        idevResult = idevice_new_with_options( &device, udid.toUtf8().constData(),
                                               (idevice_options)( IDEVICE_LOOKUP_NETWORK ) );
        if ( idevResult != IDEVICE_E_SUCCESS ) {
            LOG_WARNING << "Failed to connect to device " << udid << ": " << idevResult;
            return info;
        }
    }

    lockdownd_client_t lockdown = nullptr;
    auto lockdownResult = lockdownd_client_new( device, &lockdown, "klogg" );
    if ( lockdownResult != LOCKDOWN_E_SUCCESS ) {
        LOG_WARNING << "Failed to create lockdownd client for " << udid << ": " << lockdownResult;
        idevice_free( device );
        return info;
    }

    // Try to start a session — if it works, the device is paired/trusted
    // If it fails, syslog_relay may still work, so we mark as available regardless
    char* sessionId = nullptr;
    int sslEnabled = 0;
    auto sessionResult = lockdownd_start_session( lockdown, "klogg", &sessionId, &sslEnabled );
    LOG_INFO << "lockdownd_start_session for " << udid << ": result=" << sessionResult;

    info.isPaired = ( sessionResult == LOCKDOWN_E_SUCCESS );

    if ( info.isPaired && sessionId ) {
        // Get device name
        plist_t nameNode = nullptr;
        if ( lockdownd_get_value( lockdown, nullptr, "DeviceName", &nameNode )
             == LOCKDOWN_E_SUCCESS
             && nameNode != nullptr ) {
            char* nameStr = nullptr;
            plist_get_string_val( nameNode, &nameStr );
            if ( nameStr ) {
                info.name = QString::fromUtf8( nameStr );
                free( nameStr );
            }
            plist_free( nameNode );
        }

        // Get product model
        plist_t modelNode = nullptr;
        if ( lockdownd_get_value( lockdown, nullptr, "ProductModel", &modelNode )
             == LOCKDOWN_E_SUCCESS
             && modelNode != nullptr ) {
            char* modelStr = nullptr;
            plist_get_string_val( modelNode, &modelStr );
            if ( modelStr ) {
                info.model = QString::fromUtf8( modelStr );
                free( modelStr );
            }
            plist_free( modelNode );
        }

        // Get product version (iOS version)
        plist_t versionNode = nullptr;
        if ( lockdownd_get_value( lockdown, nullptr, "ProductVersion", &versionNode )
             == LOCKDOWN_E_SUCCESS
             && versionNode != nullptr ) {
            char* versionStr = nullptr;
            plist_get_string_val( versionNode, &versionStr );
            if ( versionStr ) {
                info.productVersion = QString::fromUtf8( versionStr );
                free( versionStr );
            }
            plist_free( versionNode );
        }

        lockdownd_stop_session( lockdown, sessionId );
        free( sessionId );
    }

    if ( info.name.isEmpty() ) {
        info.name = udid.left( 16 );
    }

    lockdownd_client_free( lockdown );
    idevice_free( device );

    return info;
}

} // anonymous namespace

IosDeviceManager::IosDeviceManager( QObject* parent )
    : QObject( parent )
{
}

IosDeviceManager::~IosDeviceManager()
{
    stopMonitoring();
}

QVector<IosDeviceManager::DeviceInfo> IosDeviceManager::availableDevices() const
{
    return devices_.values().toVector();
}

void IosDeviceManager::startMonitoring()
{
    if ( subscription_ ) {
        return;
    }

    auto result = idevice_events_subscribe( &subscription_, deviceEventCallback, this );
    if ( result != IDEVICE_E_SUCCESS ) {
        LOG_ERROR << "Failed to subscribe to device events: " << result;
        return;
    }

    LOG_INFO << "Started monitoring iOS devices";
    refreshDevices();
}

void IosDeviceManager::stopMonitoring()
{
    if ( subscription_ ) {
        idevice_events_unsubscribe( subscription_ );
        subscription_ = nullptr;
        LOG_INFO << "Stopped monitoring iOS devices";
    }
}

void IosDeviceManager::refreshDevices()
{
    int count = 0;
    char** deviceList = nullptr;

    auto result = idevice_get_device_list( &deviceList, &count );
    if ( result != IDEVICE_E_SUCCESS ) {
        LOG_ERROR << "Failed to get device list: " << result;
        return;
    }

    QStringList currentUdids;
    for ( int i = 0; i < count; ++i ) {
        currentUdids << QString::fromUtf8( deviceList[ i ] );
    }

    idevice_device_list_free( deviceList );

    QStringList keys = devices_.keys();
    for ( const auto& udid : keys ) {
        if ( !currentUdids.contains( udid ) ) {
            onDeviceRemoved( udid );
        }
    }

    // Query device info asynchronously to avoid blocking the GUI thread.
    // Each query can take 5-10 seconds (lockdown handshake), so we run
    // them on the thread pool and emit deviceAdded when done.
    for ( const auto& udid : currentUdids ) {
        if ( !devices_.contains( udid ) ) {
            queryDeviceInfoAsync( udid );
        }
    }
}

void IosDeviceManager::deviceEventCallback( const idevice_event_t* event, void* user_data )
{
    if ( !event || !user_data ) {
        return;
    }

    auto* self = static_cast<IosDeviceManager*>( user_data );
    QString udid = QString::fromUtf8( event->udid );

    switch ( event->event ) {
    case IDEVICE_DEVICE_ADD:
        QMetaObject::invokeMethod( self, [ self, udid ]() { self->onDeviceAdded( udid ); },
                                    Qt::QueuedConnection );
        break;
    case IDEVICE_DEVICE_REMOVE:
        QMetaObject::invokeMethod( self, [ self, udid ]() { self->onDeviceRemoved( udid ); },
                                    Qt::QueuedConnection );
        break;
    case IDEVICE_DEVICE_PAIRED:
        QMetaObject::invokeMethod( self, [ self, udid ]() { self->onDeviceAdded( udid ); },
                                    Qt::QueuedConnection );
        break;
    }
}

void IosDeviceManager::onDeviceAdded( const QString& udid )
{
    // Query device info asynchronously — lockdown calls block for seconds
    queryDeviceInfoAsync( udid );
}

void IosDeviceManager::onDeviceRemoved( const QString& udid )
{
    if ( devices_.contains( udid ) ) {
        LOG_INFO << "iOS device removed: " << devices_[ udid ].name << " (" << udid << ")";
        devices_.remove( udid );
        Q_EMIT deviceRemoved( udid );
    }
}

void IosDeviceManager::queryDeviceInfoAsync( const QString& udid )
{
    // Run the slow lockdown query off the GUI thread
    QPointer<IosDeviceManager> self( this );
    auto future = QtConcurrent::run( [ self, udid ]() {
        auto info = queryDeviceInfoOffThread( udid );
        if ( info.udid.isEmpty() ) {
            return;
        }
        // Marshal the result back to the GUI thread
        QMetaObject::invokeMethod(
            self, [ self, info ]() {
                if ( !self ) {
                    return;
                }
                bool isNew = !self->devices_.contains( info.udid );
                self->devices_[ info.udid ] = info;
                if ( isNew ) {
                    LOG_INFO << "iOS device added: " << info.name << " (" << info.udid << ")";
                }
                Q_EMIT self->deviceAdded( info );
            },
            Qt::QueuedConnection );
    } );
}

#endif // KLOGG_WITH_IMOBILEDEVICE