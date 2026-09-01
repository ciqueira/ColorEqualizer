# NexKeyRuntime 0.5.4 (vendored)

Binaries and headers copied verbatim from the published release archives, so
this repository builds with `make clean && make` and no external directory:

| File | Source |
|---|---|
| `include/nexkeyruntime/*` | `nexkeyruntime-0.5.4-macos-universal.zip` |
| `lib/macos/libnexkeyruntime.a` | `nexkeyruntime-0.5.4-macos-universal.zip` — universal, `x86_64 arm64` |
| `lib/windows-x64/nexkeyruntime.lib` | `nexkeyruntime-0.5.4-windows-x64.zip` |

Both from <https://github.com/ciqueira/NexKeyRuntime/releases/tag/v0.5.4>, with
the archive checksums verified against that release's `checksums.txt` before
extraction. `CHECKSUMS.txt` here covers the extracted files as they sit in this
directory.

This restores the normal release-archive provenance. The previous drop was a
pre-release testing step — macOS built locally from an untagged commit, Windows
left behind on 0.5.0 — which the 0.5.4 release supersedes on both platforms.

## Why 0.5.4 matters for this plugin

The background refresh used to be deferred every time the render guard was
called, with no ceiling. A host that renders continuously — which is what
DaVinci Resolve does during playback, scrubbing and preview — deferred it
forever, so nothing from the server ever reached a plugin while it was in use:
not revocation, not suspension, not expiry, not deactivation. Deferral is now
bounded. See the SDK's `CHANGELOG.md`.
