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

#ifndef IOS_DEVICE_MANAGER_H
#define IOS_DEVICE_MANAGER_H

#include <QObject>
#include <QMap>
#include <QVector>
#include <QString>

#ifdef KLOGG_WITH_IMOBILEDEVICE

#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>

class IosDeviceManager : public QObject {
    Q_OBJECT

  public:
    struct DeviceInfo {
        QString udid;
        QString name;
        QString model;
        QString productVersion;
        bool isPaired = false;
    };

    explicit IosDeviceManager( QObject* parent = nullptr );
    ~IosDeviceManager();

    IosDeviceManager( const IosDeviceManager& ) = delete;
    IosDeviceManager& operator=( const IosDeviceManager& ) = delete;

    QVector<DeviceInfo> availableDevices() const;

  public Q_SLOTS:
    void startMonitoring();
    void stopMonitoring();
    void refreshDevices();

  Q_SIGNALS:
    void deviceAdded( const IosDeviceManager::DeviceInfo& info );
    void deviceRemoved( const QString& udid );

  private:
    static void deviceEventCallback( const idevice_event_t* event, void* user_data );
    void onDeviceAdded( const QString& udid );
    void onDeviceRemoved( const QString& udid );
    // Queries device info on a background thread (lockdown calls block for seconds)
    void queryDeviceInfoAsync( const QString& udid );

    idevice_subscription_context_t subscription_ = nullptr;
    QMap<QString, DeviceInfo> devices_;
};

#endif // KLOGG_WITH_IMOBILEDEVICE

#endif // IOS_DEVICE_MANAGER_H