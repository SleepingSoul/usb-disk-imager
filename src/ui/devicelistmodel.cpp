#include <ui/devicelistmodel.hpp>

#include <algorithm>

#include <managers/devicemanager.hpp>
#include <utils/formatting.hpp>
#include <utils/usersettings.hpp>


namespace UDI
{
using namespace Qt::Literals::StringLiterals;

namespace
{
// Not a QLatin1StringView: the separator is a multi-byte UTF-8 character, which Latin-1 would render as
// two stray glyphs.
const QString DetailSeparator = QStringLiteral(" · ");
} // namespace

DeviceListModel::DeviceListModel(QObject* parent)
    : QAbstractListModel(parent)
    , m_includeFixedDisks(UserSettings::getIncludeFixedDisks())
{}

int DeviceListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_devices.size());
}

QVariant DeviceListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
    {
        return QVariant{};
    }

    const DeviceInfo& device = m_devices.at(static_cast<std::size_t>(index.row()));

    switch (role)
    {
    case NameRole:            return device.getProductName();
    case DetailsRole:         return buildDetails(device);
    case PathRole:            return QString::fromUtf8(device.path);
    case ShortIdentifierRole: return QString::fromUtf8(device.shortIdentifier);
    case SizeTextRole:        return formatByteSize(device.sizeBytes);
    case BusNameRole:         return QString{ busTypeName(device.bus) };
    case MountPointsTextRole: return buildMountPointsText(device);
    case RemovableRole:       return device.removableMedia;
    case SystemDeviceRole:    return device.systemDevice;
    case WriteProtectedRole:  return device.writeProtected;
    default:                  break;
    }

    return QVariant{};
}

QHash<int, QByteArray> DeviceListModel::roleNames() const
{
    return {
        { NameRole, QByteArrayLiteral("name") },
        { DetailsRole, QByteArrayLiteral("details") },
        { PathRole, QByteArrayLiteral("path") },
        { ShortIdentifierRole, QByteArrayLiteral("shortIdentifier") },
        { SizeTextRole, QByteArrayLiteral("sizeText") },
        { BusNameRole, QByteArrayLiteral("busName") },
        { MountPointsTextRole, QByteArrayLiteral("mountPointsText") },
        { RemovableRole, QByteArrayLiteral("removable") },
        { SystemDeviceRole, QByteArrayLiteral("systemDevice") },
        { WriteProtectedRole, QByteArrayLiteral("writeProtected") }
    };
}

void DeviceListModel::setSelectedIndex(int selectedIndex)
{
    const int clampedIndex = (selectedIndex >= 0 && selectedIndex < rowCount()) ? selectedIndex : -1;

    if (clampedIndex == m_selectedIndex)
    {
        return;
    }

    m_selectedIndex = clampedIndex;

    Q_EMIT selectionChanged();
}

bool DeviceListModel::hasSelection() const
{
    return getSelectedDevice() != nullptr;
}

const DeviceInfo* DeviceListModel::getSelectedDevice() const
{
    if (m_selectedIndex < 0 || m_selectedIndex >= rowCount())
    {
        return nullptr;
    }

    return &m_devices.at(static_cast<std::size_t>(m_selectedIndex));
}

QString DeviceListModel::getSelectedName() const
{
    const DeviceInfo* const device = getSelectedDevice();

    return device ? device->getProductName() : QString{};
}

QString DeviceListModel::getSelectedDetails() const
{
    const DeviceInfo* const device = getSelectedDevice();

    return device ? buildDetails(*device) : QString{};
}

QString DeviceListModel::getSelectedPath() const
{
    const DeviceInfo* const device = getSelectedDevice();

    return device ? QString::fromUtf8(device->path) : QString{};
}

QString DeviceListModel::getSelectedSizeText() const
{
    const DeviceInfo* const device = getSelectedDevice();

    return device ? formatByteSize(device->sizeBytes) : QString{};
}

QString DeviceListModel::getSelectedMountPointsText() const
{
    const DeviceInfo* const device = getSelectedDevice();

    return device ? buildMountPointsText(*device) : QString{};
}

bool DeviceListModel::isSelectedSystemDevice() const
{
    const DeviceInfo* const device = getSelectedDevice();

    return device && device->systemDevice;
}

bool DeviceListModel::isSelectedWriteProtected() const
{
    const DeviceInfo* const device = getSelectedDevice();

    return device && device->writeProtected;
}

void DeviceListModel::setIncludeFixedDisks(bool includeFixedDisks)
{
    if (m_includeFixedDisks == includeFixedDisks)
    {
        return;
    }

    m_includeFixedDisks = includeFixedDisks;
    UserSettings::setIncludeFixedDisks(includeFixedDisks);

    auto& deviceManager = getManager<DeviceManager>();
    deviceManager.setIncludeFixedDisks(includeFixedDisks);

    Q_EMIT includeFixedDisksChanged();

    refresh();
}

void DeviceListModel::refresh()
{
    QMetaObject::invokeMethod(&getManager<DeviceManager>(), &DeviceManager::refresh, Qt::QueuedConnection);
}

void DeviceListModel::onDevicesUpdated(const UDI::DeviceList& devices, const QStringList& warnings)
{
    const DeviceInfo* const previouslySelected = getSelectedDevice();
    const QByteArray previouslySelectedPath = previouslySelected ? previouslySelected->path : QByteArray{};

    beginResetModel();
    m_devices = devices;
    endResetModel();

    restoreSelection(previouslySelectedPath);

    if (m_warnings != warnings)
    {
        m_warnings = warnings;

        for (const QString& warning : m_warnings)
        {
            qWarning() << "Device enumeration:" << warning;
        }

        Q_EMIT warningsChanged();
    }

    Q_EMIT countChanged();
}

void DeviceListModel::restoreSelection(const QByteArray& previouslySelectedPath)
{
    const auto match = std::find_if(m_devices.cbegin(), m_devices.cend(),
        [&previouslySelectedPath](const DeviceInfo& device)
        {
            return device.path == previouslySelectedPath;
        });

    const int restoredIndex = match != m_devices.cend()
        ? static_cast<int>(std::distance(m_devices.cbegin(), match))
        // A single device is what the user meant anyway; anything else waits for a deliberate choice.
        : (m_devices.size() == 1 ? 0 : -1);

    m_selectedIndex = restoredIndex;

    Q_EMIT selectionChanged();
}

QString DeviceListModel::buildDetails(const DeviceInfo& device) const
{
    QStringList parts;
    parts.push_back(formatByteSize(device.sizeBytes));
    parts.push_back(QString{ busTypeName(device.bus) });

    const QString mountPoints = buildMountPointsText(device);

    if (!mountPoints.isEmpty())
    {
        parts.push_back(mountPoints);
    }

    return parts.join(DetailSeparator);
}

QString DeviceListModel::buildMountPointsText(const DeviceInfo& device) const
{
    QStringList entries;

    for (std::size_t index = 0; index < device.mountPoints.size(); ++index)
    {
        const QString& mountPoint = device.mountPoints.at(index);
        const QString label = index < device.volumeLabels.size() ? device.volumeLabels.at(index) : QString{};

        entries.push_back(label.isEmpty() ? mountPoint : QStringLiteral("%1 (%2)").arg(mountPoint, label));
    }

    return entries.join(", "_L1);
}
} // namespace UDI
