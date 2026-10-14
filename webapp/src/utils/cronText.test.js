import { describe, expect, it } from "vitest";
import { newCard, parseCron } from "./cron";
import { describeCardExact, describeRule } from "./cronText";

describe("describeRule: what a rule really does", () => {
  it("does not call */45 'every 45 minutes' - it restarts every hour", () => {
    expect(describeRule("*/45 * *")).toBe("at :00, :45 of every hour, every day");
  });

  it("says that 45 * * is once an hour", () => {
    expect(describeRule("45 * *")).toBe("every hour, at :45, every day");
  });

  it("keeps 'every n minutes' for the n that divide the hour", () => {
    expect(describeRule("*/5 * *")).toBe("every 5 minutes, every day");
    expect(describeRule("*/15 * *")).toBe("every 15 minutes, every day");
    expect(describeRule("*/30 * *")).toBe("every 30 minutes, every day");
    expect(describeRule("* * *")).toBe("every minute, every day");
  });

  it("limits to the hours of a window", () => {
    expect(describeRule("*/5 6-22 1-5")).toBe("every 5 minutes, from 06:00 to 22:59, weekdays");
    expect(describeRule("*/45 8-18 *")).toBe(
      "at :00, :45 of every hour, from 08:00 to 18:59, every day"
    );
  });

  it("says every n hours only where the gaps are all n, also across midnight", () => {
    expect(describeRule("0 */4 *")).toBe("every 4 hours, every day");
    expect(describeRule("0 */6 *")).toBe("every 6 hours, every day");
    expect(describeRule("30 */2 *")).toBe("every 2 hours, at :30, every day");
    // 24 is not a multiple of 5: 20:00 is followed by 00:00 after four hours
    expect(describeRule("0 */5 *")).toBe("at 00:00, 05:00, 10:00, 15:00, 20:00, every day");
    expect(describeRule("0 */7 *")).toBe("at 00:00, 07:00, 14:00, 21:00, every day");
    expect(describeRule("0 */12 *")).toBe("at 00:00, 12:00, every day");
  });

  it("names hourly rules", () => {
    expect(describeRule("0 * *")).toBe("every hour, on the hour, every day");
    expect(describeRule("0 6-22 *")).toBe("every hour from 06:00 to 22:00, every day");
    expect(describeRule("0 8-20/4 *")).toBe("at 08:00, 12:00, 16:00, 20:00, every day");
  });

  it("lists fixed times and days", () => {
    expect(describeRule("30 7 1-5")).toBe("at 07:30, weekdays");
    expect(describeRule("0 8,18 0,6")).toBe("at 08:00, 18:00, weekends");
    expect(describeRule("15 9 1,3,5")).toBe("at 09:15, Mon, Wed, Fri");
    expect(describeRule("0 0 7")).toBe("at 00:00, Sun");
  });

  it("answers null for what is not a rule", () => {
    expect(describeRule("nonsense")).toBeNull();
    expect(describeRule("*/0 * *")).toBeNull();
    expect(describeRule("")).toBeNull();
  });

  // The summary must not claim a regular gap that the rule does not have: every "every n minutes / hours" is checked
  // against the minutes the rule fires at over two days.
  it("never claims a gap that is not there", () => {
    const rules = [];
    for (const m of ["*", "*/2", "*/7", "*/10", "*/45", "0", "5", "0,30", "15-45/15"])
      for (const h of ["*", "*/3", "*/5", "*/12", "0", "6-22", "8-20/4", "9,17"])
        rules.push(`${m} ${h} *`);
    let claims = 0;
    for (const expr of rules) {
      const text = describeRule(expr);
      const rule = parseCron(expr);
      const fires = [];
      for (let day = 0; day < 3; day++)
        for (let h = 0; h < 24; h++)
          for (let m = 0; m < 60; m++)
            if (rule.hour.has(h) && rule.minute.has(m)) fires.push(day * 1440 + h * 60 + m);
      const claim = /^every (\d+) (minutes|hours)/.exec(text);
      if (!claim) continue;
      claims++;
      const gap = Number(claim[1]) * (claim[2] === "hours" ? 60 : 1);
      for (let i = 1; i < fires.length; i++) {
        const d = fires[i] - fires[i - 1];
        // nothing fires closer than the claim says, and a gap up to the claim is exactly the claim
        expect(d, `${expr}: ${text}`).toBeGreaterThanOrEqual(gap);
        if (claim[2] === "hours" || rule.hour.size === 24) expect(d, `${expr}: ${text}`).toBe(gap);
      }
    }
    expect(claims).toBeGreaterThan(5);
  });
});

describe("describeCardExact: the summary under a builder card", () => {
  it("turns the builder's 45 minutes into what it fires at", () => {
    const card = { ...newCard(), mode: "interval", unit: "minutes", every: 45 };
    expect(describeCardExact(card)).toBe("Rotate at :00, :45 of every hour, every day");
  });

  it("keeps the usual wording for the usual cards", () => {
    expect(describeCardExact({ ...newCard(), mode: "interval", unit: "hours", every: 4 })).toBe(
      "Rotate every 4 hours, every day"
    );
    expect(describeCardExact({ ...newCard(), mode: "times", times: ["08:00", "18:00"] })).toBe(
      "Rotate at 08:00, 18:00, every day"
    );
  });

  it("explains a custom rule instead of only repeating it", () => {
    expect(describeCardExact({ ...newCard(), raw: "45 * *" })).toBe(
      "Custom (45 * *): rotate every hour, at :45, every day"
    );
    expect(describeCardExact({ ...newCard(), raw: "bad" })).toBe("Custom: bad");
  });

  it("uses the verb it is given", () => {
    expect(describeCardExact({ ...newCard(), mode: "times", times: ["06:30"] }, "Draw")).toBe(
      "Draw at 06:30, every day"
    );
  });
});
