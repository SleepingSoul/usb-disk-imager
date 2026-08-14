#include <app/managerconfig.hpp>

#include <limits>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>


namespace UDI
{
using namespace Qt::Literals::StringLiterals;

namespace
{
const QLatin1StringView ConfigsDirectoryName{ "configs" };
const QLatin1StringView BundledConfigsPrefix{ ":/configs/" };
} // namespace

ManagerConfigValue::ManagerConfigValue(QJsonValue value, QLatin1StringView key)
    : m_value(std::move(value))
    , m_key(key)
{}

bool ManagerConfigValue::get(bool& output) const
{
    if (!m_value.isBool())
    {
        reportMismatch("bool"_L1);
        return false;
    }

    output = m_value.toBool();

    return true;
}

bool ManagerConfigValue::get(int& output) const
{
    if (!m_value.isDouble())
    {
        reportMismatch("int"_L1);
        return false;
    }

    output = m_value.toInt();

    return true;
}

bool ManagerConfigValue::get(unsigned& output) const
{
    qint64 wide = 0;
    if (!get(wide))
    {
        return false;
    }

    if (wide < 0 || wide > static_cast<qint64>(std::numeric_limits<unsigned>::max()))
    {
        reportMismatch("unsigned"_L1);
        return false;
    }

    output = static_cast<unsigned>(wide);

    return true;
}

bool ManagerConfigValue::get(qint64& output) const
{
    if (!m_value.isDouble())
    {
        reportMismatch("integer"_L1);
        return false;
    }

    output = m_value.toInteger();

    return true;
}

bool ManagerConfigValue::get(quint64& output) const
{
    qint64 signedValue = 0;
    if (!get(signedValue))
    {
        return false;
    }

    if (signedValue < 0)
    {
        reportMismatch("unsigned integer"_L1);
        return false;
    }

    output = static_cast<quint64>(signedValue);

    return true;
}

bool ManagerConfigValue::get(double& output) const
{
    if (!m_value.isDouble())
    {
        reportMismatch("double"_L1);
        return false;
    }

    output = m_value.toDouble();

    return true;
}

bool ManagerConfigValue::get(QString& output) const
{
    if (!m_value.isString())
    {
        reportMismatch("string"_L1);
        return false;
    }

    output = m_value.toString();

    return true;
}

bool ManagerConfigValue::get(QByteArray& output) const
{
    QString text;
    if (!get(text))
    {
        return false;
    }

    output = text.toUtf8();

    return true;
}

bool ManagerConfigValue::get(std::vector<QString>& output) const
{
    if (!m_value.isArray())
    {
        reportMismatch("array of strings"_L1);
        return false;
    }

    const QJsonArray array = m_value.toArray();

    output.clear();
    output.reserve(static_cast<std::size_t>(array.size()));

    for (const QJsonValue& element : array)
    {
        if (!element.isString())
        {
            reportMismatch("array of strings"_L1);
            return false;
        }

        output.push_back(element.toString());
    }

    return true;
}

std::optional<ManagerConfigMap> ManagerConfigValue::asMap() const
{
    if (!m_value.isObject())
    {
        reportMismatch("object"_L1);
        return std::nullopt;
    }

    return ManagerConfigMap{ m_value.toObject() };
}

void ManagerConfigValue::reportMismatch(QLatin1StringView expectedType) const
{
    if (m_value.isUndefined())
    {
        qCritical() << "Config key" << m_key << "is missing; expected" << expectedType;
    }
    else
    {
        qCritical() << "Config key" << m_key << "has the wrong type; expected" << expectedType;
    }
}

ManagerConfigMap::ManagerConfigMap(QJsonObject object)
    : m_object(std::move(object))
{}

ManagerConfigValue ManagerConfigMap::operator[](QLatin1StringView key) const
{
    return ManagerConfigValue{ m_object.value(key), key };
}

std::optional<ManagerConfigMap> loadManagerConfigFile(const QString& fileName)
{
    const QString externalPath = QDir{ QCoreApplication::applicationDirPath() }
        .filePath(QDir{ ConfigsDirectoryName }.filePath(fileName));
    const QString bundledPath = BundledConfigsPrefix + fileName;

    const QString path = QFile::exists(externalPath) ? externalPath : bundledPath;

    QFile file{ path };
    if (!file.open(QIODevice::ReadOnly))
    {
        qCritical() << "Could not open config file" << path << ":" << file.errorString();
        return std::nullopt;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        qCritical() << "Config file" << path << "is not valid JSON:" << parseError.errorString();
        return std::nullopt;
    }

    if (!document.isObject())
    {
        qCritical() << "Config file" << path << "must contain a JSON object";
        return std::nullopt;
    }

    return ManagerConfigMap{ document.object() };
}
} // namespace UDI
