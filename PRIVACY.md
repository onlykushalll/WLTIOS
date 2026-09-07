# Privacy Policy — WLTIOS

**Last Updated:** September 7, 2026

WLTIOS (We Love Tor, Intelligent OS) is built from the ground up on the principle of absolute personal privacy and anonymity.

---

## 1. Amnesic RAM-Only Guarantee

- **Zero Disk Residue:** All operating system filesystem writes execute within volatile system RAM (`tmpfs`).
- **Complete Destruction on Shutdown:** When your computer is powered off or rebooted, the RAM loses power and all session activity, documents, browser caches, and encryption keys are permanently destroyed. No forensic artifacts are written to physical hard drives or SSDs.

---

## 2. Unconditional Tor Anonymity

- **100% Tor Routing:** Every network connection initiated by any application or service is routed through the Tor network via SOCKS5 on port 9050.
- **Hardware Kill Switch:** Direct non-Tor internet connections are blocked at the kernel layer using route deletion (`SIOCDELRT`). If Tor loses connectivity, all network egress drops immediately to prevent clearnet leaks.
- **MAC Address Randomization:** A new random MAC address is generated for every network adapter on every boot.

---

## 3. Zero Telemetry & Zero Logging

- WLTIOS contains zero diagnostic beacons, telemetry collection, or remote reporting.
- We have no ability to identify who downloads, boots, or uses WLTIOS.

---

## 4. Operational Security Limitations

While WLTIOS defends against network surveillance and remote exploitation, no software can protect against:
- Compromised hardware firmware (BIOS/UEFI backdoors)
- Hardware keyloggers or screen recording cameras
- Cold-boot attacks executed immediately after power-off before RAM cells discharge

---

## 5. Contact

For security vulnerability disclosures or architectural questions, please file an issue on GitHub:
https://github.com/onlykushalll/WLTIOS
