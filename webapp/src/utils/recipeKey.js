// The shape of a personal key of TheMealDB (build option recipes): letters and digits, 1 to 24 - what
// the frame accepts (recipe_mealdb_key_valid() in main/recipe_source.c). Anything else would never be
// sent, so the form says so while it is typed.
export function isMealDbKey(value) {
  return typeof value === "string" && /^[A-Za-z0-9]{1,24}$/.test(value);
}
