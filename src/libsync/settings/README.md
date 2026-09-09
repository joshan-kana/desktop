<!--
  - SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
  - SPDX-License-Identifier: GPL-2.0-or-later
-->

# Settings

This folder holds the client settings subsystems: managed settings, documented
below, and migration, documented in MIGRATION.md.

# Managed config: getConfig gateway design

## Context

Issue #5497 introduced a managed settings resolver so a setting can be resolved
across device enforced policy, server enforced policy, user config, server
defaults, device defaults and the builtin default. Update, proxy, folder limit and
virtual files keys now resolve through ConfigFile::getConfig. The remaining settings are still
read with ConfigFile::getValue (OS default plus user config only),
ConfigFile::getPolicySetting (Windows policy overlay) or raw QSettings, and are
migrated onto getConfig as they are onboarded into the schema.

Those read paths skip the enforcement hierarchy, so a setting read through them
cannot be enforced by an administrator. getConfig is the single enforcement aware
read path.

## Principle

There is exactly one way to read a managed setting: getConfig. It walks the full
hierarchy and returns the effective value plus metadata (source, enforcement).
getValue and getPolicySetting stay only for keys not yet onboarded, and a wired
managed key is never read with raw QSettings.

We do not move away from ConfigFile. ConfigFile is the enforcement gateway;
getConfig is its single read entry point.

## API (on ConfigFile)

    // Resolved read: value plus source and enforcement metadata.
    ManagedValue getConfig(const QString &name, const QVariant &builtinDefault = {},
                           const QString &connectionGroupName = {}) const;

    // Typed convenience over getConfig().value.
    template<typename T>
    T getConfig(name, group = {}) const;

    // Write to the user config; returns false and writes nothing when enforced.
    bool setConfig(name, const QVariant &value, group = {});

    // UI helpers.
    bool isEnforced(name, group = {}) const;             // true means disable the control
    SettingSourceType sourceOf(name, group = {}) const;  // for the "Managed by ..." label

## Behavior

- getConfig finds the SettingSpec in ManagedSettingsSchema, or synthesizes one
  (builtinDefault, enforceable) for keys not yet in the schema, then builds the
  source stack (buildDeviceSources, UserConfigSource for the group,
  buildServerSources from the cached server settings) and resolves. The
  getConfig<T> template returns any type; resolveManagedBool is removed.
- setConfig resolves first: if the effective value is enforced it returns false
  and writes nothing, so a user can never override an enforced value. Otherwise it
  writes the user config.
- isEnforced and sourceOf read the resolved metadata. The settings UI disables a
  control when isEnforced is true and shows who set it from sourceOf. Enforced
  settings are disabled, never hidden; a server default stays editable.
- Update and proxy keys are default only from the server. Only device policy can
  enforce them, so a server cannot disable updates or reroute traffic.

## Managed keys

Wired through getConfig: skipUpdateCheck, autoUpdateCheck, confirmExternalStorage,
useNewBigFolderSizeLimit, notifyExistingFoldersOverLimit, newBigFolderSizeLimit,
stopSyncingExistingFoldersOverLimit. The last two carry a runtime or theme
default, so they are resolved with a runtime default at the call site and are not
in the schema.

Proxy resolves through getConfig as well, wrapped in
ConfigFile::managedProxySettings, which reads proxyType, proxyHost and proxyPort
together so the type, host and port are always managed as one tuple.

virtualFilesMode resolves through ConfigFile::managedVirtualFilesMode. It is per
folder (FolderDefinition), so the value is applied where a new folder is created:
the add folder wizard preselects and, when enforced, disables the virtual files
checkbox, and the account setup wizard forces or hides the virtual files sync mode
the same way. The server may enforce it, so an enforced value can come from the
server or from device policy.

Legacy keys are stored at the top level of the .cfg while managed writes use the
account group, so getConfig reads both: the account group at priority 50 and the
top level at 49, the group value winning when both exist.

Server delivered values are range checked in sanitizeServerManagedSettings (the
folder size limit); invalid values are dropped.

Setters refuse an enforced write: the folder limit setters go through setConfig and
Account::setProxySettings refuses a managed proxy write. UI enforcement (disable and
label) covers the update control, the folder limit controls (advancedsettings) and
the proxy editor (networksettings).

Proxy unifies the two layers. The network dialog edits per account state (Account)
while the resolver reads the managed proxy keys, so managedProxySettings resolves
type, host and port as one tuple and AccountManager applies it at account load: an
enforced value always wins, a default only when the account follows the system
proxy. Account::proxySettingsAreManaged is set when the proxy is enforced,
Account::setProxySettings refuses a write while managed, and NetworkSettings
disables the editor and shows the managed label. Any enforced field disables the
whole editor, but only the managed fields replace values, so an account keeps its
own value for the rest. The server can only default the proxy, never enforce it, so
an enforced proxy always comes from device policy.

## Scope

Done:
- getConfig, the typed getConfig<T> template, setConfig, isEnforced and sourceOf on
  ConfigFile; resolveManagedBool removed.
- Wired keys: skipUpdateCheck, autoUpdateCheck, the folder limit keys, proxy and
  virtualFilesMode.
- Enforced controls disabled with a managed label in the settings dialogs (folder
  limits in advancedsettings, proxy in networksettings) and in the folder wizards
  (virtual files).
- Server delivered values sanitized against the client allow list and range checked.

Follow up:
- Migrate the remaining settings onto getConfig as they are onboarded into the
  schema, retiring their getValue and getPolicySetting use.
- A QML facade (Q_INVOKABLE getConfig/setConfig/isEnforced) for the QML settings UI.
- A guard against new raw QSettings reads of managed keys.
- Reapply a managed proxy to loaded accounts on capability refresh; today a managed
  proxy applies at account load, so a change takes effect on reconnect or restart.

## Out of scope

Secrets (proxyPass) and pure runtime state (geometry, lastSelectedAccount) are
not managed and keep plain setValue/getValue.

## Testing

Unit tests are still pending (tracked separately). They should cover getConfig
across bool, int and string; setConfig writing when not enforced and refusing when
enforced; isEnforced and sourceOf metadata; the proxy per field merge and the
setProxySettings guard while managed; and virtualFilesMode default versus enforced
in the wizards.

## Flow diagrams

### Resolution

How a managed setting is resolved (e.g. virtualFilesMode, which is server
enforceable; update and proxy keys never take the server enforced step):

```text
config.php (admin)                                     [server, optional]
  |
  support app Capabilities::getCapabilities
  |   allow list filter, enterprise subscription gate
  |
  OCS: support.desktopClient { defaults, enforced }
  |
Account::setCapabilities                               [client]
  |
  Account::updateServerManagedSettings
  |   Capabilities::desktopClientManagedSettings then parseServerManagedSettings
  |   sanitizeServerManagedSettings  (client allow list, drops non enforceable)
  |
  AccountManager::updateServerManagedSettings
  |   merge subscribed accounts (the subscribed account wins)
  |
  ConfigFile::setServerManagedSettings   (JSON in .cfg, offline cache)
  |
ConfigFile::getConfig                                  [read]
  |   add the sources for this key:
  |   buildDeviceSources()   Windows GP / macOS forced (enforced), OS default
  |   UserConfigSource       the user .cfg
  |   buildServerSources()   server enforced, server default
  |
  ManagedSettings::resolve(spec)
  |   the highest priority source that has a value wins:
  |
  device enforced (200) > server enforced (100) > user (50)
                      > server default (30) > device default (20) > builtin
  |
  ManagedValue { value, source, enforced/default }     [return]
```

### Server delivery

From the admin's config.php to a source the resolver can read:

```text
[server, support app]                     (separate repo, enterprise gated)
  config.php: desktopclient.defaults / .enforced
    |
  DesktopClientSettingsService   allow list, never secrets
    |
  Capabilities: support.desktopClient { schemaVersion, defaults, enforced }
    |
  OCS  /cloud/capabilities
    |
[client]
  Account::setCapabilities
    |
  Capabilities::desktopClientManagedSettings
    |   parseServerManagedSettings   (capability map into ServerManagedSettings)
    |
  sanitizeServerManagedSettings
    |   client allow list: drop unknown keys,
    |   keep only server enforceable keys in enforced
    |   (update and proxy keys are default only, never server enforced)
    |
  AccountManager::updateServerManagedSettings
    |   merge subscribed accounts, the subscribed account wins
    |
  ConfigFile::setServerManagedSettings   (JSON in .cfg, offline cache)
    |
  buildServerSources
    |   ServerSettingsSource  enforced   (ServerEnforced, priority 100)
    |   ServerSettingsSource  defaults   (ServerDefault, priority 30)
    |
  [added to the resolver by ConfigFile::getConfig]
```
