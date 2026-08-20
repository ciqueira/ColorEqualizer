# NexKeyRuntime 0.2.2 (vendored)

Binaries and headers copied verbatim from the published release so this
repository builds with `make clean && make` and no external directory:

<https://github.com/ciqueira/NexKeyRuntime/releases/tag/v0.2.2>

| File | Source archive |
|---|---|
| `include/nexkeyruntime/*` | `nexkeyruntime-0.2.2-macos-universal.zip` |
| `lib/macos/libnexkeyruntime.a` | `nexkeyruntime-0.2.2-macos-universal.zip` |
| `lib/windows-x64/nexkeyruntime.lib` | `nexkeyruntime-0.2.2-windows-x64.zip` |

The headers are taken from the macOS archive. Both archives carry the same
header content; the Windows one only differs in line endings, because
`Compress-Archive` rewrote them to CRLF.

`libnexkeyruntime.a` is a universal binary (arm64 + x86_64) and is
self-contained: it vendors Monocypher for Ed25519, so nothing needs libsodium
installed to consume it.

## Updating

Download both archives from a newer release, replace the files above, and bump
`VERSION`. Then rebuild and re-check that the plugin still exports exactly two
symbols — linking a static archive re-exports whatever that archive marked
default-visible, which is what `../../..//MCPlugins/MCColorEqualizerMake/ofx-exports.txt`
exists to contain:

```sh
nm -gU MCColorEqualizer.ofx      # must list only OfxGetNumberOfPlugins/OfxGetPlugin
```

## Licence

Apache-2.0. See `LICENSE` and `NOTICE` in the release archive and at
<https://github.com/ciqueira/NexKeyRuntime>.
