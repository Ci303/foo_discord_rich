# Configuration Guide

Discord Rich Presence Integration uses foobar2000 title formatting strings for
the text shown in Discord. You can edit these in:

`Preferences > Tools > Discord Rich Presence Integration > Main`

## Text Fields

The component exposes three text fields:

- Top: usually the track title or main display line.
- Middle: usually the artist.
- Bottom: usually the album, release, playlist, or playback context.

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

For automatic artwork:

1. Disable `Upload and display art` on the Advanced tab.
2. Enable `Fetch and display album art from MusicBrainz` on the Main tab.
3. Make sure tracks have useful `%artist%` and `%album%` tags.
4. Prefer MusicBrainz album ID tags where available:
   - `MUSICBRAINZ_ALBUMID`
   - `MUSICBRAINZ ALBUM ID`

Artwork is cached by artist/album or upload pin so repeat plays do not need to
fetch the same art again.

## Playback Images

The component can show small playback status images:

- Playing image
- Paused image
- Disabled

If `Disable Rich Presence when paused` is enabled, Discord presence is cleared
while playback is paused instead of showing the paused state.

## Troubleshooting

### Album art does not appear

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
