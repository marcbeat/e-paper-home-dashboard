const SHARED_TOKEN = "REPLACE_WITH_LONG_RANDOM_TOKEN";

function formatEvent(ev) {
  const allDay = ev.isAllDayEvent();
  const start = ev.getStartTime();
  const hh = String(start.getHours()).padStart(2, "0");
  const mm = String(start.getMinutes()).padStart(2, "0");
  return {
    time: allDay ? "" : `${hh}:${mm}`,
    title: ev.getTitle() || "(Ohne Titel)",
    allDay: allDay,
  };
}

function splitEventsForTodayAndTomorrow(calId, from, todayEnd, tomorrowStart, tomorrowEnd) {
  if (!calId) return { today: [], tomorrow: [] };
  const cal = CalendarApp.getCalendarById(calId);
  if (!cal) return { today: [], tomorrow: [] };

  const today = [];
  const tomorrow = [];
  for (const ev of cal.getEvents(from, tomorrowEnd)) {
    const start = ev.getStartTime();
    const end = ev.getEndTime();
    const item = formatEvent(ev);
    if (start <= todayEnd && (end > from || (!item.allDay && start >= from))) {
      today.push(item);
    }
    if (start <= tomorrowEnd && (end > tomorrowStart || (!item.allDay && start >= tomorrowStart))) {
      tomorrow.push(item);
    }
  }
  return { today, tomorrow };
}

function doGet(e) {
  if (!e || e.parameter.token !== SHARED_TOKEN) {
    return ContentService.createTextOutput(JSON.stringify({ error: "unauthorized" }))
      .setMimeType(ContentService.MimeType.JSON);
  }
  const parsed = e.parameter.datetime ? new Date(e.parameter.datetime) : new Date();
  const from = isNaN(parsed.getTime()) ? new Date() : parsed;
  const y = from.getFullYear();
  const m = from.getMonth();
  const d = from.getDate();
  const todayEnd      = new Date(y, m, d, 23, 59, 59, 999);
  const tomorrowStart = new Date(y, m, d + 1, 0, 0, 0, 0);
  const tomorrowEnd   = new Date(y, m, d + 1, 23, 59, 59, 999);

  const payload = {
    cal1: splitEventsForTodayAndTomorrow(e.parameter.cal1 || "", from, todayEnd, tomorrowStart, tomorrowEnd),
    cal2: splitEventsForTodayAndTomorrow(e.parameter.cal2 || "", from, todayEnd, tomorrowStart, tomorrowEnd),
  };
  return ContentService.createTextOutput(JSON.stringify(payload))
    .setMimeType(ContentService.MimeType.JSON);
}