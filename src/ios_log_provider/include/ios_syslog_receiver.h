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

#ifndef IOS_SYSLOG_RECEIVER_H
#define IOS_SYSLOG_RECEIVER_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <atomic>
#include <mutex>
#include <vector>

#ifdef KLOGG_WITH_IMOBILEDEVICE

#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/syslog_relay.h>

// Receives syslog data from an iOS device via libimobiledevice.
//
// The syslog_relay callback delivers one character at a time on a
// background thread.  Characters are buffered in a lock-free vector
// and periodically flushed on the Qt event loop.  Two signals are
// available:
//
//   rawDataReceived(QByteArray) – the original bytes (for streaming)
//   lineReceived(QString)        – newline-delimited lines (legacy)
//
// Callers that integrate with the LiveSourceTransport pipeline should
// use rawDataReceived; callers that write to a temp file can use
// lineReceived.

class IosSyslogReceiver : public QObject {
    Q_OBJECT

  public:
    explicit IosSyslogReceiver( QObject* parent = nullptr );
    ~IosSyslogReceiver();

    IosSyslogReceiver( const IosSyslogReceiver& ) = delete;
    IosSyslogReceiver& operator=( const IosSyslogReceiver& ) = delete;

    bool isConnected() const;

  public Q_SLOTS:
    bool connectToDevice( const QString& udid );
    void disconnectFromDevice();

  Q_SIGNALS:
    // Raw bytes from the syslog relay, suitable for StreamingLogData.
    void rawDataReceived( const QByteArray& data );
    // Parsed lines (legacy API, kept for backwards compatibility).
    void lineReceived( const QString& line );
    void connectionLost();
    void error( const QString& message );

  private Q_SLOTS:
    void processBuffer();
    void checkConnection();

  private:
    static void syslogCallback( char c, void* user_data );

    idevice_t device_ = nullptr;
    syslog_relay_client_t syslogClient_ = nullptr;

    std::mutex bufferMutex_;
    std::vector<char> rawBuffer_;
    QString lineBuffer_;

    QTimer* flushTimer_ = nullptr;
    QTimer* watchdogTimer_ = nullptr;
    std::atomic<bool> dataReceived_{ false };
    int idleCheckCount_ = 0;
    static constexpr int MaxIdleChecks = 10; // 10 * 3s = 30s without data = disconnected
};

#endif // KLOGG_WITH_IMOBILEDEVICE

#endif // IOS_SYSLOG_RECEIVER_H