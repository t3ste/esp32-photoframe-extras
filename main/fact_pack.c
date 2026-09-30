#include "fact_pack.h"

#include <string.h>

// ---------------------------------------------------------------------------------------------
// Parsing

// Copies [begin, end) trimmed of blanks into `dest` (of `size` bytes), cut at a character boundary.
static void copy_field(char *dest, size_t size, const char *begin, const char *end)
{
    while (begin < end && (*begin == ' ' || *begin == '\t')) {
        begin++;
    }
    while (end > begin && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) {
        end--;
    }
    size_t len = (size_t) (end - begin);
    if (len > size - 1) {
        len = size - 1;
        while (len > 0 && ((unsigned char) begin[len] & 0xC0) == 0x80) {
            len--;
        }
    }
    memcpy(dest, begin, len);
    dest[len] = '\0';
}

int fact_pack_parse(const char *text, fact_t *out, int max, int *skipped)
{
    int count = 0;
    int bad = 0;
    if (text && max > 0) {
        const char *p = text;
        if ((unsigned char) p[0] == 0xEF && (unsigned char) p[1] == 0xBB &&
            (unsigned char) p[2] == 0xBF) {
            p += 3;  // a byte order mark from an editor
        }
        while (*p != '\0') {
            const char *line_end = p;
            while (*line_end != '\0' && *line_end != '\n') {
                line_end++;
            }
            const char *a = p;
            const char *b = line_end;
            p = (*line_end == '\0') ? line_end : line_end + 1;
            while (a < b && (*a == ' ' || *a == '\t')) {
                a++;
            }
            if (a == b || *a == '#' || *a == '\r') {
                continue;
            }
            // up to three fields: title | text | question (the question keeps any further bars)
            const char *sep1 = memchr(a, '|', (size_t) (b - a));
            const char *sep2 = sep1 ? memchr(sep1 + 1, '|', (size_t) (b - sep1 - 1)) : NULL;
            fact_t fact;
            memset(&fact, 0, sizeof(fact));
            if (!sep1) {
                copy_field(fact.text, sizeof(fact.text), a, b);
            } else if (!sep2) {
                copy_field(fact.title, sizeof(fact.title), a, sep1);
                copy_field(fact.text, sizeof(fact.text), sep1 + 1, b);
            } else {
                copy_field(fact.title, sizeof(fact.title), a, sep1);
                copy_field(fact.text, sizeof(fact.text), sep1 + 1, sep2);
                copy_field(fact.question, sizeof(fact.question), sep2 + 1, b);
            }
            if (fact.text[0] == '\0' || count >= max) {
                bad++;
                continue;
            }
            out[count++] = fact;
        }
    }
    if (skipped) {
        *skipped = bad;
    }
    return count;
}

// ---------------------------------------------------------------------------------------------
// The day

long fact_day_number(int year, int month, int day)
{
    // days from civil (Howard Hinnant's algorithm)
    long y = year - (month <= 2 ? 1 : 0);
    long era = (y >= 0 ? y : y - 399) / 400;
    long yoe = y - era * 400;
    long doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

int fact_pick_index(long day_number, int count)
{
    if (count < 1) {
        return -1;
    }
    long index = day_number % count;
    return (int) (index < 0 ? index + count : index);
}

// ---------------------------------------------------------------------------------------------
// The built-in facts (this file is UTF-8). Original wording, well-known facts; the same list in
// both languages, so the fact of a day is the same fact in either.

static const fact_t BUILTIN_EN[] = {
    {"Kitchen chemistry",
     "Honey almost never spoils. Jars of edible honey have been found in ancient Egyptian tombs.",
     "Why can bacteria not live in honey?"},
    {"Ocean",
     "An octopus has three hearts and blue blood. Two hearts pump blood through the gills, the "
     "third through the rest of the body.",
     ""},
    {"Space",
     "A day on Venus is longer than its year: it turns once in about 243 Earth days but circles "
     "the Sun in about 225.",
     "Which way does Venus spin compared with Earth?"},
    {"Plants", "Botanically, bananas are berries, but strawberries are not.",
     "Which other fruits are berries?"},
    {"Paris",
     "The Eiffel Tower is up to 15 centimetres taller in summer, because iron expands when it "
     "gets warm.",
     "What happens to the tower in winter?"},
    {"Weather", "Lightning is about five times hotter than the surface of the Sun.", ""},
    {"Animals",
     "Sharks are older than trees: they have swum the oceans for more than 400 million years.", ""},
    {"Animals", "A group of flamingos is called a flamboyance.", ""},
    {"Animals", "Wombats make cube-shaped droppings.", "Why might that help them?"},
    {"History",
     "The shortest war in history, between Britain and Zanzibar in 1896, lasted less than an "
     "hour.",
     ""},
    {"Physics", "Sound travels about four times faster in water than in air.", ""},
    {"Body",
     "Babies are born with about 300 bones. An adult has 206, because many of them fuse "
     "together while we grow.",
     "Where are the smallest bones of the body?"},
    {"Space", "Sunlight needs about eight minutes and twenty seconds to reach the Earth.",
     "How far away is the Sun?"},
    {"Geography",
     "The biggest desert on Earth is not the Sahara but Antarctica: it hardly ever rains or "
     "snows there.",
     ""},
    {"History",
     "Cleopatra lived closer in time to the Moon landing than to the building of the Great "
     "Pyramid.",
     "How old are the Pyramids of Giza?"},
    {"History",
     "The University of Oxford is older than the Aztec Empire: teaching began there around "
     "1096.",
     ""},
    {"Animals", "Butterflies taste with their feet.", ""},
    {"Animals", "Sloths can hold their breath for up to 40 minutes, longer than dolphins.", ""},
    {"Weather", "The smell of rain on dry ground has a name: petrichor.", ""},
    {"Plants", "Some bamboo grows almost one metre in a single day.", ""},
    {"Animals", "Crows can recognise human faces and remember them for years.", ""},
    {"Science",
     "Water is one of the few substances that is lighter as a solid than as a liquid, so ice "
     "floats.",
     "What would happen to a lake if ice sank?"},
    {"Space", "The Moon moves away from the Earth by about four centimetres every year.", ""},
    {"Language",
     "The word quarantine comes from the Italian for forty days, the time ships had to wait "
     "before they were let into the harbour.",
     ""},
};

static const fact_t BUILTIN_DE[] = {
    {"Küchenchemie",
     "Honig verdirbt fast nie. In altägyptischen Gräbern hat man noch essbaren Honig gefunden.",
     "Warum können Bakterien in Honig nicht leben?"},
    {"Ozean",
     "Ein Oktopus hat drei Herzen und blaues Blut. Zwei Herzen pumpen Blut durch die Kiemen, "
     "das dritte durch den übrigen Körper.",
     ""},
    {"Weltall",
     "Ein Tag auf der Venus ist länger als ihr Jahr: Sie dreht sich in etwa 243 Erdentagen "
     "einmal um sich selbst, umrundet die Sonne aber in etwa 225.",
     "In welche Richtung dreht sich die Venus im Vergleich zur Erde?"},
    {"Pflanzen", "Botanisch sind Bananen Beeren, Erdbeeren dagegen nicht.",
     "Welche anderen Früchte sind Beeren?"},
    {"Paris",
     "Der Eiffelturm ist im Sommer bis zu 15 Zentimeter höher, weil sich das Eisen bei Wärme "
     "ausdehnt.",
     "Was passiert im Winter mit dem Turm?"},
    {"Wetter", "Ein Blitz ist etwa fünfmal heißer als die Oberfläche der Sonne.", ""},
    {"Tiere",
     "Haie sind älter als Bäume: Sie durchschwimmen die Meere seit mehr als 400 Millionen "
     "Jahren.",
     ""},
    {"Tiere", "Eine Gruppe Flamingos heißt auf Englisch flamboyance, also Pracht.", ""},
    {"Tiere", "Wombats machen würfelförmige Kothäufchen.", "Warum könnte ihnen das nützen?"},
    {"Geschichte",
     "Der kürzeste Krieg der Geschichte, 1896 zwischen Großbritannien und Sansibar, dauerte "
     "weniger als eine Stunde.",
     ""},
    {"Physik", "Schall breitet sich im Wasser etwa viermal schneller aus als in der Luft.", ""},
    {"Körper",
     "Babys kommen mit etwa 300 Knochen zur Welt. Erwachsene haben 206, weil viele beim "
     "Wachsen zusammenwachsen.",
     "Wo sind die kleinsten Knochen des Körpers?"},
    {"Weltall", "Das Sonnenlicht braucht etwa acht Minuten und zwanzig Sekunden bis zur Erde.",
     "Wie weit ist die Sonne entfernt?"},
    {"Erdkunde",
     "Die größte Wüste der Erde ist nicht die Sahara, sondern die Antarktis: Dort regnet und "
     "schneit es kaum.",
     ""},
    {"Geschichte",
     "Kleopatra lebte zeitlich näher an der Mondlandung als am Bau der Großen Pyramide.",
     "Wie alt sind die Pyramiden von Gizeh?"},
    {"Geschichte",
     "Die Universität Oxford ist älter als das Aztekenreich: Dort wird seit etwa 1096 "
     "gelehrt.",
     ""},
    {"Tiere", "Schmetterlinge schmecken mit den Füßen.", ""},
    {"Tiere", "Faultiere können bis zu 40 Minuten die Luft anhalten, länger als Delfine.", ""},
    {"Wetter", "Der Geruch von Regen auf trockenem Boden hat einen Namen: Petrichor.", ""},
    {"Pflanzen", "Manche Bambusarten wachsen fast einen Meter an einem einzigen Tag.", ""},
    {"Tiere", "Krähen erkennen menschliche Gesichter und merken sie sich jahrelang.", ""},
    {"Naturwissenschaft",
     "Wasser ist einer der wenigen Stoffe, die fest leichter sind als flüssig - deshalb "
     "schwimmt Eis.",
     "Was würde mit einem See passieren, wenn Eis untergehen würde?"},
    {"Weltall", "Der Mond entfernt sich jedes Jahr um etwa vier Zentimeter von der Erde.", ""},
    {"Sprache",
     "Das Wort Quarantäne kommt aus dem Italienischen und meint vierzig Tage: so lange mussten "
     "Schiffe warten, bevor sie in den Hafen durften.",
     ""},
};

_Static_assert(sizeof(BUILTIN_EN) / sizeof(BUILTIN_EN[0]) ==
                   sizeof(BUILTIN_DE) / sizeof(BUILTIN_DE[0]),
               "the built-in facts must be the same list in both languages");

int fact_builtin_count(void)
{
    return (int) (sizeof(BUILTIN_EN) / sizeof(BUILTIN_EN[0]));
}

const fact_t *fact_builtin(int index, bool german)
{
    if (index < 0 || index >= fact_builtin_count()) {
        return NULL;
    }
    return german ? &BUILTIN_DE[index] : &BUILTIN_EN[index];
}
