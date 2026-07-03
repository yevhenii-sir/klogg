#include "iosnativelogtransport.h"

#ifdef KLOGG_WITH_IMOBILEDEVICE

#include "log.h"

#include <QTimer>

IosNativeLogTransport::IosNativeLogTransport( QString deviceUdid, QObject* parent )
    : LiveSourceTransport( parent )
    , deviceUdid_( std::move( deviceUdid ) )
    , receiver_( std::make_unique<IosSyslogReceiver>( this ) )
{
    connect( receiver_.get(), &IosSyslogReceiver::rawDataReceived, this,
             [ this ]( const QByteArray& data ) {
                 if ( !data.isEmpty() ) {
                     Q_EMIT bytesReceived( data );
                 }
             } );

    connect( receiver_.get(), &IosSyslogReceiver::connectionLost, this, [ this ]() {
        if ( !disconnectRequested_ ) {
            lastError_ = tr( "iOS syslog connection lost" );
            setState( State::Error );
            Q_EMIT errorOccurred( lastError_ );
        }
    } );

    connect( receiver_.get(), &IosSyslogReceiver::error, this,
             [ this ]( const QString& error ) {
                 if ( !disconnectRequested_ && !error.isEmpty() ) {
                     lastError_ = error;
                     setState( State::Error );
                     Q_EMIT errorOccurred( lastError_ );
                 }
             } );
}

IosNativeLogTransport::~IosNativeLogTransport()
{
    disconnectRequested_ = true;
    if ( receiver_ ) {
        receiver_->disconnectFromDevice();
    }
}

bool IosNativeLogTransport::connectTransport()
{
    if ( receiver_->isConnected() ) {
        setState( State::Connected );
        return true;
    }

    disconnectRequested_ = false;
    lastError_.clear();

    if ( !receiver_->connectToDevice( deviceUdid_ ) ) {
        lastError_ = receiver_->isConnected() ? QString() : tr( "Failed to connect to iOS device syslog" );
        if ( lastError_.isEmpty() ) {
            // connectToDevice emitted the error signal with a specific message
            lastError_ = tr( "Failed to connect to iOS device syslog" );
        }
        setState( State::Error );
        return false;
    }

    setState( State::Connected );
    return true;
}

void IosNativeLogTransport::connectTransportAsync()
{
    // For the native transport, connectTransport() is not blocking
    // (libimobiledevice calls are fast), so we can call it directly.
    // But we still use a small delay to keep the API consistent.
    QTimer::singleShot( 0, this, [ this ]() {
        if ( disconnectRequested_ ) {
            return;
        }
        if ( !connectTransport() ) {
            // Error already set in connectTransport()
        }
    } );
}

void IosNativeLogTransport::disconnectTransport()
{
    disconnectRequested_ = true;
    if ( receiver_ ) {
        receiver_->disconnectFromDevice();
    }
    setState( State::Disconnected );
}

bool IosNativeLogTransport::clearRemote( QString* error )
{
    // iOS syslog cannot be cleared remotely
    if ( error ) {
        *error = tr( "iOS live logs cannot be cleared remotely." );
    }
    return false;
}

QString IosNativeLogTransport::lastError() const
{
    return lastError_;
}

void IosNativeLogTransport::setState( State state )
{
    if ( state_ == state ) {
        return;
    }
    state_ = state;
    Q_EMIT stateChanged( state_ );
}

#endif // KLOGG_WITH_IMOBILEDEVICE