#include "ioslogdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QHeaderView>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUuid>
#include <QtConcurrent>

#include <memory>

#include "configuration.h"
#include "iosdevicelistprovider.h"
#include "ioslogprocesstransport.h"

#ifdef KLOGG_WITH_IMOBILEDEVICE
#include "ios_device_manager.h"
#endif

IosLogDialog::IosLogDialog( QWidget* parent )
    : QDialog( parent )
{
    setWindowTitle( tr( "Open iOS Log Stream" ) );
    setModal( true );
    resize( 720, 380 );

    auto* rootLayout = new QVBoxLayout( this );
    auto* formLayout = new QFormLayout();

#ifdef KLOGG_WITH_IMOBILEDEVICE
    // When libimobiledevice is available, use native device detection.
    // The executable field is hidden — native transport doesn't need pymobiledevice3.
    nativeDeviceManager_ = new IosDeviceManager( this );
    connect( nativeDeviceManager_, &IosDeviceManager::deviceAdded, this,
             [ this ]( const IosDeviceManager::DeviceInfo& info ) {
                 onDeviceAdded( info.udid, info.name );
             } );
    connect( nativeDeviceManager_, &IosDeviceManager::deviceRemoved, this,
             &IosLogDialog::onDeviceRemoved );
    nativeDeviceManager_->startMonitoring();

    nativeDeviceList_ = new QTreeWidget( this );
    nativeDeviceList_->setObjectName( QStringLiteral( "nativeDeviceList" ) );
    nativeDeviceList_->setHeaderLabels( { tr( "Device" ), tr( "Model" ), tr( "iOS" ), tr( "UDID" ),
                                         tr( "Status" ) } );
    nativeDeviceList_->setRootIsDecorated( false );
    nativeDeviceList_->setAlternatingRowColors( true );
    nativeDeviceList_->header()->setStretchLastSection( true );
    connect( nativeDeviceList_, &QTreeWidget::itemSelectionChanged, this,
             &IosLogDialog::updateAcceptState );
    formLayout->addRow( tr( "iOS Devices" ), nativeDeviceList_ );
#else
    // Fallback to pymobiledevice3 CLI
    auto* executableRowLayout = new QHBoxLayout();
    executableEdit_ = new QLineEdit( this );
    executableEdit_->setObjectName( QStringLiteral( "iosLogExecutableEdit" ) );
    executableEdit_->setPlaceholderText( QStringLiteral( "pymobiledevice3" ) );
    refreshButton_ = new QPushButton( tr( "Refresh Devices" ), this );
    refreshButton_->setObjectName( QStringLiteral( "refreshDevicesButton" ) );
    executableRowLayout->addWidget( executableEdit_ );
    executableRowLayout->addWidget( refreshButton_ );

    deviceCombo_ = new QComboBox( this );
    deviceCombo_->setObjectName( QStringLiteral( "deviceCombo" ) );
    deviceCombo_->setSizeAdjustPolicy( QComboBox::AdjustToContents );
    formLayout->addRow( tr( "pymobiledevice3 executable" ), executableRowLayout );
    formLayout->addRow( tr( "Device" ), deviceCombo_ );
#endif

    extraArgsEdit_ = new QLineEdit( this );
    extraArgsEdit_->setObjectName( QStringLiteral( "extraArgsEdit" ) );
    ansiOutputCheckBox_ = new QCheckBox( tr( "Enable ANSI color output" ), this );
    ansiOutputCheckBox_->setObjectName( QStringLiteral( "ansiOutputCheckBox" ) );

    // Auto-reconnect: enables automatic reconnection with exponential backoff
    autoReconnectCheckBox_
        = new QCheckBox( tr( "Enable auto-reconnect on connection loss" ), this );
    autoReconnectCheckBox_->setObjectName( QStringLiteral( "autoReconnectCheckBox" ) );
    autoReconnectCheckBox_->setToolTip(
        tr( "When enabled, klogg automatically attempts to reconnect to the live source "
            "after an unexpected disconnection or error. Uses exponential backoff "
            "starting at 1 second and capping at 30 seconds between attempts." ) );

    // Max reconnect attempts
    maxAttemptsSpinBox_ = new QSpinBox( this );
    maxAttemptsSpinBox_->setObjectName( QStringLiteral( "maxAttemptsSpinBox" ) );
    maxAttemptsSpinBox_->setRange( 0, 9999 );
    maxAttemptsSpinBox_->setSpecialValueText( tr( "Unlimited" ) );
    maxAttemptsSpinBox_->setToolTip(
        tr( "Maximum number of automatic reconnection attempts before giving up. "
            "Set to 0 for unlimited retries. Each retry uses increasing delay "
            "(1s, 2s, 4s, 8s, ... up to 30s)." ) );

    // Max capture file size (displayed in MB, stored in bytes)
    maxFileSizeSpinBox_ = new QSpinBox( this );
    maxFileSizeSpinBox_->setObjectName( QStringLiteral( "maxFileSizeSpinBox" ) );
    maxFileSizeSpinBox_->setRange( 0, 1048576 ); // 0 to ~1 TB in MB
    maxFileSizeSpinBox_->setSpecialValueText( tr( "Unlimited" ) );
    maxFileSizeSpinBox_->setSuffix( tr( " MB" ) );
    maxFileSizeSpinBox_->setToolTip(
        tr( "Maximum size of each rolling capture file in megabytes. "
            "When exceeded, the file is rotated and a new one is started. "
            "Set to 0 to disable size-based rolling." ) );

    // Rolling backup count
    backupCountSpinBox_ = new QSpinBox( this );
    backupCountSpinBox_->setObjectName( QStringLiteral( "backupCountSpinBox" ) );
    backupCountSpinBox_->setRange( 0, 999 );
    backupCountSpinBox_->setToolTip(
        tr( "Number of old capture files to keep when rolling by file size. "
            "Older files beyond this count are deleted. "
            "Set to 0 to keep all rotated files indefinitely." ) );

#ifndef KLOGG_WITH_IMOBILEDEVICE
    formLayout->addRow( tr( "Extra iOS log args" ), extraArgsEdit_ );
    formLayout->addRow( QString{}, ansiOutputCheckBox_ );
#endif
    formLayout->addRow( QString{}, autoReconnectCheckBox_ );
    formLayout->addRow( tr( "Max reconnect attempts" ), maxAttemptsSpinBox_ );
    formLayout->addRow( tr( "Max capture file size" ), maxFileSizeSpinBox_ );
    formLayout->addRow( tr( "Rolling backup count" ), backupCountSpinBox_ );

    statusLabel_ = new QLabel( this );
    statusLabel_->setObjectName( QStringLiteral( "iosLogStatusLabel" ) );
    statusLabel_->setWordWrap( true );

    buttonBox_ = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this );
    buttonBox_->setObjectName( QStringLiteral( "buttonBox" ) );

    rootLayout->addLayout( formLayout );
    rootLayout->addWidget( statusLabel_ );
    rootLayout->addWidget( buttonBox_ );

#ifndef KLOGG_WITH_IMOBILEDEVICE
    connect( refreshButton_, &QPushButton::clicked, this, &IosLogDialog::refreshDevices );
    connect( deviceCombo_, qOverload<int>( &QComboBox::currentIndexChanged ), this,
             &IosLogDialog::updateAcceptState );
#endif
    connect( buttonBox_, &QDialogButtonBox::accepted, this, [ this ] {
        saveSettings();
        accept();
    } );
    connect( buttonBox_, &QDialogButtonBox::rejected, this, &QDialog::reject );

#ifndef KLOGG_WITH_IMOBILEDEVICE
    deviceRefreshWatcher_ = new QFutureWatcher<QList<IosDeviceInfo>>( this );
    connect( deviceRefreshWatcher_, &QFutureWatcher<QList<IosDeviceInfo>>::finished, this,
             &IosLogDialog::onDevicesEnumerated );
#endif

    loadSettings();

#ifdef KLOGG_WITH_IMOBILEDEVICE
    // Trigger initial device refresh
    QTimer::singleShot( 0, this, [ this ]() { nativeDeviceManager_->refreshDevices(); } );
#else
    QTimer::singleShot( 0, this, &IosLogDialog::refreshDevices );
#endif

    updateAcceptState();
}

AdbLogcatSessionData IosLogDialog::sessionData() const
{
    AdbLogcatSessionData sessionData;

#ifdef KLOGG_WITH_IMOBILEDEVICE
    // For native transport, adbExecutable is empty → makeTransport() will
    // create an IosNativeLogTransport instead of IosLogProcessTransport.
    sessionData.adbExecutable.clear();

    // Get selected device from the tree widget
    auto selected = nativeDeviceList_ ? nativeDeviceList_->selectedItems() : QList<QTreeWidgetItem*>();
    if ( !selected.isEmpty() ) {
        sessionData.deviceSerial = selected.first()->data( 0, Qt::UserRole ).toString();
        sessionData.deviceDescription = selected.first()->text( 0 );
    }
#else
    sessionData.adbExecutable = executableEdit_->text().trimmed();
    sessionData.deviceSerial = deviceCombo_->currentData( Qt::UserRole ).toString();
    sessionData.deviceDescription = deviceCombo_->currentText();
#endif

    sessionData.extraArgs = extraArgsEdit_->text().trimmed();
    sessionData.captureId = QUuid::createUuid().toString( QUuid::WithoutBraces );
    sessionData.sourceType = LiveLogSourceType::IosLogStream;
    sessionData.ansiOutputEnabled = ansiOutputCheckBox_ ? ansiOutputCheckBox_->isChecked() : false;
    sessionData.autoReconnectEnabled = autoReconnectCheckBox_->isChecked();
    sessionData.maxReconnectAttempts = maxAttemptsSpinBox_->value();
    sessionData.captureMaxFileSize = static_cast<qint64>( maxFileSizeSpinBox_->value() ) * 1024 * 1024;
    sessionData.captureBackupCount = backupCountSpinBox_->value();
    return sessionData;
}

void IosLogDialog::refreshDevices()
{
#ifndef KLOGG_WITH_IMOBILEDEVICE
    deviceCombo_->clear();
    updateAcceptState();
    statusLabel_->setText( tr( "Detecting iOS devices..." ) );

    auto provider = std::make_shared<IosDeviceListProvider>( executableEdit_->text() );
    deviceRefreshWatcher_->setFuture(
        QtConcurrent::run( [provider]() { return provider->listDevices(); } ) );
#endif
}

#ifdef KLOGG_WITH_IMOBILEDEVICE
void IosLogDialog::onDeviceAdded( const QString& udid, const QString& name )
{
    // Check if already in the tree
    for ( int i = 0; i < nativeDeviceList_->topLevelItemCount(); ++i ) {
        auto* item = nativeDeviceList_->topLevelItem( i );
        if ( item->data( 0, Qt::UserRole ).toString() == udid ) {
            // Update existing item
            item->setText( 0, name );
            item->setData( 0, Qt::UserRole, udid );
            return;
        }
    }

    // Add new item
    auto* item = new QTreeWidgetItem( { name, {}, {}, udid, tr( "Connected" ) } );
    item->setData( 0, Qt::UserRole, udid );
    nativeDeviceList_->addTopLevelItem( item );
    nativeDeviceList_->resizeColumnToContents( 0 );

    statusLabel_->setText(
        tr( "Data stays in temp capture storage until you explicitly save or close the tab." ) );
    updateAcceptState();
}

void IosLogDialog::onDeviceRemoved( const QString& udid )
{
    for ( int i = 0; i < nativeDeviceList_->topLevelItemCount(); ++i ) {
        auto* item = nativeDeviceList_->topLevelItem( i );
        if ( item->data( 0, Qt::UserRole ).toString() == udid ) {
            delete nativeDeviceList_->takeTopLevelItem( i );
            break;
        }
    }

    if ( nativeDeviceList_->topLevelItemCount() == 0 ) {
        statusLabel_->setText( tr( "No iOS devices detected." ) );
    }
    updateAcceptState();
}
#endif // KLOGG_WITH_IMOBILEDEVICE

void IosLogDialog::onDevicesEnumerated()
{
#ifndef KLOGG_WITH_IMOBILEDEVICE
    const auto devices = deviceRefreshWatcher_->result();
    populateDeviceCombo( devices );

    if ( devices.isEmpty() ) {
        statusLabel_->setText( tr( "No iOS devices detected." ) );
    }
    else {
        statusLabel_->setText(
            tr( "Data stays in temp capture storage until you explicitly save or close the tab." ) );
    }

    updateAcceptState();
#endif
}

void IosLogDialog::updateAcceptState()
{
    if ( auto* okButton = buttonBox_->button( QDialogButtonBox::Ok ) ) {
#ifdef KLOGG_WITH_IMOBILEDEVICE
        auto selected
            = nativeDeviceList_ ? nativeDeviceList_->selectedItems() : QList<QTreeWidgetItem*>();
        okButton->setEnabled( !selected.isEmpty() );
#else
        okButton->setEnabled( deviceCombo_->count() > 0 && deviceCombo_->currentIndex() >= 0 );
#endif
    }
}

void IosLogDialog::loadSettings()
{
    const auto& config = Configuration::get();
#ifndef KLOGG_WITH_IMOBILEDEVICE
    executableEdit_->setText( config.iosLogExecutable() );
    extraArgsEdit_->setText( config.iosLogExtraArgs() );
    ansiOutputCheckBox_->setChecked( config.iosLogAnsiOutputEnabled() );
#endif
    autoReconnectCheckBox_->setChecked( config.liveAutoReconnectEnabled() );
    maxAttemptsSpinBox_->setValue( config.liveAutoReconnectMaxAttempts() );
    maxFileSizeSpinBox_->setValue( config.liveCaptureRollingMaxFileSizeMb() );
    backupCountSpinBox_->setValue( config.liveCaptureRollingBackupCount() );
}

void IosLogDialog::saveSettings() const
{
    auto& config = Configuration::get();
#ifndef KLOGG_WITH_IMOBILEDEVICE
    config.setIosLogExecutable( executableEdit_->text().trimmed() );
    config.setIosLogExtraArgs( extraArgsEdit_->text().trimmed() );
    config.setIosLogAnsiOutputEnabled( ansiOutputCheckBox_->isChecked() );
#endif
    config.setLiveAutoReconnectEnabled( autoReconnectCheckBox_->isChecked() );
    config.setLiveAutoReconnectMaxAttempts( maxAttemptsSpinBox_->value() );
    config.setLiveCaptureRollingMaxFileSizeMb( maxFileSizeSpinBox_->value() );
    config.setLiveCaptureRollingBackupCount( backupCountSpinBox_->value() );
    config.save();
}

void IosLogDialog::populateDeviceCombo( const QList<IosDeviceInfo>& devices )
{
#ifndef KLOGG_WITH_IMOBILEDEVICE
    for ( const auto& device : devices ) {
        deviceCombo_->addItem( device.displayName, device.udid );
        deviceCombo_->setItemData( deviceCombo_->count() - 1, device.description, Qt::ToolTipRole );
    }
#else
    Q_UNUSED( devices );
#endif
}