// What a schedule rule really does, in words (build option `fixes`): the summary under a rule of the rotation / Agenda schedule editor.
//
// The builder's summary said "Rotate every 45 minutes" for a rule that fires at :00 and :45 of every hour: a cron step restarts with
// every hour (and every day), so "every n minutes" is only true when n divides 60, and "every n hours" only when n divides 24. The text
// here is worked out from the minutes, hours and weekdays the rule hits, so it cannot say more than the rule does.
import { DAY_LABELS, compileCard, parseCron } from "./cron";

const pad = (n) => String(n).padStart(2, "0");
const sorted = (set) => [...set].sort((a, b) => a - b);

// the common difference of a list, 0 when it has none (or fewer than two entries)
function stepOf(list) {
  if (list.length < 2) return 0;
  const step = list[1] - list[0];
  for (let i = 2; i < list.length; i++) if (list[i] - list[i - 1] !== step) return 0;
  return step;
}

function daysText(dow) {
  const days = sorted(dow);
  if (days.length === 7) return "every day";
  if (days.join() === "1,2,3,4,5") return "weekdays";
  if (days.join() === "0,6") return "weekends";
  return days.map((d) => DAY_LABELS[d]).join(", ");
}

// the hours the rule runs in: "" for all day, "from 06:00 to 22:59", or the list
function windowText(hours) {
  if (hours.length === 24) return "";
  const contiguous = hours.length === hours[hours.length - 1] - hours[0] + 1;
  if (contiguous) return `from ${pad(hours[0])}:00 to ${pad(hours[hours.length - 1])}:59`;
  return `in the hours ${hours.map(pad).join(", ")}`;
}

function timesText(minutes, hours) {
  const M = sorted(minutes);
  const H = sorted(hours);
  const window = windowText(H);

  if (M.length === 1) {
    const hStep = stepOf(H);
    const at = M[0] === 0 ? "" : `, at :${pad(M[0])}`;
    if (H.length === 24)
      return M[0] === 0 ? "every hour, on the hour" : `every hour, at :${pad(M[0])}`;
    // "every n hours" only when the gap is n all day round, the one over midnight included
    if (hStep && 24 % hStep === 0 && H.length * hStep === 24 && H.length >= 3)
      return `every ${hStep} hours${at}`;
    // a few fixed times: say them, nothing to misread
    if (H.length <= 8) return `at ${H.map((h) => `${pad(h)}:${pad(M[0])}`).join(", ")}`;
    if (hStep === 1)
      return `every hour from ${pad(H[0])}:${pad(M[0])} to ${pad(H[H.length - 1])}:${pad(M[0])}`;
    return `at ${H.map((h) => `${pad(h)}:${pad(M[0])}`).join(", ")}`;
  }
  if (M.length * H.length <= 8)
    return `at ${H.flatMap((h) => M.map((m) => `${pad(h)}:${pad(m)}`)).join(", ")}`;

  const tail = window ? `, ${window}` : "";
  const mStep = M.length === 60 ? 1 : stepOf(M);
  // every minute, or every n minutes when n divides the hour; otherwise the steps restart with the hour
  if (M.length === 60) return `every minute${tail}`;
  if (mStep && M[0] === 0 && 60 % mStep === 0 && M.length === 60 / mStep)
    return `every ${mStep} minutes${tail}`;
  if (M.length <= 6) return `at ${M.map((m) => `:${pad(m)}`).join(", ")} of every hour${tail}`;
  return `at minutes ${M.join(", ")} of every hour${tail}`;
}

// A summary of one rule ("minute hour weekday"), or null when it is not a valid rule.
export function describeRule(expr) {
  const rule = parseCron(expr);
  if (!rule) return null;
  return `${timesText(rule.minute, rule.hour)}, ${daysText(rule.dow)}`;
}

// The summary under a card of the builder: what its rule(s) do, starting with `verb` as the editor's own text did.
export function describeCardExact(card, verb = "Rotate") {
  const custom = card.raw !== null && card.raw !== undefined;
  const rules = custom ? [card.raw.trim()] : compileCard(card);
  const texts = rules.map(describeRule);
  if (!rules.length || texts.some((t) => t === null)) return `Custom: ${rules.join("; ")}`;
  if (custom) return `Custom (${rules[0]}): ${verb.toLowerCase()} ${texts[0]}`;
  return `${verb} ${texts.join("; ")}`;
}
