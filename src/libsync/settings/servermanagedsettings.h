/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SERVERMANAGEDSETTINGS_H
#define SERVERMANAGEDSETTINGS_H

#include <QString>
#include <QVariantMap>

#include <memory>
#include <optional>
#include <vector>

#include "owncloudlib.h"
#include "settings/managedsettings.h"

namespace OCC {

/**
 * Server delivered managed settings: from the admin's config.php to a source the
 * resolver can read. See ManagedSettings for the full resolution flow.
 *
 * [server, support app]                     (separate repo, enterprise gated)
 *   config.php: desktopclient.defaults / .locked
 *     |
 *   DesktopClientSettingsService   allow-list, never secrets
 *     |
 *   Capabilities: support.desktopClient { schemaVersion, defaults, locked }
 *     |
 *   OCS  /cloud/capabilities
 *     |
 * [client]
 *   Account::setCapabilities
 *     |
 *   Capabilities::desktopClientManagedSettings
 *     |   parseServerManagedSettings   (capability map -> ServerManagedSettings)
 *     |
 *   sanitizeServerManagedSettings
 *     |   client allow-list: drop unknown keys,
 *     |   keep only server lockable keys in locked
 *     |
 *   AccountManager::updateServerManagedSettings
 *     |   merge subscribed accounts, the subscribed account wins
 *     |
 *   ConfigFile::setServerManagedSettings   (JSON in .cfg, offline cache)
 *     |
 *   buildServerSources
 *     |-- ServerSettingsSource  locked    (ServerLocked, priority 100)
 *     |-- ServerSettingsSource  defaults  (ServerDefault, priority 30)
 *     |
 *   [added to the resolver by ConfigFile::resolveManagedBool]
 */

// Managed settings delivered by the server through the support.desktopClient
// capability. defaults are suggestions, locked are enforced.
struct ServerManagedSettings {
    int schemaVersion = 0;
    QVariantMap defaults;
    QVariantMap locked;
};

// Parse the support.desktopClient capability submap into ServerManagedSettings.
[[nodiscard]] OWNCLOUDSYNC_EXPORT ServerManagedSettings parseServerManagedSettings(const QVariantMap &desktopClientCapability);

// Apply the client allow-list: keep only accepted keys in defaults, and only
// accepted server-lockable keys in locked. Single control point for what the
// server may deliver and enforce.
[[nodiscard]] OWNCLOUDSYNC_EXPORT ServerManagedSettings sanitizeServerManagedSettings(const ServerManagedSettings &raw);

// A setting source backed by an in-memory map, the sanitized server values.
class OWNCLOUDSYNC_EXPORT ServerSettingsSource : public SettingSource
{
public:
    ServerSettingsSource(QVariantMap values, SettingSourceKind kind, LockState lockState, int priority);

    [[nodiscard]] std::optional<QVariant> read(const QString &key, const QString &group) const override;
    [[nodiscard]] SettingSourceKind kind() const override;
    [[nodiscard]] LockState lockState() const override;
    [[nodiscard]] int priority() const override;

private:
    QVariantMap _values;
    SettingSourceKind _kind;
    LockState _lockState;
    int _priority;
};

// Build the server default (priority 30) and server locked (priority 100) sources
// from already sanitized settings. Empty maps produce no source.
[[nodiscard]] OWNCLOUDSYNC_EXPORT std::vector<std::unique_ptr<SettingSource>> buildServerSources(const ServerManagedSettings &sanitized);

} // namespace OCC

#endif // SERVERMANAGEDSETTINGS_H
