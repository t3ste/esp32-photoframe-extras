// Pages and hold times per Agenda schedule (build option schedule-pages). The device keeps them per
// compiled cron rule, in the order of the rules; the editor keeps them per card, and one card can
// compile to several rules (specific times with different minutes), all carrying the card's own.

// A hold in whole minutes, 0-240 (0: the common minimum time between two displays).
export function cleanHold(value) {
  const minutes = Math.round(Number(value));
  return Number.isFinite(minutes) ? Math.max(0, Math.min(240, minutes)) : 0;
}

// The pages and holds of every compiled rule of the cards: { pages: [[name]], holds: [minutes] }.
export function extrasOf(cards, compileCard) {
  const pages = [];
  const holds = [];
  for (const card of cards) {
    const count = compileCard(card).length;
    for (let i = 0; i < count; i++) {
      pages.push([...(card.pages || [])]);
      holds.push(cleanHold(card.hold));
    }
  }
  return { pages, holds };
}

// Gives the cards the pages and holds of the rules; a card takes those of its first rule.
export function applyExtras(cards, pages, holds, compileCard) {
  let rule = 0;
  for (const card of cards) {
    card.pages = [...((pages && pages[rule]) || [])];
    card.hold = cleanHold(holds && holds[rule]);
    rule += compileCard(card).length;
  }
}

// Whether the pages and holds the device sent differ from what the cards hold now.
export function extrasDiffer(cards, pages, holds, compileCard) {
  const now = extrasOf(cards, compileCard);
  const wantPages = (pages || []).slice(0, now.pages.length).map((p) => [...(p || [])]);
  const wantHolds = (holds || []).slice(0, now.holds.length).map(cleanHold);
  // rules the device sent nothing for count as "no pages, no hold"
  while (wantPages.length < now.pages.length) wantPages.push([]);
  while (wantHolds.length < now.holds.length) wantHolds.push(0);
  return (
    JSON.stringify(now.pages) !== JSON.stringify(wantPages) ||
    JSON.stringify(now.holds) !== JSON.stringify(wantHolds)
  );
}
