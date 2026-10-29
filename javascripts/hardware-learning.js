// Explicit course completion, with browser-local storage and no network calls.
(function (root, factory) {
  const api = factory();
  if (typeof module === "object" && module.exports) module.exports = api;
  if (root.document && root.window) {
    api.start(root.document, root.window,
      typeof document$ === "undefined" ? null : document$);
  }
})(typeof globalThis === "undefined" ? this : globalThis, function () {
  "use strict";
  const KEY = "with-rpi:cpp-hardware:progress:v1";

  function normalize(value, ids) {
    const known = new Set(ids);
    if (!value || value.version !== 1) value = {};
    const completed = new Set(Array.isArray(value.completed) ? value.completed : []);
    return { version: 1, completed: ids.filter(id => completed.has(id)),
      last: known.has(value.last) ? value.last : null };
  }

  function createStore(storage, ids) {
    let state = normalize(null, ids);
    let persistent = Boolean(storage);
    function reload() {
      if (!persistent) return;
      try {
        const raw = storage.getItem(KEY);
        try { state = normalize(raw ? JSON.parse(raw) : null, ids); }
        catch (_) { state = normalize(null, ids); }
      } catch (_) { persistent = false; }
    }
    function write() {
      if (!persistent) return;
      try { storage.setItem(KEY, JSON.stringify(state)); }
      catch (_) { persistent = false; }
    }
    reload();
    return {
      snapshot: () => ({ ...state, completed: [...state.completed] }),
      persistent: () => persistent,
      reload,
      visit(id) {
        if (!ids.includes(id)) return;
        reload(); state.last = id; write();
      },
      complete(id, checked) {
        if (!ids.includes(id)) return;
        reload();
        const selected = new Set(state.completed);
        if (checked) selected.add(id); else selected.delete(id);
        state.completed = ids.filter(item => selected.has(item));
        state.last = id;
        write();
      },
      reset() {
        state = normalize(null, ids);
        if (storage) {
          try { storage.removeItem(KEY); } catch (_) { persistent = false; }
        }
      }
    };
  }

  function resume(state, lessons) {
    const done = new Set(state.completed);
    const last = lessons.findIndex(lesson => lesson.id === state.last);
    if (last >= 0 && !done.has(state.last)) return lessons[last];
    for (let offset = 1; offset <= lessons.length; offset++) {
      const candidate = lessons[(Math.max(last, -1) + offset) % lessons.length];
      if (!done.has(candidate.id)) return candidate;
    }
    return lessons[Math.max(last, 0)];
  }

  function start(doc, win, navigation) {
    const initialized = new WeakSet();
    let store, lessons;
    const panels = () => Array.from(doc.querySelectorAll("[data-hardware-course], [data-hardware-resume]"));
    function render() {
      if (!store) return;
      const state = store.snapshot();
      const next = resume(state, lessons);
      const allDone = state.completed.length === lessons.length;
      const last = lessons.find(lesson => lesson.id === state.last);
      panels().forEach(panel => {
        const home = panel.hasAttribute("data-hardware-resume");
        panel.hidden = home && !last && state.completed.length === 0;
        const summary = panel.querySelector("[data-learning-summary]");
        summary.textContent = `${state.completed.length} of ${lessons.length} lessons marked complete`;
        const link = panel.querySelector("[data-learning-resume]");
        link.setAttribute("href", next.path);
        link.textContent = `${allDone ? "Review" : last || state.completed.length ? "Continue" : "Start"}: ${next.title}`;
        if (home) return;
        const progress = panel.querySelector("[data-learning-progress]");
        progress.max = lessons.length;
        progress.value = state.completed.length;
        const lastNotice = panel.querySelector("[data-learning-last]");
        lastNotice.hidden = !last;
        lastNotice.textContent = last ? `Last opened: ${last.title}` : "";
        const storageNotice = panel.querySelector("[data-learning-storage]");
        storageNotice.hidden = store.persistent();
        storageNotice.textContent = store.persistent()
          ? "Progress saving is available in this browser."
          : "Progress cannot be saved in this browser. Changes last only until this page reloads.";
        const checkbox = panel.querySelector("[data-learning-complete]");
        if (checkbox) checkbox.checked = state.completed.includes(panel.getAttribute("data-lesson"));
      });
    }
    function initialize() {
      const roots = panels();
      if (!roots.length) return;
      if (!store) {
        try {
          lessons = JSON.parse(roots[0].querySelector("[data-learning-config]").textContent);
          if (!Array.isArray(lessons) || lessons.length === 0 ||
              !lessons.every(item => item && typeof item.id === "string" && /^[a-z-]+$/.test(item.id) &&
                typeof item.title === "string" && typeof item.path === "string" &&
                /^\/programming\/[a-z0-9._-]+\/$/.test(item.path)) ||
              new Set(lessons.map(item => item.id)).size !== lessons.length) return;
        } catch (_) { return; }
        let storage;
        try { storage = win.localStorage; } catch (_) { storage = null; }
        store = createStore(storage, lessons.map(item => item.id));
      }
      roots.forEach(panel => {
        if (initialized.has(panel)) return;
        initialized.add(panel);
        const id = panel.getAttribute("data-lesson");
        if (lessons.some(lesson => lesson.id === id)) store.visit(id);
        const checkbox = panel.querySelector("[data-learning-complete]");
        if (checkbox) checkbox.addEventListener("change", () => {
          store.complete(id, checkbox.checked); render();
        });
        const reset = panel.querySelector("[data-learning-reset]");
        if (reset) reset.addEventListener("click", () => {
          if (win.confirm("Reset only your C++ Hardware Programming progress in this browser?")) {
            store.reset(); render();
          }
        });
      });
      render();
    }
    if (doc.readyState === "loading") doc.addEventListener("DOMContentLoaded", initialize, { once: true });
    else initialize();
    if (navigation) navigation.subscribe(initialize);
    win.addEventListener("storage", event => {
      if (store && (event.key === KEY || event.key === null)) { store.reload(); render(); }
    });
    return { initialize };
  }
  return { KEY, normalize, createStore, resume, start };
});
