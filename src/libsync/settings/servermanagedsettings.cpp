/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "settings/servermanagedsettings.h"

#include <QHash>

namespace OCC {

namespace {
// Keys the client accepts from the server, and whether the server may enforce
// (lock) each. Tightening this map is how server delivered settings are
// restricted. Security note: update and proxy keys are server lockable by
// request, so a trusted server can enforce them; device policy still wins.
struct ServerKeyPolicy {
    bool serverLockable = false;
};

const QHash<QString, ServerKeyPolicy> &acceptedServerKeys()
{
    static const QHash<QString, ServerKeyPolicy> keys = {
        {QStringLiteral("skipUpdateCheck"), {true}},
        {QStringLiteral("autoUpdateCheck"), {true}},
        {QStringLiteral("virtualFilesMode"), {true}},
        {QStringLiteral("proxyHost"), {true}},
        {QStringLiteral("proxyPort"), {true}},
        {QStringLiteral("proxyType"), {true}},
        {QStringLiteral("newBigFolderSizeLimit"), {true}},
        {QStringLiteral("confirmExternalStorage"), {true}},
        {QStringLiteral("useNewBigFolderSizeLimit"), {true}},
        {QStringLiteral("notifyExistingFoldersOverLimit"), {true}},
        {QStringLiteral("stopSyncingExistingFoldersOverLimit"), {true}},
    };
    return keys;
}
}

ServerManagedSettings parseServerManagedSettings(const QVariantMap &desktopClientCapability)
{
    ServerManagedSettings parsed;
    parsed.schemaVersion = desktopClientCapability.value(QStringLiteral("schemaVersion")).toInt();
    parsed.defaults = desktopClientCapability.value(QStringLiteral("defaults")).toMap();
    parsed.locked = desktopClientCapability.value(QStringLiteral("locked")).toMap();
    return parsed;
}

ServerManagedSettings sanitizeServerManagedSettings(const ServerManagedSettings &raw)
{
    ServerManagedSettings clean;
    clean.schemaVersion = raw.schemaVersion;

    const auto &accepted = acceptedServerKeys();
    for (const auto &[key, value] : raw.defaults.asKeyValueRange()) {
        if (accepted.contains(key)) {
            clean.defaults.insert(key, value);
        }
    }
    for (const auto &[key, value] : raw.locked.asKeyValueRange()) {
        const auto policy = accepted.constFind(key);
        if (policy != accepted.cend() && policy->serverLockable) {
            clean.locked.insert(key, value);
        }
    }
    return clean;
}

ServerSettingsSource::ServerSettingsSource(QVariantMap values, SettingSourceKind kind, LockState lockState, int priority)
    : _values(std::move(values))
    , _kind(kind)
    , _lockState(lockState)
    , _priority(priority)
{
}

std::optional<QVariant> ServerSettingsSource::read(const QString &key, const QString &) const
{
    if (!_values.contains(key)) {
        return std::nullopt;
    }
    return _values.value(key);
}

SettingSourceKind ServerSettingsSource::kind() const
{
    return _kind;
}

LockState ServerSettingsSource::lockState() const
{
    return _lockState;
}

int ServerSettingsSource::priority() const
{
    return _priority;
}

std::vector<std::unique_ptr<SettingSource>> buildServerSources(const ServerManagedSettings &sanitized)
{
    std::vector<std::unique_ptr<SettingSource>> sources;
    if (!sanitized.locked.isEmpty()) {
        sources.push_back(std::make_unique<ServerSettingsSource>(
            sanitized.locked, SettingSourceKind::ServerLocked, LockState::Locked, 100));
    }
    if (!sanitized.defaults.isEmpty()) {
        sources.push_back(std::make_unique<ServerSettingsSource>(
            sanitized.defaults, SettingSourceKind::ServerDefault, LockState::Unlocked, 30));
    }
    return sources;
}

} // namespace OCC
