# Discord Application Asset Mapping

This directory records the Developer Portal keys used by the maintained Discord
application for this fork.

- Application name: `Foobar2000`
- Application ID: `1536157545863847938`

## Rich Presence Art Assets

Configure these six keys under `Rich Presence > Art Assets`. Discord lowercases
uploaded asset keys, and the component must reference each key exactly as saved.

| Asset key | Repository file | Role |
| --- | --- | --- |
| `foobar2000` | Not redistributed | Light large-image fallback |
| `foobar2000-dark` | Not redistributed | Dark large-image fallback |
| `playing` | `playing.png` | Light playing-status image |
| `playing-dark` | `playing-dark.png` | Dark playing-status image |
| `paused` | `paused.png` | Light paused-status image |
| `paused-dark` | `paused-dark.png` | Dark paused-status image |

The included playback-state PNGs are setup and maintenance sources. They are
not included in the `.fb2k-component` package because the component sends only
asset keys; Discord serves the corresponding uploaded images from its CDN.

## Portal Metadata

The Discord application icon and optional 1024x576 Rich Presence invite image
are Portal metadata, not runtime component files. The invite image affects only
Discord chat invites and does not change the listening activity card.

Logo-derived application, fallback, and invite artwork is deliberately not
redistributed here because permission to redistribute the foobar2000 icons has
not been established. Maintainers should provide artwork they are authorised to
use. See the official [foobar2000 licence and
credits](https://www.foobar2000.org/license).
