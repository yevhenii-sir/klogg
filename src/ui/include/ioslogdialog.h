#ifndef IOSLOGDIALOG_H
#define IOSLOGDIALOG_H

#include <QDialog>
#include <QFutureWatcher>

#include "adblogcatsource.h"
#include "iosdeviceparser.h"

class QComboBox;
class QDialogButtonBox;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTreeWidget;

#ifdef KLOGG_WITH_IMOBILEDEVICE
class IosDeviceManager;
#endif

class IosLogDialog : public QDialog {
    Q_OBJECT

  public:
    explicit IosLogDialog( QWidget* parent = nullptr );

    AdbLogcatSessionData sessionData() const;

  private Q_SLOTS:
    void refreshDevices();
    void onDevicesEnumerated();

#ifdef KLOGG_WITH_IMOBILEDEVICE
    void onDeviceAdded( const QString& udid, const QString& name );
    void onDeviceRemoved( const QString& udid );
#endif

  private:
    void updateAcceptState();
    void loadSettings();
    void saveSettings() const;
    void populateDeviceCombo( const QList<IosDeviceInfo>& devices );

    // pymobiledevice3-based device enumeration (async)
    QFutureWatcher<QList<IosDeviceInfo>>* deviceRefreshWatcher_ = nullptr;

#ifdef KLOGG_WITH_IMOBILEDEVICE
    // Native device monitoring (hot-plug via libimobiledevice)
    IosDeviceManager* nativeDeviceManager_ = nullptr;
    QTreeWidget* nativeDeviceList_ = nullptr;
#endif

    QLineEdit* executableEdit_ = nullptr;
    QPushButton* refreshButton_ = nullptr;
    QComboBox* deviceCombo_ = nullptr;
    QLineEdit* extraArgsEdit_ = nullptr;
    QCheckBox* ansiOutputCheckBox_ = nullptr;
    QCheckBox* autoReconnectCheckBox_ = nullptr;
    QSpinBox* maxAttemptsSpinBox_ = nullptr;
    QSpinBox* maxFileSizeSpinBox_ = nullptr;
    QSpinBox* backupCountSpinBox_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QDialogButtonBox* buttonBox_ = nullptr;
};

#endif