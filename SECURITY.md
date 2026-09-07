# Security Policy

## Supported versions

| Version | Supported |
|---|---|
| Firmware 1.6.x | Yes |
| Firmware older than 1.6 | No. Contains the 49.71-day uplink alarm overflow, please upgrade |

## Reporting a vulnerability

Please **do not** open a public issue for a security problem.

1. Preferred: use GitHub's private vulnerability reporting, under
   **Security, Report a vulnerability** on this repository.
2. Alternatively: email `email@knollfelix.de` with `[HydroNode Security]` in the
   subject.

Expect an acknowledgement within 7 days. Since this is a spare-time project,
please allow up to 90 days for a fix before public disclosure.

## Scope

In scope:

- Firmware in `stm32_node/` (own application code and sensor drivers)
- The LoRaWAN payload codec and `doc/payload-decoder.js`
- Hardware design flaws with a security impact

Out of scope:

- The HydroNode backend and iOS app. Separate projects, not in this repository
- Vendored third-party components (ST HAL, Arm CMSIS, Semtech LoRa Basics
  Modem). Report those upstream
- Physical attacks requiring disassembly of a deployed node

## How this repository handles key material

**No real LoRaWAN credentials are stored in this repository.**

- `stm32_node/LoRaWAN/App/se-identity.h` is tracked and contains **zeros only**.
  It is a template, not a secret store.
- Real `JoinEUI` / `AppKey` values belong exclusively in
  `stm32_node/LoRaWAN/App/se-identity-local.h`, which is listed in
  `.gitignore` and must never be committed.
- The `DeviceEUI` is derived at runtime from the STM32's 96-bit unique ID and
  does not need to be stored at all.

The git history of this repository was rewritten before it was made public to
remove development key material that had been committed while it was private.
Those keys never left the private repository and are no longer in use.

### If you fork this

Before you push, confirm your keys are not tracked:

```bash
git check-ignore -v stm32_node/LoRaWAN/App/se-identity-local.h
git grep -nE "define +LORAWAN_(APP_KEY|JOIN_EUI)" -- stm32_node/LoRaWAN/App/se-identity.h
```

The second command must show zeros only.

## Threat model notes for operators

- **OTAA only.** ABP is not used; session keys are negotiated per join.
- **Downlink commands** (`0x10` to `0xFF`, see the README) are accepted from the
  network server without an additional application-layer signature. An attacker
  who controls your network-server integration can change the TX interval or
  reset the node. Restrict access to your network server accordingly.
- **Physical access** to a deployed node exposes the SWD interface. Enable
  readout protection (RDP level 1) on production units if that matters to you;
  it is not enabled by default because it complicates field debugging.
