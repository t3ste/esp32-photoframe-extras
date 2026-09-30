Answers of a Radicale server (default configuration) to the calendar-query the frame sends
(`caldav_build_report_body()`), for a synthetic calendar - fixed window 2026-10-01 .. 2026-10-04 (UTC):

- `radicale-report-expand.xml` - with `<c:expand>`: repeating events already come as single events
  (a weekly `BYDAY=MO,TH`, a daily rule with an `EXDATE`, a monthly rule).
- `radicale-report-plain.xml` - without it: the master events with their `RRULE`s.

Used by `CalendarIcsCaldavServerAnswers` in `../../test_calendar_ics.cpp`. The events are made up.

Task list (`radicale-todo-open.xml`, `radicale-todo-all.xml`): the answers to the frame's to-do query
(`caldav_build_todo_report_body()`), with the filter for open to-dos and without it (the second one also
carries the finished to-do). Used by `TodoCaldav` in `../../test_todo.cpp`. The to-dos are made up: a
dated and prioritised one, one with a UTC due time, a finished one, a cancelled one, a repeating one, one
with umlauts and a folded line, one with an alarm, one without a date.
