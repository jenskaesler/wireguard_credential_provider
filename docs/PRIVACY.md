# Privacy Policy

WireGuard Credential Provider is designed for enterprise deployment on domain-joined
Windows machines. This document describes what data leaves the machine and under what
conditions.

---

## Data collection

**WireGuard Credential Provider does not collect, store, or transmit any personal data**
beyond what is described in this document.

There is no telemetry, no analytics, no crash reporting, and no user tracking of any kind.

---

## Automatic update check

The tray application (`WireGuardCPTray.exe`) includes an optional automatic update check.

**What is sent:**

| Data | Recipient | Purpose |
|------|-----------|---------|
| IP address (implicit in the HTTPS connection) | GitHub (api.github.com) | Routing |
| User-Agent header: `WireGuardCP-UpdateCheck/1.0` | GitHub (api.github.com) | Identifying the client |

No personal data, device identifiers, usernames, or configuration details are sent.

**When it runs:**

10 minutes after the tray application starts, a single HTTPS request is made to:

```
GET https://api.github.com/repos/jenskaesler/wireguard_credential_provider/releases/latest
```

If a newer version is available, a Windows balloon notification is shown. No background
communication takes place after that.

**GitHub's privacy policy** applies to the connection to `api.github.com`:  
https://docs.github.com/en/site-policy/privacy-policies/github-general-privacy-statement

**How to disable:**

The update check is controlled by a registry value and is disabled by default for silent
installs with the `/NOUPDATECHECK` parameter. Administrators can disable it via registry
or Group Policy:

```reg
[HKEY_LOCAL_MACHINE\SOFTWARE\Jens Kaesler\WireGuard Credential Provider]
"AutoUpdateCheck"=dword:00000000
```

---

## Log files

Log files are written locally to `%InstallDir%\logs\wgcp_ddMMyyyy.log`. They contain
diagnostic information (connection events, errors, timestamps) and are never transmitted
anywhere. Retention is configurable via the `LogRetentionDays` registry value (default: 7 days).

---

## YubiKey serial number

When a YubiKey is inserted, the tray application optionally reads the device serial number
using `ykman info` and caches it **in memory only** for display purposes in the context menu.
The serial number is not written to disk or transmitted anywhere. It is cleared from memory
when the YubiKey is removed.

---

## WireGuard VPN traffic

VPN traffic is handled entirely by the WireGuard for Windows client. WireGuard Credential
Provider does not inspect, log, or forward VPN traffic. Refer to WireGuard's documentation
for information about the WireGuard protocol.

---

## Contact

For privacy-related questions, open an issue on GitHub or use the contact information
in the project's [LICENSE](../LICENSE).
