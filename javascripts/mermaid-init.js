(function () {
  function renderMermaid() {
    if (!window.mermaid) {
      return;
    }

    window.mermaid.initialize({ startOnLoad: false });
    window.mermaid.run({ querySelector: ".mermaid" }).catch(function (error) {
      console.warn("Mermaid diagram rendering failed", error);
    });
  }

  if (window.document$ && typeof window.document$.subscribe === "function") {
    window.document$.subscribe(renderMermaid);
  } else if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", renderMermaid, { once: true });
  } else {
    renderMermaid();
  }
})();
