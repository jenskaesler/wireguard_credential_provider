# Security Policy

## Supported Versions

Only the latest released version receives security fixes.

| Version | Supported |
|---------|-----------|
| Latest  | ✅ Yes    |
| Older   | ❌ No     |

## Reporting a Vulnerability

**Please do not report security vulnerabilities through public GitHub issues.**

This project is a Windows Credential Provider — a pre-logon authentication component that
runs inside LogonUI and Winlogon. A vulnerability here could allow authentication bypass,
privilege escalation, or exposure of VPN credentials. Please treat findings accordingly.

### How to report

Use one of the following channels:

1. **GitHub Private Vulnerability Reporting** (preferred):  
   [github.com/jenskaesler/wireguard_credential_provider/security/advisories/new](https://github.com/jenskaesler/wireguard_credential_provider/security/advisories/new)

2. **Email**: mail@jens-kaesler.de  
   Please encrypt sensitive reports using the PGP key linked in the GitHub profile.

Include in your report:
- Component affected (Credential Provider DLL, Tray App, Shutdown Service, Installer)
- Description of the vulnerability and its impact
- Steps to reproduce or a proof-of-concept
- Affected version(s)

### Response timeline

| Milestone | Target |
|-----------|--------|
| Acknowledgement | Within 5 business days |
| Initial assessment | Within 10 business days |
| Fix / mitigation | Depends on severity (critical: ASAP) |

You will be credited in the release notes unless you request otherwise.

## Scope

**In scope:**
- Authentication bypass in the pre-logon Credential Provider tile
- Privilege escalation via the `WireGuardShutdownHelper` Windows service (SYSTEM context)
- Named pipe (`\\.\pipe\WireGuardCPHelperSvc`) access control bypass
- Arbitrary file read/write/delete via the named pipe IPC without path validation
- PIN or certificate thumbprint exposure from registry or memory
- DPAPI key material exposure via the SYSTEM service
- Installer privilege escalation or malicious payload substitution

**Out of scope:**
- Vulnerabilities in WireGuard for Windows itself — report those to the [WireGuard project](https://www.wireguard.com/security/)
- Vulnerabilities in YubiKey hardware or the YubiKey Manager CLI — report those to [Yubico](https://www.yubico.com/support/security-advisories/)
- Attacks requiring physical access to an already-authenticated session
- Social engineering

## Security-sensitive design decisions

The named pipe `\\.\pipe\WireGuardCPHelperSvc` has a NULL DACL (world-accessible by design,
since the tray application runs as a standard user and must reach the SYSTEM service). The
security boundary is path validation: only paths inside the WireGuard configuration directory
ending in `.conf.dpapi` or `.conf` are accepted. If you believe this validation is bypassable,
that is in-scope.
