#pragma once

#include <optional>
#include <vector>

#include <QByteArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLatin1StringView>
#include <QString>


namespace UDI
{
class ManagerConfigValue;

class ManagerConfigMap
{
public:
    explicit ManagerConfigMap(QJsonObject object);

    ManagerConfigValue operator[](QLatin1StringView key) const;

private:
    QJsonObject m_object;
};

// Every getter reports a missing key or a type mismatch as false (and logs which key failed), which is
// what lets Manager's constructor refuse to start the app on a broken config instead of running with
// half-loaded values.
class ManagerConfigValue
{
public:
    ManagerConfigValue(QJsonValue value, QLatin1StringView key);

    bool get(bool& output) const;
    bool get(int& output) const;
    bool get(unsigned& output) const;
    bool get(qint64& output) const;
    bool get(quint64& output) const;
    bool get(double& output) const;
    bool get(QString& output) const;
    bool get(QByteArray& output) const;
    bool get(std::vector<QString>& output) const;

    template <typename TNestedConfig>
    bool getNested(TNestedConfig& output) const
    {
        const auto nested = asMap();

        return nested && output.parse(*nested);
    }

private:
    std::optional<ManagerConfigMap> asMap() const;
    void reportMismatch(QLatin1StringView expectedType) const;

    QJsonValue m_value;
    QLatin1StringView m_key;
};

// Looks for configs/<fileName> next to the executable first so a deployed build can be retuned without a
// rebuild, and falls back to the copy baked into the binary's resources.
std::optional<ManagerConfigMap> loadManagerConfigFile(const QString& fileName);
} // namespace UDI
