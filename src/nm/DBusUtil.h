#pragma once

#include <QDBusArgument>
#include <QVariant>

// Values read from D-Bus arrive as QDBusArgument; values built in-process (tests) are plain.
template<typename T>
T unwrap(const QVariant &v)
{
    if (v.canConvert<QDBusArgument>())
        return qdbus_cast<T>(v.value<QDBusArgument>());
    return v.value<T>();
}
