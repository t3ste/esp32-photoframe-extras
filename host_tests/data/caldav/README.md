Answers of a Radicale server (default configuration) to the calendar-query the frame sends
(`caldav_build_report_body()`), for a synthetic calendar - fixed window 2026-10-01 .. 2026-10-04 (UTC):

- `radicale-report-expand.xml` - with `<c:expand>`: repeating events already come as single events
  (a weekly `BYDAY=MO,TH`, a daily rule with an `EXDATE`, a monthly rule).
- `radicale-report-plain.xml` - without it: the master events with their `RRULE`s.

Used by `CalendarIcsCaldavServerAnswers` in `../../test_calendar_ics.cpp`. The events are made up.
