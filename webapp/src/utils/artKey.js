// The shape of a personal Smithsonian API key (build option artworks): letters, digits and "_",
// 4 to 64 - what the frame accepts (art_si_key_valid() in main/art_sources.c). Anything else would
// never be sent, so the form says so while it is typed.
export function isArtKey(value) {
  return typeof value === "string" && /^[A-Za-z0-9_]{4,64}$/.test(value);
}

// An album name is a folder name on the frame: letters, digits, blank, "-" and "_", 1 to 31
// characters, no blank at either end (config_manager_set_art_album() in main/config_manager.c).
export function isArtAlbumName(value) {
  return (
    typeof value === "string" && /^[A-Za-z0-9_-]([A-Za-z0-9 _-]{0,29}[A-Za-z0-9_-])?$/.test(value)
  );
}
