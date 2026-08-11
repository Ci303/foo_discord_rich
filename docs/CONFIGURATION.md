# Configuration Guide

Discord Rich Presence Integration uses foobar2000 title formatting strings for
the text shown in Discord. You can edit these in:

`Preferences > Tools > Discord Rich Presence Integration > Main`

## Text Fields

The component exposes three text fields:

- Top: usually the track title or main display line.
- Middle: usually the artist.
- Bottom: usually the album, release, playlist, or playback context.

Use **Preview current track** to evaluate the unsaved Top, Middle, and Bottom
fields against the currently playing track before applying the settings.

Discord's `Listening to ...` application label is controlled by Discord and
cannot be changed per track. The title-formatting fields below it are the parts
this component can update dynamically.

## Recommended Layouts

### Track, Artist, Album

Top:

```text
[$if2(%title%,%filename%)]
```

Middle:

```text
[$if2(%artist%,Unknown artist)]
```

Bottom:

```text
[$if2(%album%,Unknown album)]
```

### Artist - Track

Top:

```text
[$if2(%artist%,Unknown artist) - $if2(%title%,%filename%)]
```

Middle:

```text
[$if2(%album%,Unknown album)]
```

Bottom:

```text
[]
```

### Album-Focused

Top:

```text
[$if2(%album%,Unknown album)]
```

Middle:

```text
[$if2(%artist%,Unknown artist)]
```

Bottom:

```text
[$if2(%title%,%filename%)]
```

### Radio or Stream

Top:

```text
[$if2(%title%,$if2(%streamtitle%,%filename%))]
```

Middle:

```text
[$if2(%artist%,$if2(%album artist%,Live stream))]
```

Bottom:

```text
[$if2(%album%,$if2(%codec%,))]
```

## Common Variables

These are standard foobar2000 title-formatting fields commonly useful in
Discord presence:

| Variable | Meaning |
| --- | --- |
| `%title%` | Track title |
| `%artist%` | Track artist |
| `%album artist%` | Album artist |
| `%album%` | Album title |
| `%date%` | Release date or year |
| `%genre%` | Genre |
| `%tracknumber%` | Track number |
| `%discnumber%` | Disc number |
| `%filename%` | File name without extension |
| `%codec%` | Audio codec |
| `%bitrate%` | Bitrate |
| `%samplerate%` | Sample rate |
| `%length%` | Formatted track length |
| `%length_seconds%` | Track length in seconds |
| `%playback_time%` | Current playback time |
| `%playback_time_seconds%` | Current playback time in seconds |

Foobar2000 supports many more fields and functions. Use foobar2000's built-in
title formatting help for the complete reference.

## Useful Functions

### Fallbacks

Use `$if2(a,b)` when a tag may be missing:

```text
[$if2(%title%,%filename%)]
```

### Conditional Text

Square brackets display their contents only when all fields inside are present:

```text
[%artist% - %title%]
```

If either artist or title is missing, the whole line is hidden.

### Multiple Fallbacks

Use `$if3(a,b,c)` for several possible values:

```text
[$if3(%album artist%,%artist%,Unknown artist)]
```

### Lowercase

```text
[$lower(%artist%)]
```

### Text Cleanup

For short, clean Discord fields, avoid long technical strings unless you
specifically want them visible. Discord also applies length limits, so very long
results are truncated by the component.

## Album Art

Album art can be provided in two ways:

- MusicBrainz fetcher: automatic lookup using artist, album, and optional
  MusicBrainz album ID tags.
- Art uploader: runs a user-provided command that uploads local or embedded art
  and returns a public image URL.

The uploader overrides MusicBrainz. If `Upload and display art` is enabled on
the Advanced tab, MusicBrainz fetching is disabled even if the MusicBrainz
checkbox is enabled on the Main tab.

The Main tab also provides an **Artwork behaviour** selector:

- **Prefer artwork; use large-image fallback** requests artwork first and
  displays the configured large image while artwork is being fetched or when no
  match exists. This is the default and preserves previous behaviour.
- **Use configured large image only** skips artwork requests
  and uses the configured large image. If that image is disabled, no large
  image is shown.
- **Album artwork only; no fallback image** requests artwork but leaves the
  large image empty while fetching or when no match exists.

The artwork status line reports whether the applied configuration is idle,
fetching, resolved, using a cached no-match result, or encountered a
fetch/uploader failure. Unsaved artwork changes are labelled as pending rather
than being mixed with live status. Cached status is provider-neutral, and URLs
and uploader output are not shown.

For automatic artwork:

1. Disable `Upload and display art` on the Advanced tab.
2. Enable `Fetch and display album art from MusicBrainz` on the Main tab.
3. Make sure tracks have useful `%artist%` and `%album%` tags.
4. Prefer MusicBrainz album ID tags where available:
   - `MUSICBRAINZ_ALBUMID`
   - `MUSICBRAINZ ALBUM ID`

Artwork is cached separately for MusicBrainz and the uploader so one provider's
result cannot suppress the other. A valid MusicBrainz album ID identifies its
release directly; otherwise the artist and album identify the lookup. Uploader
results use the configured upload pin. Successful results are refreshed
periodically, while "not found" results expire sooner so temporary metadata and
service problems do not require manual cache clearing. Cache formats older than
version 4 are deliberately ignored because their provider and release identity
is ambiguous; the component rebuilds them as tracks are played.

Use **Advanced > Art cache > Load** to reload the current cache file, or use
**Clear cache** if a bad lookup is cached. Load reports when no file exists.
After a successful load or clear, the component immediately re-evaluates the
current track where artwork is enabled; file errors are reported instead of
being shown as successful operations.

Use **Test** beside the upload command to run the configured uploader against
the current track. The command may take up to ten seconds and the result is
shown without applying the preferences. Upload commands are executable programs
and should only be configured from a source you trust.

## Recommended Settings

### Simple Track / Artist / Album

- Top: `[$if2(%title%,%filename%)]`
- Middle: `[$if2(%artist%,Unknown artist)]`
- Bottom: `[$if2(%album%,Unknown album)]`
- Enable `Fetch and display album art from MusicBrainz`.
- Disable `Upload and display art` unless you have configured an uploader.

### Album Art First Run

MusicBrainz art lookup runs in the background. The first play may briefly show
the large image fallback, then update to album art after the request finishes.
Subsequent plays use the local art cache.

## Playback Images

The component can show small playback status images:

- Playing image
- Paused image
- Disabled

If `Disable Rich Presence when paused` is enabled, Discord presence is cleared
while playback is paused instead of showing the paused state.

## Discord Application and Assets

This fork defaults to the maintained Discord application `Foobar2000`, with
application ID `1536157545863847938`. On the first startup after upgrading,
the component replaces the legacy default ID (`507982587416018945`) and records
that the migration has run. Any other custom application ID is preserved, and
the legacy ID can still be selected deliberately afterwards while remaining on
this or a newer release. Downgrading can discard the migration marker, so a
later re-upgrade may migrate the legacy ID again.

The six configured Rich Presence asset keys are:

- `foobar2000`
- `foobar2000-dark`
- `playing`
- `playing-dark`
- `paused`
- `paused-dark`

The key mapping, included playback-state PNGs, and Developer Portal metadata
guidance are recorded in the [Discord asset manifest](../images/README.md).
Portal artwork is not read locally or bundled into the `.fb2k-component`;
Discord serves uploaded assets by key.

## Troubleshooting

### Discord shows another application instead of foobar2000

Discord's automatic recognition and this component's Rich Presence are separate
activities. When both are active, Discord may show a recognised game or
application instead of the foobar2000 activity, particularly in the compact
profile card. This component cannot inspect, reorder, or suppress other Discord
activities.

To prefer foobar2000 over a conflicting recognised application:

1. Open `Discord > User Settings > Registered Games`.
2. Find the conflicting application and turn off its eye icon.
3. Under `Activity Sharing`, keep `Share my activity` enabled so this
   component's Rich Presence can still be displayed.

The artwork behaviour setting changes only the large image on the foobar2000
activity card. It cannot replace another application's icon or control which
activity Discord chooses to display.

### Album art does not appear

- Check the artwork status shown on the Main tab.
- Select `Prefer artwork` or `Album artwork only`; `Use configured large image
  only` intentionally skips artwork requests.
- Confirm uploader mode is disabled unless you configured an upload command.
- Confirm the track has artist and album tags.
- Try a release with a MusicBrainz album ID tag.
- Use `Advanced > Art cache > Open containing folder...` to inspect cached
  results.
- Enable debug logging in foobar2000 Advanced Preferences if you need request
  details.

### Text is blank

Square brackets hide their contents when fields are missing. Use `$if2` if you
want fallback text:

```text
[$if2(%title%,%filename%)]
```

### `Listening to foobar2000` does not change

That text is Discord's application name. Discord does not allow this component
to change it per track. Put dynamic track details in Top, Middle, and Bottom.
