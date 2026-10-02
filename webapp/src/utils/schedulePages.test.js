import { describe, expect, it } from "vitest";

import { compileCard, newCard } from "./cron";
import { applyExtras, cleanHold, extrasDiffer, extrasOf } from "./schedulePages";

function timesCard(times, pages = [], hold = 0) {
  const card = newCard();
  card.mode = "times";
  card.times = times;
  card.pages = pages;
  card.hold = hold;
  return card;
}

describe("cleanHold", () => {
  it("keeps whole minutes between 0 and 240", () => {
    expect(cleanHold(15)).toBe(15);
    expect(cleanHold("30")).toBe(30);
    expect(cleanHold(0)).toBe(0);
    expect(cleanHold(240)).toBe(240);
  });
  it("rounds, clamps and treats what is no number as 0", () => {
    expect(cleanHold(14.6)).toBe(15);
    expect(cleanHold(-5)).toBe(0);
    expect(cleanHold(1000)).toBe(240);
    expect(cleanHold("")).toBe(0);
    expect(cleanHold(undefined)).toBe(0);
    expect(cleanHold(NaN)).toBe(0);
    expect(cleanHold("abc")).toBe(0);
  });
});

describe("extrasOf", () => {
  it("gives every compiled rule the pages and hold of its card", () => {
    // 07:00 and 12:15 have different minutes: two rules from one card
    const cards = [timesCard(["07:00", "12:15"], ["weather"], 60), timesCard(["18:30"], [], 0)];
    expect(cards.flatMap(compileCard).length).toBe(3);
    const { pages, holds } = extrasOf(cards, compileCard);
    expect(pages).toEqual([["weather"], ["weather"], []]);
    expect(holds).toEqual([60, 60, 0]);
  });
  it("copies the lists, so the cards are not shared with the result", () => {
    const cards = [timesCard(["07:00"], ["fuel"], 5)];
    const { pages } = extrasOf(cards, compileCard);
    pages[0].push("markets");
    expect(cards[0].pages).toEqual(["fuel"]);
  });
  it("copes with cards that never had pages or a hold", () => {
    const card = newCard();
    expect(extrasOf([card], compileCard)).toEqual({ pages: [[]], holds: [0] });
  });
});

describe("applyExtras", () => {
  it("hands the pages of each rule to its card, in order", () => {
    const cards = [timesCard(["07:00", "12:15"]), timesCard(["18:30"])];
    applyExtras(cards, [["weather"], ["weather"], ["fuel", "markets"]], [60, 60, 15], compileCard);
    expect(cards[0].pages).toEqual(["weather"]);
    expect(cards[0].hold).toBe(60);
    expect(cards[1].pages).toEqual(["fuel", "markets"]);
    expect(cards[1].hold).toBe(15);
  });
  it("gives nothing to the cards the device sent nothing for", () => {
    const cards = [timesCard(["07:00"]), timesCard(["18:30"])];
    applyExtras(cards, [["fuel"]], [10], compileCard);
    expect(cards[1].pages).toEqual([]);
    expect(cards[1].hold).toBe(0);
  });
  it("cleans a hold that is out of range", () => {
    const cards = [timesCard(["07:00"])];
    applyExtras(cards, [[]], [9999], compileCard);
    expect(cards[0].hold).toBe(240);
  });
});

describe("extrasDiffer", () => {
  it("is false when the cards already hold what the device sent", () => {
    const cards = [timesCard(["07:00"], ["fuel"], 30)];
    expect(extrasDiffer(cards, [["fuel"]], [30], compileCard)).toBe(false);
  });
  it("is true for other pages or another hold", () => {
    const cards = [timesCard(["07:00"], ["fuel"], 30)];
    expect(extrasDiffer(cards, [["markets"]], [30], compileCard)).toBe(true);
    expect(extrasDiffer(cards, [["fuel"]], [45], compileCard)).toBe(true);
  });
  it("treats missing entries as no pages and no hold", () => {
    const cards = [timesCard(["07:00"], [], 0), timesCard(["08:00"], [], 0)];
    expect(extrasDiffer(cards, [], [], compileCard)).toBe(false);
    expect(extrasDiffer(cards, undefined, undefined, compileCard)).toBe(false);
    expect(extrasDiffer(cards, [[], ["fuel"]], [0, 0], compileCard)).toBe(true);
  });
});
