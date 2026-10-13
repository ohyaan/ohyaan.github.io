// Muted autoplay, with manual controls and a static poster for reduced motion.
(() => {
  const motion = window.matchMedia("(prefers-reduced-motion: reduce)");
  const initialized = new WeakSet();
  const videos = () => document.querySelectorAll("video[data-oled-autoplay]");

  function applyPreference(video) {
    video.muted = true;
    video.autoplay = !motion.matches;
    video.preload = motion.matches ? "none" : "auto";
    if (motion.matches) {
      video.pause();
    } else {
      // Some browsers or power-saving modes still block autoplay. Keep controls.
      const playing = video.play();
      if (playing) playing.catch(() => {});
    }
  }

  function initialize() {
    videos().forEach((video) => {
      if (initialized.has(video)) return;
      initialized.add(video);
      applyPreference(video);
    });
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", initialize, { once: true });
  } else {
    initialize();
  }
  if (typeof document$ !== "undefined") document$.subscribe(initialize);
  motion.addEventListener("change", () => videos().forEach(applyPreference));
})();
