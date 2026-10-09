# libical

The firmware built with the option `agenda-rrule` contains parts of **libical** (<https://github.com/libical/libical>,
version 4.0.6), the iCalendar library, for the recurrence rules of the Agenda's calendars.

- **Copyright** of libical: Eric Busboom, the libical authors and contributors (see the headers of the files in
  `components/libical/libical/`, and libical's `CONTRIBUTORS.md`).
- **License:** libical is offered under the *GNU Lesser General Public License v2.1 only* **or** the *Mozilla Public
  License v2.0*, at the licensee's choice. **This project uses it under the MPL-2.0.** The text is in
  [libical-MPL-2.0.txt](libical-MPL-2.0.txt) (and in `components/libical/LICENSES/`), libical's own statement of the
  dual license in `components/libical/LICENSE.txt`.
- **Source:** the files are in this repository, unmodified, in [`components/libical/libical/`](../../components/libical/libical)
  (what was taken from which tag: [`components/libical/UPSTREAM.md`](../../components/libical/UPSTREAM.md)). Under the
  MPL-2.0 the source of those files has to stay available to everyone who gets a firmware that contains them, and a
  change to one of them has to be published under the MPL-2.0 too; the rest of the firmware is not affected by it (the
  MPL works per file).
- **Not part of the firmware:** libical's other directories (bindings, tests, the zone database), and `vzic`
  (GPL-2.0-or-later, never built or shipped) - nothing of them is in this repository.
