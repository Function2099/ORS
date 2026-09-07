# Security policy

## Supported versions

ORS is in **0.x preview**. Fixes land on `main`. There are no long-term support branches yet.

| Version | Supported |
|---|---|
| 0.x (`main`) | Yes |
| Pre-release forks / unofficial builds | No |

## Reporting a vulnerability

Please **do not** open a public GitHub issue for security problems.

1. Use GitHub **Privately report a vulnerability** on this repository (Security tab), or
2. Contact the repository owner through GitHub if private reporting is unavailable.

Include the ORS version (see Settings or `QApplication::applicationVersion`), OS build, and steps to reproduce. You should hear back within 14 days. Please give us time to ship a fix before any public disclosure.

## Scope

In scope: crashes or memory-safety issues in ORS code that an untrusted file, device name, or settings value could trigger; accidental leakage of local paths or credentials in logs.

Out of scope: recording DRM-protected or anti-cheat-restricted content, social-engineering the user into running a malicious build, and bugs that only appear in unofficial patches.
