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

#include "ios_syslog_receiver.h"

#ifdef KLOGG_WITH_IMOBILEDEVICE

#include "log.h"

#include <QThread>

IosSyslogReceiver::IosSyslogReceiver( QObject* parent )
    : QObject( parent )
{
    // Timer to periodically flush buffered characters from the syslog callback
    flushTimer_ = new QTimer( this );
    flushTimer_->setInterval( 100 ); // 100ms flush interval
    connect( flushTimer_, &QTimer::timeout, this, &IosSyslogReceiver::processBuffer );

    // Watchdog: check if we're still receiving data
    // If no data comes for ~30s, consider the connection lost
    watchdogTimer_ = new QTimer( this );
    watchdogTimer_->setInterval( 3000 ); // 3s check interval
    connect( watchdogTimer_, &QTimer::timeout, this, &IosSyslogReceiver::checkConnection );
}

IosSyslogReceiver::~IosSyslogReceiver()
{
    disconnectFromDevice();
}

bool IosSyslogReceiver::isConnected() const
{
    return syslogClient_ != nullptr;
}

bool IosSyslogReceiver::connectToDevice( const QString& udid )
{
    if ( syslogClient_ ) {
        disconnectFromDevice();
    }

    LOG_INFO << "Connecting to iOS device syslog: " << udid.toStdString();

    idevice_t device = nullptr;
    auto idevResult
        = idevice_new_with_options( &device, udid.toUtf8().constData(),
                                    (idevice_options)( IDEVICE_LOOKUP_USBMUX ) );
    if ( idevResult != IDEVICE_E_SUCCESS ) {
        idevResult = idevice_new_with_options( &device, udid.toUtf8().constData(),
                                               (idevice_options)( IDEVICE_LOOKUP_NETWORK ) );
        if ( idevResult != IDEVICE_E_SUCCESS ) {
            Q_EMIT error( tr( "Failed to connect to device %1: error %2" )
                              .arg( udid )
                              .arg( idevResult ) );
            return false;
        }
    }

    device_ = device;

    syslog_relay_client_t client = nullptr;
    auto syslogResult = syslog_relay_client_start_service( device, &client, "klogg" );
    if ( syslogResult != SYSLOG_RELAY_E_SUCCESS ) {
        Q_EMIT error( tr( "Failed to start syslog service: error %1" ).arg( syslogResult ) );
        idevice_free( device_ );
        device_ = nullptr;
        return false;
    }

    syslogClient_ = client;

    syslogResult = syslog_relay_start_capture( syslogClient_, syslogCallback, this );
    if ( syslogResult != SYSLOG_RELAY_E_SUCCESS ) {
        Q_EMIT error( tr( "Failed to start syslog capture: error %1" ).arg( syslogResult ) );
        syslog_relay_client_free( syslogClient_ );
        syslogClient_ = nullptr;
        idevice_free( device_ );
        device_ = nullptr;
        return false;
    }

    flushTimer_->start();
    watchdogTimer_->start();
    dataReceived_ = false;
    idleCheckCount_ = 0;
    LOG_INFO << "Connected to iOS syslog for device: " << udid.toStdString();
    return true;
}

void IosSyslogReceiver::disconnectFromDevice()
{
    if ( flushTimer_ ) {
        flushTimer_->stop();
    }

    if ( watchdogTimer_ ) {
        watchdogTimer_->stop();
    }

    // Process any remaining data
    processBuffer();

    if ( syslogClient_ ) {
        syslog_relay_stop_capture( syslogClient_ );
        syslog_relay_client_free( syslogClient_ );
        syslogClient_ = nullptr;
    }

    if ( device_ ) {
        idevice_free( device_ );
        device_ = nullptr;
    }

    lineBuffer_.clear();
    LOG_INFO << "Disconnected from iOS syslog";
}

void IosSyslogReceiver::syslogCallback( char c, void* user_data )
{
    auto* self = static_cast<IosSyslogReceiver*>( user_data );
    if ( !self ) {
        return;
    }

    self->dataReceived_.store( true );

    std::lock_guard<std::mutex> lock( self->bufferMutex_ );
    self->rawBuffer_.push_back( c );
}

void IosSyslogReceiver::processBuffer()
{
    std::vector<char> localBuffer;
    {
        std::lock_guard<std::mutex> lock( bufferMutex_ );
        localBuffer.swap( rawBuffer_ );
    }

    if ( localBuffer.empty() ) {
        return;
    }

    // Emit raw bytes for streaming (LiveSourceTransport pipeline)
    Q_EMIT rawDataReceived( QByteArray( localBuffer.data(), static_cast<int>( localBuffer.size() ) ) );

    // Also emit parsed lines for legacy API compatibility
    for ( char c : localBuffer ) {
        if ( c == '\n' ) {
            if ( !lineBuffer_.isEmpty() ) {
                Q_EMIT lineReceived( lineBuffer_ );
                lineBuffer_.clear();
            }
        }
        else if ( c == '\0' ) {
            // Null characters are filtered by the non-raw capture mode
            continue;
        }
        else if ( c == '\r' ) {
            // Skip carriage return
            continue;
        }
        else {
            lineBuffer_ += QChar( c );
        }
    }

    // Flush incomplete line if it's getting long
    if ( lineBuffer_.length() > 4096 ) {
        Q_EMIT lineReceived( lineBuffer_ );
        lineBuffer_.clear();
    }
}

void IosSyslogReceiver::checkConnection()
{
    if ( !syslogClient_ ) {
        // Already disconnected
        return;
    }

    if ( dataReceived_.load() ) {
        // Data was received since last check — connection is alive
        dataReceived_ = false;
        idleCheckCount_ = 0;
        return;
    }

    // No data received for one check period
    idleCheckCount_++;

    // iOS devices can have silent periods, but 30s without any data
    // while connected is a strong sign the connection is dead
    if ( idleCheckCount_ >= MaxIdleChecks ) {
        LOG_WARNING << "iOS syslog: no data received for " << ( MaxIdleChecks * 3 )
                   << "s, assuming connection lost";

        watchdogTimer_->stop();
        flushTimer_->stop();

        if ( syslogClient_ ) {
            syslog_relay_stop_capture( syslogClient_ );
            syslog_relay_client_free( syslogClient_ );
            syslogClient_ = nullptr;
        }
        if ( device_ ) {
            idevice_free( device_ );
            device_ = nullptr;
        }

        Q_EMIT connectionLost();
    }
}

#endif // KLOGG_WITH_IMOBILEDEVICE