import { defineStore } from "pinia";
import { ref, computed } from "vue";

export const useAppStore = defineStore("app", () => {
  // State
  const albums = ref([]);
  const selectedAlbum = ref("Default");
  const images = ref([]);
  const battery = ref({
    connected: false,
    level: 0,
    voltage: 0,
    charging: false,
  });
  const systemInfo = ref({
    width: 800,
    height: 480,
    has_sdcard: false,
    sdcard_inserted: false,
    has_flash_storage: false,
    storage_total: 0,
    storage_used: 0,
    version: "v1.0",
    project_name: "PhotoFrame",
    wakeup_key_name: "wake button",
    compile_time: "",
    compile_date: "",
    idf_version: "",
    board_name: "waveshare_photopainter_73",
    last_crash: null,
  });
  const loading = ref({
    albums: false,
    images: false,
    // #if FORK_FIXES
    moreImages: false,
    // #endif
    battery: false,
    systemInfo: false,
  });
  // #if FORK_FIXES
  // Server-side pagination for /api/images - see main/http_server.c's
  // album_images_handler() doc comment: a large album (facecrop sidecars
  // triple the real directory-entry count per photo) can make a single
  // "list everything" request hang outright, since it forces the device to
  // walk the whole, possibly huge, SD card directory in one HTTP request
  // (confirmed live 2026-09-29). Fetch bounded pages instead -
  // GALLERY_PAGE_SIZE also drives AlbumGallery.vue's "Load more" button, so
  // one click fetches exactly one more page.
  const GALLERY_PAGE_SIZE = 60;
  const imagesHasMore = ref(false);
  const imagesOffset = ref(0);
  // #endif

  // API base URL (empty for same-origin)
  const API_BASE = "";

  // Getters
  const sortedAlbums = computed(() => {
    return [...albums.value].sort((a, b) => {
      if (a.name === "Default") return -1;
      if (b.name === "Default") return 1;
      return a.name.localeCompare(b.name);
    });
  });

  const currentAlbumImages = computed(() => images.value);

  // Grayscale panels ("gc16", and any future "gc8"/"gc4") hide the color-only
  // controls (saturation, 6-color palette calibration).
  const isGrayscale = computed(() => (systemInfo.value.display_type || "").startsWith("gc"));

  // Actions
  async function loadBatteryStatus() {
    try {
      const response = await fetch(`${API_BASE}/api/battery`);
      if (!response.ok || response.headers.get("content-type")?.includes("text/html")) {
        return;
      }
      const data = await response.json();
      battery.value = {
        connected: data.battery_connected,
        level: data.battery_level,
        voltage: data.battery_voltage,
        charging: data.charging,
      };
    } catch (_error) {
      console.log("Battery API not available (standalone mode)");
    }
  }

  async function loadAlbums() {
    loading.value.albums = true;
    try {
      if (!systemInfo.value.sdcard_inserted && !systemInfo.value.has_flash_storage) {
        albums.value = [{ name: "Default", enabled: true, image_count: 0 }];
        return;
      }
      const response = await fetch(`${API_BASE}/api/albums`);
      if (!response.ok || response.headers.get("content-type")?.includes("text/html")) {
        albums.value = [{ name: "Default", enabled: true, image_count: 0 }];
        return;
      }
      albums.value = await response.json();
    } catch (error) {
      console.log("Failed to load albums (standalone mode):", error);
      albums.value = [{ name: "Default", enabled: true, image_count: 0 }];
    } finally {
      loading.value.albums = false;
    }
  }

  // #if FORK_FIXES
  // `append: true` fetches the next page onto the end of the current list
  // (AlbumGallery.vue's "Load more") instead of replacing it - see the
  // GALLERY_PAGE_SIZE comment above for why this fetches a bounded page at
  // all instead of the whole album in one request.
  async function loadImages(albumName, { append = false } = {}) {
    loading.value[append ? "moreImages" : "images"] = true;
    try {
      if (!systemInfo.value.sdcard_inserted && !systemInfo.value.has_flash_storage) {
        images.value = [];
        imagesHasMore.value = false;
        return;
      }
      // Mirrors AlbumGallery.vue's "Show thumbnails" preference (same
      // localStorage key) so the device can skip its per-file thumbnail
      // existence check entirely when the client won't render any anyway.
      const showThumbnails = localStorage.getItem("photoframe_show_thumbnails") === "true";
      const offset = append ? imagesOffset.value : 0;
      const response = await fetch(
        `${API_BASE}/api/images?album=${encodeURIComponent(albumName)}&thumbnails=${showThumbnails ? 1 : 0}&offset=${offset}&limit=${GALLERY_PAGE_SIZE}`
      );
      if (!response.ok || response.headers.get("content-type")?.includes("text/html")) {
        if (!append) {
          images.value = [];
          imagesHasMore.value = false;
        }
        return;
      }
      const data = await response.json();
      const page = Array.isArray(data) ? data : data.images; // tolerate a plain-array response too
      images.value = append ? images.value.concat(page) : page;
      imagesOffset.value = offset + page.length;
      imagesHasMore.value = Array.isArray(data) ? false : Boolean(data.has_more);
    } catch (error) {
      console.log("Failed to load images (standalone mode):", error);
      if (!append) {
        images.value = [];
        imagesHasMore.value = false;
      }
    } finally {
      loading.value[append ? "moreImages" : "images"] = false;
    }
  }
  // #else
  async function loadImages(albumName) {
    loading.value.images = true;
    try {
      if (!systemInfo.value.sdcard_inserted && !systemInfo.value.has_flash_storage) {
        images.value = [];
        return;
      }
      const response = await fetch(`${API_BASE}/api/images?album=${encodeURIComponent(albumName)}`);
      if (!response.ok || response.headers.get("content-type")?.includes("text/html")) {
        images.value = [];
        return;
      }
      images.value = await response.json();
    } catch (error) {
      console.log("Failed to load images (standalone mode):", error);
      images.value = [];
    } finally {
      loading.value.images = false;
    }
  }
  // #endif

  function selectAlbum(albumName) {
    selectedAlbum.value = albumName;
    loadImages(albumName);
  }

  async function toggleAlbumEnabled(albumName, enabled) {
    try {
      const response = await fetch(
        `${API_BASE}/api/albums/enabled?name=${encodeURIComponent(albumName)}`,
        {
          method: "PUT",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ enabled }),
        }
      );
      if (response.ok) {
        await loadAlbums();
      }
    } catch (error) {
      console.error("Failed to toggle album:", error);
    }
  }

  async function createAlbum(name) {
    try {
      const response = await fetch(`${API_BASE}/api/albums`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ name: name.trim() }),
      });
      if (response.ok) {
        await loadAlbums();
        return true;
      }
      return false;
    } catch (error) {
      console.error("Failed to create album:", error);
      return false;
    }
  }

  async function deleteAlbum(albumName) {
    try {
      const response = await fetch(`${API_BASE}/api/albums?name=${encodeURIComponent(albumName)}`, {
        method: "DELETE",
      });
      if (response.ok) {
        await loadAlbums();
        await loadSystemInfo();
        if (selectedAlbum.value === albumName) {
          selectAlbum("Default");
        }
        return true;
      }
      return false;
    } catch (error) {
      console.error("Failed to delete album:", error);
      return false;
    }
  }

  async function deleteImage(album, filename) {
    try {
      const fullPath = `${album}/${filename}`;
      const response = await fetch(`${API_BASE}/api/delete`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ filepath: fullPath }),
      });
      if (response.ok) {
        await loadImages(selectedAlbum.value);
        await loadSystemInfo();
        return true;
      }
      return false;
    } catch (error) {
      console.error("Failed to delete image:", error);
      return false;
    }
  }

  async function displayImage(album, filename) {
    try {
      const fullPath = `${album}/${filename}`;
      const response = await fetch(`${API_BASE}/api/display`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ filepath: fullPath }),
      });
      return response.ok;
    } catch (error) {
      console.error("Failed to display image:", error);
      return false;
    }
  }

  async function enterSleep() {
    try {
      const response = await fetch(`${API_BASE}/api/sleep`, { method: "POST" });
      return response.ok;
    } catch (error) {
      console.error("Failed to enter sleep:", error);
      return false;
    }
  }

  async function rotateNow() {
    try {
      const response = await fetch(`${API_BASE}/api/rotate`, { method: "POST" });
      return response.ok;
    } catch (error) {
      console.error("Failed to rotate:", error);
      return false;
    }
  }

  async function keepAlive() {
    try {
      const response = await fetch(`${API_BASE}/api/keep_alive`, { method: "POST" });
      return response.ok;
    } catch (error) {
      console.error("Failed to send keep-alive:", error);
      return false;
    }
  }

  async function loadSystemInfo() {
    loading.value.systemInfo = true;
    try {
      const response = await fetch(`${API_BASE}/api/system-info`);
      if (response.ok) {
        systemInfo.value = await response.json();
      }
    } catch (error) {
      console.error("Failed to load system info:", error);
    } finally {
      loading.value.systemInfo = false;
    }
  }

  return {
    // State
    albums,
    selectedAlbum,
    images,
    // #if FORK_FIXES
    imagesHasMore,
    // #endif
    battery,
    systemInfo,
    loading,
    // Getters
    sortedAlbums,
    currentAlbumImages,
    isGrayscale,
    // Actions
    loadBatteryStatus,
    loadAlbums,
    loadImages,
    selectAlbum,
    toggleAlbumEnabled,
    createAlbum,
    deleteAlbum,
    deleteImage,
    displayImage,
    enterSleep,
    rotateNow,
    keepAlive,
    loadSystemInfo,
  };
});
