# The extras - user guide

This guide is for people who **use** the frame, not for people who build it. It describes the options that were added after the
first fork release - what each one does, where you switch it on, what you have to enter, what you will see, and what to do when
something does not show up. Each section ends with a link to the detailed page for readers who want the technical side.

> **Do I have these options?** The project's releases are **full** builds (every option the board's hardware supports), so a
> release that contains this work has all of them. If you build the firmware yourself, `python build.py --board <board> --with
> extras` switches on the whole group of them ([FEATURES.md](FEATURES.md)). An option that is not in your firmware simply does
> not appear in the Web UI.

## At a glance

| You want ... | Option | Where in the Web UI | You need |
| --- | --- | --- | --- |
| Calendar links that start with `webcal://` | `webcal` | Settings -> Agenda -> Calendars | nothing |
| A calendar or to-do list on a home server with a password | `source-auth` | the same address fields | the server's user name and password |
| Only the coming days from your own calendar server, repeating events right | `caldav` | Calendars A-E | a CalDAV server |
| Your CalDAV task list in the ToDo column | `caldav-todo` | Settings -> Agenda -> ToDo | a CalDAV server |
| Upload many photos at once | `multi-upload` | Gallery -> Upload Image | nothing |
| Stop the same photo from being stored twice | `upload-dedup` | Settings -> Maintenance -> Duplicate Images | storage (SD card or flash) |
| Real umlauts (ä ö ü ß), degree and euro on the display | `glyphs` | nothing to set | nothing |
| Full-screen pages that take turns with the Agenda | `info-screens` | Settings -> Agenda -> Information screens | nothing |
| ... who does which chore this week | `chore-wheel` | the same box | a list of names |
| ... the weather, big | `weather-screen` | the same box | a weather place (Overlays tab) |
| ... a fact a day | `fact-of-the-day` | the same box | nothing (your own facts are optional) |
| ... exchange rates of the ECB | `finance-snapshot` | the same box | nothing |
| ... the cheapest petrol stations (Germany) | `fuel-prices` | the same box | a free Tankerkoenig key and a place |
| ... stocks, ETFs, indices, crypto, currency pairs | `market-quotes` | the same box | nothing; two free keys are optional |
| ... the travel time there and back to work, red when a jam makes it longer | `route-time` | the fuel page | a free TomTom or HERE key |
| ... a cooking recipe with its picture (Chefkoch, TheMealDB) | `recipes` | the same box | nothing; the filters are optional |
| A schedule that always shows one page (the fuel prices at 06:30, the Agenda hourly) | `schedule-pages` | Settings -> Agenda -> Schedule | nothing |
| A painting, drawing or print from a museum at each rotation | `artworks` | Settings -> Auto Rotate -> mode *Artworks* | nothing; a Smithsonian key is optional |
| Art albums for your frame | `scripts/fetch_art.py` (a PC helper) | on your computer | Python and Node.js |

## 1. Calendars and to-dos

All of this happens in **Settings -> Agenda**, in the same address fields the Agenda always had (Calendar A-E and the ToDo list).

The tab is a list of sections - *ToDo*, *Calendar*, *Extra ICS Calendars*, *Schedule*, *Information screens*, *Appearance and colors* - that start closed. The header of each says what is on, and the sections you opened stay open the
next time. Settings that belong to something you have not switched on are not shown: tick a page under *Information screens* (Markets, say) and its symbols and keys appear; switch the Calendar on and its display options appear.

<img src="screens/agenda-grid-a.png" width="480" alt="The Agenda as a 7-day grid">

*The Calendar column as a 7-day grid (Settings -> Agenda -> Layout). The other layouts and a colour profile: [SCREENSHOTS.md](SCREENSHOTS.md).*

### webcal links

A calendar app's "subscribe" link looks like `webcal://calendar.example.org/me.ics`. Paste it into a calendar field as it is; the
frame fetches it over `https://`. (`webcals://` works too.) Nothing else changes.

### A calendar or to-do list with a password

For a calendar on your own server (Radicale, Baikal, Nextcloud), put the login in front of the host name:

```
https://user:password@calendar.example.org/path/to/calendar.ics
```

- A `@ : / ? # %` inside the user name or password has to be written as a code (`@` -> `%40`, `:` -> `%3A`, `/` -> `%2F`,
  `?` -> `%3F`, `#` -> `%23`, `%` -> `%25`, space -> `%20`). `p@ss:word` becomes `p%40ss%3Aword`.
- The frame sends the login only over `https://`. For a server in your own network that has no certificate, tick **Allow a
  login over plain http://** in the calendar settings - the login then travels unencrypted, so do it only in a network you trust.
- The address field is **write-only**: the Web UI shows only a check mark that something is saved, and a normal settings export
  leaves it out (see "Keys and passwords" below).
- Not supported: Google Calendar's and Microsoft 365's OAuth. Use the calendar's secret ICS address instead.

More: [SOURCE_AUTH.md](SOURCE_AUTH.md).

### CalDAV calendars

If your server speaks CalDAV (Nextcloud, Baikal, Radicale ...), write the address with `caldavs://` (or `caldav://` for plain
http) instead of `https://`:

```
caldavs://user:password@cloud.example.org/remote.php/dav/calendars/user/personal/
```

Use the address of the **calendar itself**, the one a calendar app such as Thunderbird asks for, not the server's start page.
The benefit: the server sends only the coming days instead of the whole history, and it expands monthly and yearly repeating
events (and the exceptions of every kind of repeat) correctly; the frame's own reader handles daily and weekly repeats with
their exceptions, but not monthly or yearly ones.

If nothing shows, the frame's log tells why: `refused the login (HTTP 401)` is a wrong user name or password (or an unencoded
`@`); `REPORT returned HTTP 404/405` is not the address of a calendar. More: [CALDAV.md](CALDAV.md).

### A CalDAV task list as the ToDo column

The same `caldavs://...` form works in the **ToDo** field, pointing at a task list (Nextcloud Tasks, Baikal, Radicale). The
column shows the open to-dos: the text, a priority chip (priority 1-2 -> A, 3-4 -> B, 5 -> C, 6-9 -> D) and the due date in
its colour. Finished ones are left out; a repeating to-do appears once. The frame only reads - ticking a to-do off on the frame
is not possible. More: [CALDAV_TODO.md](CALDAV_TODO.md).

## 2. Pictures

### Uploading many photos at once

In the **Gallery** (Upload Image), select several files in the upload dialog (up to 200) or drop them on the upload area. A queue shows them;
nothing is sent until you press **Upload N files**. Photos are converted in your browser with your current processing settings
(Cover crops to fill the screen, Fit shows the whole picture); a file that is already rendered for your screen (`.epdgz`, or a
PNG of exactly the screen's size with the matching tick box) is sent as it is. **Stop after this file** ends a batch early.
A single file opens the crop editor as before. More: [MULTI_UPLOAD.md](MULTI_UPLOAD.md).

### Stop the same photo from being stored twice

**Settings -> Maintenance -> Duplicate Images** (needs storage):

- *When an upload is already in the album:* **Refuse it** (the default: the frame says which file it duplicates, and the
  Web UI offers **Upload anyway**), **Store it, but say so**, or **Do nothing**.
- *What counts as the same image:* **The file** (same bytes) or **The picture** (same pixels - one photo converted by two
  browsers still counts as one).
- *Index the images that were there before:* turn it on once so older pictures are known too. **Index now** does it on request;
  **Find duplicates** lists what an album has twice, with a delete button for each (nothing is removed automatically).

More: [UPLOAD_DEDUP.md](UPLOAD_DEDUP.md).

### Art albums for your frame (PC helper)

`scripts/fetch_art.py` is a small program for your computer, not part of the firmware. It fetches curated public-domain
artworks, renders them for your board and writes an album folder for the SD card - or uploads it to the frame directly:

```bash
python scripts/fetch_art.py --board waveshare_photopainter_73            # 12 pictures into ./art-out/Art/
python scripts/fetch_art.py --board waveshare_photopainter_73 --count 30 --query landscape
python scripts/fetch_art.py --board waveshare_photopainter_73 --upload --host <address of your frame>
```

It writes an `ATTRIBUTION.md` with title, artist and licence of each picture; keep it with the pictures. Please be modest
with the number of pictures - it is someone else's free service. More: [ART_FETCH.md](ART_FETCH.md).

### Artworks from museums (a rotation mode)

With `artworks` the frame itself fetches one work per rotation: **Settings -> Auto Rotate -> mode Artworks**. The kind of work
(painting, drawing, print) is drawn first, then a random work of the Rijksmuseum, SMK or the Smithsonian that is public domain
or CC0; its picture is loaded in the smallest size that fits the panel, shown whole (*Fit*, the default) or filling the panel
(*Cover*) with a small caption (artist, title, year) and kept in the album `Art`. Pictures of the frame's orientation are preferred
(switchable). With no network - or when anything fails - a picture of that album is shown, also when the album is switched off in the Gallery. The oldest pictures
are deleted to keep free space (by default it keeps 20 % free and cleans up to 30 %), and only pictures this mode made. Nothing
to sign up for; a Smithsonian key is optional. For private use: you are responsible for the terms of use of the pictures. All of it,
including the settings, the free-space rule and the rights hint: [ARTWORKS.md](ARTWORKS.md). Run on one frame in a short session only.

## 3. Text on the display

With `glyphs`, the text the frame draws itself shows **ä ö ü Ä Ö Ü ß ° €** as themselves instead of `ae oe ue ss` and nothing.
There is nothing to set. Other accented letters and emoji are still left out. More: [GLYPHS.md](GLYPHS.md).

<img src="screens/info-fact-de.png" width="400" alt="A German page with umlauts and a sharp s">

## 4. Information screens

The Agenda draws a page of calendar events and to-dos whenever its schedule fires. With the information screens, the frame can
draw **other full-screen pages** on the same schedule: at each run, the next page of the rotation is drawn instead. The Agenda
is one member of the rotation. A picture of each page: [SCREENSHOTS.md](SCREENSHOTS.md).

### Switching pages on

1. Open **Settings -> Agenda -> Information screens** and tick the pages that should take part. The Agenda is ticked by
   default; without any other page ticked the frame behaves as before.
2. Set the **schedule** in the Agenda section (Schedule). Each run of the schedule draws one page, and the pages take turns in
   the order of the list. Examples: `0 */12 *` shows two pages a day (Agenda at 00:00, the next page at 12:00), `0 6 *` draws at
   06:00 every morning.
3. **Keep the interval at 3 minutes or more** - a colour panel should not be refreshed more often than its maker allows
   (Waveshare: not more than every 3 minutes). With six pages and `*/3 * *` each page is on the panel for 3 minutes out of 18.
4. **Save Settings.** The next run of the schedule draws the first page.

**A schedule that always draws the same page** (with `schedule-pages`): under each schedule card tick the pages it should draw. A schedule at
06:30 can then always show the fuel prices while the hourly one shows the Agenda. When two schedules overlap, the one with the smaller number wins
(move a schedule with the arrows), and no display replaces another within the **minimum time between two displays** (15 minutes, adjustable; a
schedule can also have its own **hold time**). The photo rotation gives way to them. A schedule with no page ticked draws the shared rotation as
before, and while no schedule has a page nothing changes. All rules: [SCHEDULE_PAGES.md](SCHEDULE_PAGES.md).

The pages use the language of the on-display text: **Settings -> Overlays -> Overlay language** (English or German).
Pages with data from the internet fetch it when they are drawn, so the frame must be online then. If it is not, or the service
does not answer, the page says so instead of drawing an empty picture, and the next turn of the rotation tries again.

### Small notes you will see

- **When the data were fetched.** Pages with online data (weather, exchange rates, fuel prices, markets) end with
  the time, in the frame's local time: `Updated 30 Sep 14:35` (German: `Stand 30.09. 14:35`) on the weather page, and on the same line as the source
  on the others (`Yahoo Finance, Twelve Data - 30 Sep 14:35`). A frame whose clock was never set draws no time.
- **Which day the data are from.** The exchange-rate and markets pages name the day of the newest price in the header (`Close 30 Sep`, German `Schluss 30.09.`),
  so it is not mistaken for today's date. A small yellow **`!`** behind it says that day is older than the last trading day.
- **How long a chart runs, and what it did.** The header says how many calendar days the lines cover (`MARKETS  41 d`, German `41 T`), and under each line is
  the change over that time (`+6.9%`) next to the change of the last day.

### Chore wheel

<img src="screens/info-chore-wheel.png" width="400" alt="The chore wheel page">

*Who does which chore this week.* Tick **Chore wheel** and fill in two lists, separated by commas:

| Field | Example | Limits |
| --- | --- | --- |
| Members | `Anna, Ben, Clara` | up to 5 names |
| Chores | `Bins, Dishes, Vacuum` | up to 6 names |

Each name can be up to 23 bytes (an umlaut counts as 2), each list up to 159 bytes. The chores go round the members by calendar week, so nobody has to
remember whose turn it is: a wheel with one coloured sector per member, one card per chore with the member's name, and (on tall
enough panels) the name for next week. Without members or chores the page tells you what to fill in. More:
[CHORE_WHEEL.md](CHORE_WHEEL.md).

### Weather

<img src="screens/info-weather.png" width="400" alt="The weather page">

*Today's weather big, and the next four days.* Tick **Weather**. The place and the weather service come from the **Overlays**
tab (Weather location - a name or coordinates - and the service); nothing is entered twice. The page shows today's high in
large digits with a big icon, the low in blue under it, the condition in words, and four more days as rows. Without a place it
says so. More: [WEATHER_SCREEN.md](WEATHER_SCREEN.md).

### Fact of the day

<img src="screens/info-fact.png" width="400" alt="The fact of the day page">

*One fact a day*, the same all day, changing at midnight: a topic, the fact in the biggest text that fits, and - if it has one -
a question to think about. Tick **Fact of the day**. The frame has 24 built-in facts in English and German.

To use **your own facts**, type them into the box under the tick box, one per line, and press **Save facts** (separate from
Save Settings):

```
Honey almost never spoils.
Animals|An octopus has three hearts.
Space|A day on Venus is longer than its year.|Which way does Venus spin compared with Earth?
```

A line is `Fact`, `Topic|Fact` or `Topic|Fact|Question`; empty lines and lines starting with `#` are ignored. Up to 64 facts and
16 KB. The facts go round in the order of your list, one per day. An empty box removes your list and brings back the built-in
facts. (Needs storage.) More: [FACT_OF_THE_DAY.md](FACT_OF_THE_DAY.md).

### A recipe

<img src="screens/info-recipe.png" width="400" alt="The recipe page">

Tick **Recipe** and choose where it comes from: the **recipe of the day** of Chefkoch (classic, vegetarian or vegan), a **Chefkoch search** with the
filters you set (category, country, type of meal, diet, time, rating, order - all optional) or **TheMealDB** (English, by category). The page is drawn for the
orientation you set under Settings -> General, in the biggest text that fits; a recipe without a picture or too long for the page is skipped, and if
nothing is found the filters are relaxed step by step (the page says so) or the last recipe is shown again with a warning. A small QR code can lead to the
recipe. The frame must be online when the page is drawn. More: [RECIPES.md](RECIPES.md).

### Exchange rates

<img src="screens/info-exchange-rates.png" width="400" alt="The exchange-rate page">

*The ECB reference rates against the euro.* Tick **Exchange rates** and, if you like, enter up to four currency codes
(`USD, GBP, CHF, JPY` - the default). Each row shows the price of 1 euro in that currency, the change against the working day
before (a green arrow up, a red arrow down) and a line of the last 30 working days. No account and no key are needed. The ECB
publishes once on working days, so on weekends you see Friday's rates. A currency the ECB has no current rate for (for
example ones it stopped publishing) is left out. More: [FINANCE_SNAPSHOT.md](FINANCE_SNAPSHOT.md).

### Fuel prices (Germany)

<img src="screens/info-fuel-prices.png" width="400" alt="The fuel-price page">

*The cheapest petrol stations around a place, priced like on the pump.* Tick **Fuel prices**, then:

1. Get a free **API key** at creativecommons.tankerkoenig.de (registration there) and paste it into the key field. It is stored on
   the frame and never shown again; leave the box empty to keep the saved one, or press **Remove**.
2. Choose the fuel (Super E5, Super E10, Diesel), the radius (1-25 km) and how many stations to show (1-5). *Hide closed*
   leaves out stations that are closed right now.
3. The **place** is the one of the weather (Overlays tab), so set it there.

**The travel time in the header** (with `route-time`): switch on *Travel time in the header*, type the two addresses, press **Find** and choose the place that is meant, then
**Check the route** - the frame takes the two places over only if it calculated a believable route between them both ways. The times of the check are offered as the
usual ones (change them if you like, or take the ones without traffic); a way that takes more than the usual time by the percentage **and** the minutes you set is drawn as a
red block with a "!". You need a free key from developer.tomtom.com or developer.here.com. The display shows only a name you choose, never the addresses.
More: [ROUTE_TIME.md](ROUTE_TIME.md).

The page shows the cheapest first (a green bar for the cheapest), each with brand, street, town, distance and price, and the
attribution the licence asks for. If this page comes up more often than every 5 minutes (few pages in the rotation and a short
schedule), **lengthen the schedule**: the service asks home-automation systems not to query more often than once in 5 minutes. More: [FUEL_PRICES.md](FUEL_PRICES.md).

### Markets (stocks, ETFs, indices, crypto, currency pairs)

<img src="screens/info-markets.png" width="400" alt="The markets page">

*Up to four symbols with the last price, the change against the day before and a line of the last 30 days.* Tick **Markets**
and type the symbols the way Yahoo writes them, separated by commas. Empty means `AAPL, EUNL.DE, ^GDAXI, BTC-EUR`.

| Kind | Examples |
| --- | --- |
| Share | `AAPL`, `MSFT`, `BRK-B` |
| Listing on another exchange | `EUNL.DE`, `VOD.L`, `SHOP.TO`, `ASML.AS` |
| Index | `^GDAXI`, `^GSPC` |
| Future | `GC=F`, `CL=F` |
| Crypto | `BTC-EUR`, `ETH-USD` |
| Currency pair | `EURUSD=X` |

- **Only the first four symbols are used.** Further ones are ignored without a message (so `SPYI.DE, ASML.AS, NOV.DE, AMZ.DE,
  FB2A.DE, MSF.DE` shows the first four). A symbol that is not made of letters, digits and `. - ^ = _` (at most 15 characters)
  is left out and does not use up one of the four places. A symbol that no source knows gets a row that says `n/a`.
- **Where the prices come from.** For each symbol the sources are tried in this order, and the first that answers wins:
  **Yahoo Finance** (no key, worldwide, but an unofficial interface that may refuse or change), then **Twelve Data** and
  **Alpha Vantage** (free personal keys, both optional). *Use Yahoo Finance* can be switched off. A source without a key, without
  an equivalent for that kind of symbol, or with its daily quota used up is skipped: Twelve Data (800 requests a day) serves
  US shares and ETFs, currency pairs and crypto; Alpha Vantage (25 a day) serves US shares and ETFs and listings with the suffixes
  `.DE .L .TO .V .BO .SS .SZ`. **Indices and futures, and listings such as `.AS`, only come from Yahoo.**
- **Keys.** Get them free at twelvedata.com and alphavantage.co and paste them into the two key fields (letters and digits, 4 to 64
  characters). They are stored on the frame and never shown again; **Remove** deletes one. The public `demo` keys are accepted
  but serve only a few symbols (Twelve Data: `AAPL`, `BTC/USD`, `EUR/USD`; Alpha Vantage: `IBM`).
- **Blue prices are the last known ones.** When no source can answer (no network on this wake, or a refused key), the frame
  shows the last good answer it kept on its storage, in blue, and the footer says `Blue = last known`. Answers older than 14
  days are dropped. The footer also names the sources the prices came from.
- **Different sources, slightly different numbers.** Daily data of different services can differ by a day or in rounding; the
  header shows the date of the newest price.

If there is no source at all (Yahoo off and no key), the page says so. More: [MARKET_QUOTES.md](MARKET_QUOTES.md).

## 5. Keys and passwords

| What | Where | How it is kept |
| --- | --- | --- |
| Login inside a calendar or to-do address | the address fields | write-only; not in a normal export |
| Tankerkoenig key | Fuel prices | write-only; **Remove** button |
| Twelve Data key, Alpha Vantage key | Markets | write-only; **Remove** buttons |

**Write-only** means the Web UI and the settings API never show the value again - only that one is saved. A normal **Export
config** leaves them out. Only the export option **Include credentials and URLs in export** writes them into the file - keep
that file private. The frame stores its settings in its flash memory, not encrypted, so if other people can reach its web page,
set the device password (General -> Advanced network settings). The frame never writes keys or logins to its log.

## 6. When something does not show up

| You see | Probably | What to do |
| --- | --- | --- |
| A page says "No network" | the frame had no WiFi at that wake | wait for the next turn; check the WiFi |
| A page says the service did not answer | the online service was down or slow | the next turn tries again |
| "No data source" on Markets | Yahoo is off and no key is saved | switch Yahoo on or enter a key |
| A markets row says `n/a` | no source knows the symbol | check the spelling (Yahoo's way); indices, futures and `.AS` need Yahoo |
| Blue prices | the newest fetch failed; these are the last known | see the footer; check the network and keys |
| Fuel page: "no key" / "no place" | the key or the weather place is missing | enter them (Fuel section, Overlays tab) |
| Fuel page shows the service's own words about the key | the key was refused | check it at creativecommons.tankerkoenig.de |
| A calendar with a password stays empty | wrong login, or a character that must be encoded | see the login rules above; the frame's log says `refused the login` |
| A page never comes up | it is not ticked, or the schedule has not fired yet | tick it under Information screens and check the schedule |
| A page is on the panel only a few seconds | an old firmware drew twice per turn | update to a build with the Agenda timer fix |
| No note `Updated ...` | the frame's clock was never set | connect it to the network once so it gets the time |

The frame's debug log (Settings -> Maintenance -> Debug Logging: switch it on, then download it) names what the servers answered. It never contains your keys or logins.

## 7. What is not included

- Pages redraw only when the schedule fires; there is no live clock.
- Calendars: no Google or Microsoft OAuth, no writing back, one task list per ToDo address, no discovery of the calendar from the
  account address.
- Fuel prices: Germany only (the Tankerkoenig service).
- Markets: up to four symbols, daily data, no live quotes; the quotes are meant for a glance at the wall, not for trading.
- Not part of this firmware: tree/bird/dinosaur "of the day" packs and a newsstand of newspaper front pages.

## Further reading

[FEATURES.md](FEATURES.md) (all options and what they need) - [INFO_SCREENS.md](INFO_SCREENS.md) - [SOURCE_AUTH.md](SOURCE_AUTH.md) -
[CALDAV.md](CALDAV.md) - [CALDAV_TODO.md](CALDAV_TODO.md) - [MULTI_UPLOAD.md](MULTI_UPLOAD.md) -
[UPLOAD_DEDUP.md](UPLOAD_DEDUP.md) - [GLYPHS.md](GLYPHS.md) - [ART_FETCH.md](ART_FETCH.md) - [CHORE_WHEEL.md](CHORE_WHEEL.md) -
[WEATHER_SCREEN.md](WEATHER_SCREEN.md) - [FACT_OF_THE_DAY.md](FACT_OF_THE_DAY.md) - [FINANCE_SNAPSHOT.md](FINANCE_SNAPSHOT.md) -
[FUEL_PRICES.md](FUEL_PRICES.md) - [MARKET_QUOTES.md](MARKET_QUOTES.md) - [RECIPES.md](RECIPES.md)
