# Kimura SCM Unreal Plugin

**Plugin version:** 0.8.3

This plugin integrates Unreal Engine's Source Control interface with [Kimura SCM](https://www.kimurascm.com). It connects the Unreal Editor to the Kimura SCM command-line bridge (`kscm.exe`) and exposes Kimura workspaces, file status, changelists, history, sync, submit, and related operations.

For Kimura SCM setup and workflow information, see the official [Documentation and resources](#documentation-and-resources).

## Requirements

- Windows 64-bit
- Unreal Engine 5.7 or 5.8
- Kimura SCM installed on the machine running the Editor ([installer](https://www.kimurascm.com/download/))
- A Kimura SCM server, account, and workspace

It launches the Kimura SCM workspace bridge (`kscm.exe host-workspace`) when the module starts. The installed bridge must be compatible with the plugin version; the plugin rejects bridge versions with a newer major or minor version than it supports.

## Compatibility

| Plugin version | Unreal Engine | Platform |
|---|---|---|
| 0.8.x | 5.7, 5.8, 6.0 | Win64 |

The Kimura SCM workspace bridge must also be compatible with the plugin version. Newer major or minor bridge versions are rejected.

## Installation

1. Install Kimura SCM and verify that its `kscm.exe` workspace bridge is installed.
2. Copy this plugin directory into the project, for example:

   ```text
   MyGame/Plugins/KSCM/
   ```

3. Regenerate the Unreal project files if required by your engine version.
4. Build the project Editor target for `Win64`.
5. Start the Unreal Editor and enable **Kimura SCM** if it is not already enabled.
6. Select Kimura SCM as the active source-control provider.

This repository is source-oriented. A packaged plugin should be built against the target Unreal Engine version and tested from a clean checkout before distribution.

## Configuration

Open the Unreal source-control settings and select **Kimura SCM**. Configure:

- **Workspace** — the plugin attempts to list workspaces compatible with the current project's path; select the appropriate workspace from the list.
- **Server address** — the Kimura SCM server name and port, for example `HOSTMACHINE:8666`.
- **User** and **Password** — Kimura SCM credentials.
- **Verify server certificate** — require a trusted server certificate.
- **Client certificate** — use no certificate, a certificate-store thumbprint, or a PFX file and password.

## Supported operations

The provider currently advertises these Unreal source-control operations:

- Connect and status refresh
- Mark for add, checkout, mark for delete, save, revert, and sync
- Check in files or changelists
- Create, edit, delete, refresh, and move files between changelists
- Query available workspaces and file locations
- File history and revision status

## Troubleshooting

### Kimura SCM is unavailable

Check that:

- The Editor is running on Win64.
- Kimura SCM is installed.
- The `kscm.exe` workspace bridge is installed and can be launched.
- The installed bridge does not have a newer major or minor version than the plugin's supported bridge version.

### Connection fails

Verify the server address, credentials, workspace, certificate settings, and network access. If server certificate verification is enabled, ensure the server certificate is trusted by the machine.

### Finding diagnostic logs

Search the Unreal Editor log for `LogKimuraSCM`. Useful messages include bridge startup, connection failures, operation failures, unsupported operations, and operation timings.

## License

The Kimura SCM Unreal Plugin is licensed under the [Apache License 2.0](LICENSE).

Kimura SCM, `kscm.exe`, the Kimura SCM server, and related trademarks are separate products and remain subject to their own licensing and usage terms. Unreal Engine is governed by Epic Games' applicable license.

## Documentation and resources

The [Kimura SCM documentation](https://docs.kimurascm.com) is the primary source of information for Kimura SCM, including server, workspace, authentication, and workflow details.

- [Kimura SCM documentation](https://docs.kimurascm.com)
- [Kimura SCM website](https://www.kimurascm.com)
