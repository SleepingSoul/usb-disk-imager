#pragma once

#include <QAbstractListModel>
#include <QStringList>

#include <disk/disktypes.hpp>


namespace UDI
{
// The device list as QML sees it, plus the current selection. Rows carry pre-formatted text so a delegate
// never has to compute anything while it scrolls.
class DeviceListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(DeviceListModel)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged FINAL)
    Q_PROPERTY(int selectedIndex READ getSelectedIndex WRITE setSelectedIndex NOTIFY selectionChanged FINAL)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionChanged FINAL)
    Q_PROPERTY(QString selectedName READ getSelectedName NOTIFY selectionChanged FINAL)
    Q_PROPERTY(QString selectedDetails READ getSelectedDetails NOTIFY selectionChanged FINAL)
    Q_PROPERTY(QString selectedPath READ getSelectedPath NOTIFY selectionChanged FINAL)
    Q_PROPERTY(QString selectedSizeText READ getSelectedSizeText NOTIFY selectionChanged FINAL)
    Q_PROPERTY(QString selectedMountPointsText READ getSelectedMountPointsText NOTIFY selectionChanged FINAL)
    Q_PROPERTY(bool selectedIsSystemDevice READ isSelectedSystemDevice NOTIFY selectionChanged FINAL)
    Q_PROPERTY(bool selectedIsWriteProtected READ isSelectedWriteProtected NOTIFY selectionChanged FINAL)
    Q_PROPERTY(bool includeFixedDisks READ getIncludeFixedDisks WRITE setIncludeFixedDisks NOTIFY includeFixedDisksChanged FINAL)
    Q_PROPERTY(QStringList warnings READ getWarnings NOTIFY warningsChanged FINAL)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        DetailsRole,
        PathRole,
        ShortIdentifierRole,
        SizeTextRole,
        BusNameRole,
        MountPointsTextRole,
        RemovableRole,
        SystemDeviceRole,
        WriteProtectedRole
    };

    explicit DeviceListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex{}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int getSelectedIndex() const { return m_selectedIndex; }
    void setSelectedIndex(int selectedIndex);
    bool hasSelection() const;

    QString getSelectedName() const;
    QString getSelectedDetails() const;
    QString getSelectedPath() const;
    QString getSelectedSizeText() const;
    QString getSelectedMountPointsText() const;
    bool isSelectedSystemDevice() const;
    bool isSelectedWriteProtected() const;

    bool getIncludeFixedDisks() const { return m_includeFixedDisks; }
    void setIncludeFixedDisks(bool includeFixedDisks);

    const QStringList& getWarnings() const { return m_warnings; }

    // nullptr when nothing is selected. Valid until the next device update.
    const DeviceInfo* getSelectedDevice() const;

    Q_INVOKABLE void refresh();

Q_SIGNALS:
    void countChanged();
    void selectionChanged();
    void includeFixedDisksChanged();
    void warningsChanged();

public Q_SLOTS:
    void onDevicesUpdated(const UDI::DeviceList& devices, const QStringList& warnings);

private:
    QString buildDetails(const DeviceInfo& device) const;
    QString buildMountPointsText(const DeviceInfo& device) const;
    // Keeps the user's choice across a refresh by matching the device path rather than the row number.
    void restoreSelection(const QByteArray& previouslySelectedPath);

    DeviceList m_devices;
    QStringList m_warnings;
    int m_selectedIndex{ -1 };
    bool m_includeFixedDisks{ false };
};
} // namespace UDI
