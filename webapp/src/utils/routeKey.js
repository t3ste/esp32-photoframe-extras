// The shape of a personal API key of TomTom or HERE (build option route-time): letters, digits, "-"
// and "_", 16 to 104 - what the frame accepts (route_key_valid() in main/route_time.c). Anything else
// would never be sent, so the form says so while it is typed.
export function isRouteKey(value) {
  return typeof value === "string" && /^[A-Za-z0-9_-]{16,104}$/.test(value);
}
