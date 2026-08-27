# NexKeyRuntime 0.5.2 (vendored — macOS locally built, Windows still 0.5.0)

Binaries and headers copied verbatim from a local build so this repository
builds with `make clean && make` and no external directory:

| File | Source |
|---|---|
| `lib/macos/libnexkeyruntime.a` | Built locally from `NexKeyRuntime` commit `e13bb30` — not yet a published GitHub release |
| `include/nexkeyruntime/*` | Same local build; only the version macros and the earlier Perfil→Profile comment fix changed since 0.5.0 |
| `lib/windows-x64/nexkeyruntime.lib` | **Unchanged — still `nexkeyruntime-0.5.0-windows-x64.zip`** from <https://github.com/ciqueira/NexKeyRuntime/releases/tag/v0.5.0>. Not rebuilt in this pass; see below. |

This is a pre-release testing step, not a version bump backed by a tagged
release: the macOS fix being validated here (both `nexkeyruntime_license_deactivate()`
and `nexkeyruntime_license_export_deactivation_proof()` used to wipe every
receipt in the tenant directory instead of just the one for this handle's
own entitlement — see the SDK's `CHANGELOG.md` for the full write-up) has
not gone through `release-nexkeyruntime.yml` yet. Once `v0.5.2` is tagged
and published for both platforms, replace this README (and the Windows
binary) with the normal release-archive provenance the rest of this file's
history uses.

`libnexkeyruntime.a` is a universal binary (arm64 + x86_64) and is
self-contained: it vendors Monocypher for Ed25519, so nothing needs libsodium
installed to consume it — confirmed for this build via
`PKG_CONFIG_LIBDIR=/nonexistent` at configure time, the same flag
`release-nexkeyruntime.yml` uses to force the vendored backend on a runner
that happens to have libsodium installed.
