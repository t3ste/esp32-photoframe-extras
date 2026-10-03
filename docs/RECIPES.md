# Recipe page

> **Build option:** compiled in only with `python build.py --with recipes` (needs `info-screens`; the build pulls it in with what it needs), part of
> the `extras` bundle and of every full build. Without it the firmware is the upstream firmware (see [FEATURES.md](FEATURES.md)).
>
> **Status:** STATUS_PLACEHOLDER

A page for the [information screens](INFO_SCREENS.md) that shows **one cooking recipe with its picture**: the title in red, the category and the time in blue,
the ingredients on the left with bullets, the preparation on the right in paragraphs, and the picture at the top right. The page is drawn for the display
orientation you set (Settings -> General): in **landscape** the ingredients are at the left and the preparation at the right, in **portrait** the
ingredients and the picture are on top and the preparation runs over the full width below them.

<img src="screens/info-recipe.png" width="480" alt="The recipe page in landscape: a red title, the category and time in blue, the ingredients with bullets on the left, the preparation in paragraphs on the right, the picture at the top right and a small QR code at the bottom right">

*Invented sample recipe with a made-up picture, drawn by the firmware's own code - [more pictures](SCREENSHOTS.md).*

## Settings

Settings -> Agenda -> Information screens -> tick **Recipe**:

| Setting | Default | What it does |
| --- | --- | --- |
| Source | Chefkoch - recipe of the day | Where the recipe comes from (see below) |
| Recipe of the day | classic | *classic*, *vegetarian* or *vegan*: which of the three recipes of the day |
| Search words | empty | Words Chefkoch is asked for (search source) |
| Category, country or cuisine, type of meal, diet, property | no filter | Filters of the search source; "no filter" is the default |
| Order | recommended | *recommended*, *best rated* or *newest* |
| Preparation time | no limit | Up to 15 / 30 minutes, 1 / 2 hours of **preparation** (Chefkoch's working time - the time the header shows is the total time, with cooking and resting, so it can be longer) |
| Rating | any | 2, 3, 4 stars and more, or "top" (4.5 and more) |
| TheMealDB category | any | Category of the English source |
| TheMealDB key | empty | Optional personal key, **write-only** (see below) |
| Show the picture | on | Off: no picture is loaded and recipes without a picture are not skipped |
| QR code of the recipe's address | off | A small QR code at the bottom right that opens the recipe on the source's site |

With every filter at "no filter" the search source shows a **random recipe of the first thousand** of the chosen order - the part of Chefkoch's list that
its interface lets a program read.

## Sources

| Source | Language | What it gives |
| --- | --- | --- |
| **Chefkoch - recipe of the day** | German | One of the three recipes of the day of chefkoch.de (classic, vegetarian, vegan) - the same all day, however often the page is drawn. If today's has no picture or does not fit, the frame takes the same kind from the days before (as many as the website lists) |
| **Chefkoch - search** | German | A recipe from the search of chefkoch.de with your filters. The **preparation time**, the **rating** and the **order** are asked of Chefkoch itself; **category, country, type of meal, diet and property** are added to the search words, so they narrow the choice but are not exact. *Easy* is checked by the frame (the recipe's difficulty), *quick* means up to 30 minutes of preparation |
| **TheMealDB** | English | A recipe of the chosen category of themealdb.com, or a random one |

The **language of the labels follows the source** (a German recipe says ZUTATEN / ZUBEREITUNG / Quelle, an English one INGREDIENTS / PREPARATION / Source),
not the language of the frame.

**Chefkoch and its terms.** The frame reads the website's page of the recipes of the day and its application interface, and says in every request who it is
(the user agent names the project). That interface is not documented for third parties, and no terms for it were found when this was written (the website's
robots.txt allows these requests); it can change or be closed at any time, and then this source stops working (the frame falls back to the last recipe, see below). The recipes and pictures belong to
chefkoch.de and its authors: the frame shows them **for you, at home**, and never stores them for anything but showing them again; do not publish them.
The page names the source ("Quelle: Chefkoch - Rezept des Tages"), and the QR code leads to the recipe on the site.

**TheMealDB.** The frame uses TheMealDB's free interface with the **development key "1"**, which the site offers for development and learning. If you
want to use the page long or publicly, get a key of your own from themealdb.com and enter it in the card (write-only, stored on the frame; it is in an export only
if the credentials are included). Attribution: recipe data and imagery are TheMealDB's; the page says "Source: TheMealDB".

## How a recipe is chosen - and what happens when it fails

1. **Three tries with exactly your filters.** A try asks the source, takes up to six recipes in random order, and the first that **has a picture** (when pictures are on),
   **fits the page without being cut** at the smallest size (12 px text), and **has not been shown lately** (the frame remembers the last 20; the recipe of the
   day is exempt - it is meant to stay the same all day) is the recipe.
2. **Only if the source answered but nothing usable came out**, the filters are **relaxed step by step** and asked once more each: first the type of meal,
   the diet and the property are dropped, then category and country, and last everything (the search words, the time and the rating too). The page then
   says "Filter gelockert" / "Filters relaxed". A failing network never relaxes anything.
3. **If nothing is found at all** (also when there is no network): the **last recipe that was fetched** is shown again with a warning
   ("Letztes Rezept" / "Last recipe", and "Offline" without a network). If there never was one, **nothing is drawn** and the page is skipped (the
   other pages of the rotation take its turn); if it is the only page, a short message ("No recipe could be loaded") is shown instead.
4. A try takes a few seconds; after about **110 seconds** no further try is made.

The frame keeps the last recipe and its picture in its storage (`.recipe_last.bin`, `.recipe_last.jpg`) and the list of recent ones in `.recipe_seen.txt`; they
are hidden files and never uploaded anywhere.

## What the page looks like

- **Header:** the title (red, as large as fits), below it the category and the time (blue; for Chefkoch the path below the root of its category tree, e.g.
  "Suppen / Gebundene"), at the right the source; a yellow rule; small warnings if there are any
  (no picture, text cut, last recipe, filters relaxed, offline).
- **Text size:** the page uses **the largest text size at which everything fits** - the ingredients and the preparation each have their own size, from 24 px down
  to 12 px (on large panels the same steps scale up). A recipe that does not fit even at the smallest size is **not shown** (another is taken); only the
  last recipe, shown as a fallback, may be cut, with "..." at the cut and the warning "Text gekürzt" / "Text cut".
- **Text never overlaps** the picture or the QR code: next to them the lines are narrower.
- **Characters and step numbers are cleaned**: HTML, entities and characters the font has no glyph for are removed or replaced, a step number that stands alone on
  a line ("1.", "STEP 2") is joined with the text after it, and the preparation is split into readable paragraphs.
- **Colors:** the page uses the panel's colors - title, bullets and warnings in red, the category in blue, the rule in yellow, the text in black; the picture is
  dithered to the palette (to sixteen grays on a grayscale panel). The picture is cropped to fill its frame.
- **Font:** Noto Sans (SIL Open Font License, see [third_party/NotoSans-OFL.txt](third_party/NotoSans-OFL.txt)), compiled in as bitmaps for the sizes 12 to 30 px
  (regular, and bold for the headings), with the characters of Windows-1252 (German and the Western European languages).

On small panels the text is correspondingly small; on a 7.3" panel (800 x 480) the usual recipe is drawn at 18 - 20 px.

## The picture and the QR code

- The picture is loaded as a small JPEG (Chefkoch: 240 x 160, 360 x 240 or 642 x 428, whichever the room on the page needs; TheMealDB: 200 or 350 px), decoded on
  the frame and dithered.
- The QR code holds the recipe's short address (`https://www.chefkoch.de/rezepte/<id>/`, or the recipe's page at TheMealDB). It is small on purpose
  (version up to 6, low error correction) - a phone reads it from the panel at arm's length. It uses the room at the bottom right, and the text keeps out of it.

## Privacy

Every time the page is drawn the frame makes **two to a dozen HTTPS requests** to chefkoch.de (and its picture server) or themealdb.com. The sites see the
frame's IP address and, for the search source, your search words and filters. The frame **never logs** the requests' addresses (they contain the search
words and the key). Nothing is sent to anyone else.

## Settings in the API

`GET/PATCH /api/config`: `recipe_source` (`day`, `search`, `mealdb`), `recipe_variant` (`classic`, `vegetarian`, `vegan`), `recipe_query` (up to 47 characters),
`recipe_property`, `recipe_health`, `recipe_category`, `recipe_country`, `recipe_meal` (the German words of the lists in the Web UI, `""` = no filter),
`recipe_max_minutes` (0, 15, 30, 60, 120), `recipe_min_rating` (tenths of a star: 0, 20, 30, 40, 45), `recipe_sort` (`recommended`, `rating`, `newest`),
`recipe_mealdb_category` (English, `""` = any), `recipe_image`, `recipe_qr` and the key `recipe_mealdb_key` (write-only: `GET` answers only
`recipe_mealdb_key_configured`; `recipe_mealdb_key_clear: true` removes it; letters and digits, up to 24). A value that is not in the lists is ignored.
The page is added to the rotation with `"recipe"` in `info_screens`.

## Files and tests

- Code: `main/recipe_*.c` (fonts, text cleaning, sources, layout, QR code, engine, device glue) and `main/screen_recipe.c`; the page id `recipe` in `main/info_screens.c`.
- Host tests (`make test`): `recipe_text_test`, `recipe_source_test`, `recipe_layout_test`, `recipe_engine_test` - 153 tests with invented sample answers in
  `host_tests/data/recipe/`; they also run under AddressSanitizer / UBSan (see [MAINTAINING.md](MAINTAINING.md)).
- `host_tests/render_recipe.cpp` draws the page for the panels of all boards, both orientations, into PNG files without a frame (that is how the pictures in these
  docs are made).
- `scripts/gen_recipe_font.py` makes `main/recipe_font_data.c` from the Noto Sans files (not included in the repository: download them from the Noto project).

## If something does not work

The frame's log has one line per drawing: `recipe_service: Recipe: a new one (1 try, filters as set, 3 requests)`. "2 tries"
or "relaxed" says the filters were too narrow; `the last one` says nothing new could be had; `none` says there was nothing at all. `No recipe: this page is
skipped` means the rotation went on to the next page. If every drawing ends with the last recipe, the source may have changed its interface - report
it with the log lines of the drawing.
