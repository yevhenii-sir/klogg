#ifndef IOSNATIVELOGTRANSPORT_H
#define IOSNATIVELOGTRANSPORT_H

#ifdef KLOGG_WITH_IMOBILEDEVICE

#include <memory>
#include <QString>

#include "ios_syslog_receiver.h"
#include "livesourcetransport.h"

class IosNativeLogTransport : public LiveSourceTransport {
    Q_OBJECT

  public:
    IosNativeLogTransport( QString deviceUdid, QObject* parent = nullptr );
    ~IosNativeLogTransport() override;

    bool connectTransport() override;
    void connectTransportAsync() override;
    void disconnectTransport() override;
    bool clearRemote( QString* error ) override;
    QString lastError() const override;

  private:
    void setState( State state );

    QString deviceUdid_;
    QString lastError_;
    std::unique_ptr<IosSyslogReceiver> receiver_;
    bool disconnectRequested_ = false;
    State state_{ State::Disconnected };
};

#endif // KLOGG_WITH_IMOBILEDEVICE

#endif // IOSNATIVELOGTRANSPORT_H