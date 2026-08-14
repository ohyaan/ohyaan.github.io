(() => {
  const selector = "[data-aosp-map-fullscreen]";

  const enterFullscreen = async (button) => {
    const shell = button.closest(".aosp-map-shell");
    const frame = shell?.querySelector(".aosp-map-frame");
    if (!frame) return;

    try {
      await frame.requestFullscreen();
    } catch (error) {
      console.warn("The browser could not enter full-screen map mode.", error);
    }
  };

  document.addEventListener("click", (event) => {
    const button = event.target.closest(selector);
    if (button) enterFullscreen(button);
  });
})();
