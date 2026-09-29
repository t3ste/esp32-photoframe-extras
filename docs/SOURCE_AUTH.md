# A login in the calendar and ToDo addresses

> **Build option:** compiled in only with `python build.py --with source-auth` (needs `agenda`); without it the
> firmware and its web UI are the upstream ones (see [FEATURES.md](FEATURES.md)).

Some calendars are not public: a home server (Radicale, Baikal, Nextcloud) asks for a user name and a password. With this
option the address of a calendar (A-E) or of the ToDo list may carry them:

```
https://user:password@calendar.example.org/dav/user/calendar/?export
```

The frame takes the login out of the address, sends the request, and answers the server's demand for a login with **HTTP
Basic** or **Digest** authentication - whichever the server asks for. Nothing else changes: the address is entered in the
same field as before.

## Special characters

A `@`, `:`, `/`, `?`, `#` or `%` inside the user name or the password has to be written as a percent code, or the address
means something else:

| Character | Write | Character | Write |
| --- | --- | --- | --- |
| `@` | `%40` | `/` | `%2F` |
| `:` | `%3A` | `?` | `%3F` |
| `#` | `%23` | `%` | `%25` |
| space | `%20` | `+` | stays `+` |

A password `p@ss:word` becomes `p%40ss%3Aword`. The user name ends at the first `:`; the address itself (host, path,
query) is not changed. A malformed code (`%zz`) or a part that is too long (over 63 characters for the user name, over 127 for the password) makes the
fetch fail with a message in the frame's log.

## What keeps the login safe

- **It stays in the address field** - a **write-only** field like every calendar address: the Web UI never shows it again
  (only a check mark that something is saved), a normal **Export config** leaves it out, and the example configs of this
  project never contain one. Only the opt-in export option **Include credentials and URLs in export** writes it out - keep that file private.
- **Not over plain `http://`.** A login is only sent over an encrypted `https://` connection. For a server in your own
  network that has no certificate (Radicale on a Raspberry Pi, say) tick **Allow a login over plain http://** in the
  calendar settings: the login then goes over the network unencrypted, so use it only where you trust the network. With
  the box unticked such a fetch is refused, with a message in the log.
- **Digest first for plain http.** Over `http://` the frame waits for the server's challenge instead of sending Basic
  right away, so a server that offers Digest never receives the password itself.
- **No second chances.** One answer to the challenge, then the fetch stops (a wrong password is not tried again and
  again, which some servers treat as an attack); the log says the server refused the login.
- **No redirects with a login.** A redirect is not followed while a login is set - the login must never end up at another
  host. Use the address the server finally answers from.
- **Never in the log.** The frame's log does not print calendar addresses, and the login is removed from the address
  before the request is built.

The frame stores the address like all its settings: in its flash memory, not encrypted. Anyone who can reach the web UI
of the frame can export it - set the device password (General → Advanced network settings) if that is not only you.

## Notes

- Works for every field that takes a calendar or ToDo address: Calendar A-E and the ToDo list. It also applies to the
  weather or headline feeds, should one of their addresses carry a login.
- `webcal://user:password@host/...` works as well with the `webcal` option.
- OAuth (Google Calendar's private CalDAV, Microsoft 365) is not supported - use the calendar's ICS "secret address"
  instead.
