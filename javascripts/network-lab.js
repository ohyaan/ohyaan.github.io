(function () {
  "use strict";

  // Selected mappings for the overview, not every possible protocol stack.
  const paths = [
    { name: "UDP", layers: [["Your messages", "Separate datagrams"], ["UDP", "No delivery or order guarantee"]], note: "Send independent messages, such as periodic readings." },
    { name: "TCP", layers: [["Your message format", "Define boundaries in your code"], ["TCP", "Reliable, ordered bytes"]], note: "Get an ordered stream; parse messages in your program." },
    { name: "HTTP", layers: [["HTTP/1.1", "Requests and responses"], ["TCP", "Ordered bytes"]], note: "Request a resource, such as the latest temperature." },
    { name: "HTTPS", layers: [["HTTP/1.1 or HTTP/2", "Requests and responses"], ["TLS", "Authenticate and protect"], ["TCP", "Ordered bytes"]], note: "Protect HTTP traffic and verify the server's identity." },
    { name: "WebSocket", layers: [["WebSocket", "Two-way messages"], ["TCP", "Ordered bytes"]], note: "Exchange ongoing updates after an HTTP/1.1 handshake." },
    { name: "WSS", layers: [["WebSocket", "Two-way messages"], ["TLS", "Authenticate and protect"], ["TCP", "Ordered bytes"]], note: "Add TLS protection to the WebSocket connection." },
    { name: "DTLS", layers: [["Your messages", "Separate datagrams"], ["DTLS", "Authenticate and protect"], ["UDP", "No delivery or order guarantee"]], note: "Protect datagrams while keeping UDP delivery behaviour." },
    { name: "QUIC", layers: [["Your application", "One or more streams"], ["QUIC", "Reliable streams · TLS handshake · protection"], ["UDP", "Carries QUIC packets"]], note: "QUIC adds reliable streams and protection above UDP." },
    { name: "HTTP/3", layers: [["HTTP/3", "Requests and responses"], ["QUIC", "Reliable streams · TLS handshake · protection"], ["UDP", "Carries QUIC packets"]], note: "HTTPS can use QUIC and UDP instead of TCP." }
  ];

  function element(tag, text, className) {
    const node = document.createElement(tag);
    if (text !== undefined) node.textContent = text;
    if (className) node.className = className;
    return node;
  }

  function button(text, action, className) {
    const node = element("button", text, className);
    node.type = "button";
    node.addEventListener("click", action);
    return node;
  }

  function stackExplorer(root) {
    if (root.dataset.ready) return;
    root.dataset.ready = "true";
    const controls = element("div", undefined, "network-controls");
    controls.setAttribute("role", "group");
    controls.setAttribute("aria-label", "Choose a protocol path");
    const diagram = element("div", undefined, "network-stack");
    diagram.setAttribute("aria-live", "polite");
    root.replaceChildren(controls, diagram);
    function select(path) {
      for (const control of controls.children) control.setAttribute("aria-pressed", String(control.textContent === path.name));
      const layers = element("div", undefined, "network-layers");
      for (const [name, detail] of [...path.layers, ["IP", "Addresses and routing"], ["Ethernet / Wi-Fi", "Local network link"]]) {
        const layer = element("div", undefined, "network-layer" + (["IP", "Ethernet / Wi-Fi"].includes(name) ? " network-base" : ""));
        layer.append(element("strong", name), element("span", detail));
        layers.append(layer);
      }
      const note = element("div", undefined, "network-note");
      note.append(element("strong", path.name), element("p", path.note));
      diagram.replaceChildren(layers, note);
    }
    for (const path of paths) controls.append(button(path.name, () => select(path)));
    select(paths[3]);
  }

  function deliveryExplorer(root) {
    if (root.dataset.ready) return;
    root.dataset.ready = "true";
    const scenarios = [
      { name: "In order", events: [1, 2, 3] },
      { name: "2 arrives late", events: [1, 3, 2] },
      { name: "2 is lost", events: [1, "lost", 3, "retry"] }
    ];
    let scenario = scenarios[1];
    let step = 0;
    let timer = null;
    const choices = element("div", undefined, "network-controls");
    choices.setAttribute("role", "group");
    choices.setAttribute("aria-label", "Network conditions");
    const comparison = element("div", undefined, "network-comparison");

    function chips(target, numbers, style) {
      target.replaceChildren();
      target.classList.toggle("is-stream", style === "stream");
      if (!numbers.length) target.append(element("span", "—", "network-empty"));
      for (const n of numbers) target.append(element("span", String(n), "network-chip network-chip-" + n + (style === "waiting" ? " is-waiting" : "")));
    }

    const panels = ["UDP", "TCP"].map(protocol => {
      const panel = element("div", undefined, "network-panel");
      panel.append(element("h3", protocol));
      function row(label) {
        const line = element("div", undefined, "network-row");
        const values = element("div", undefined, "network-values");
        line.append(element("span", label, "network-row-label"), values);
        panel.append(line);
        return values;
      }
      chips(row("Sent"), [1, 2, 3]);
      const wire = element("div", undefined, "network-wire");
      wire.setAttribute("aria-hidden", "true");
      panel.append(wire);
      const arrived = row("Arrived");
      const waiting = row(protocol === "TCP" ? "TCP holds" : "Holds");
      const app = row("App gets");
      app.classList.add("network-app");
      comparison.append(panel);
      return { wire, arrived, waiting, app };
    });

    const explanation = element("p", undefined, "network-result");
    explanation.setAttribute("role", "status");
    const controls = element("div", undefined, "network-controls");
    function stop() {
      clearTimeout(timer);
      timer = null;
      play.textContent = "Play";
    }
    function advance() {
      if (step < scenario.events.length) { step++; render(true); }
      if (step === scenario.events.length) stop();
    }
    function tick() {
      if (!root.isConnected) { stop(); return; }
      advance();
      if (step < scenario.events.length) timer = setTimeout(tick, 1200);
    }
    const play = button("Play", () => {
      if (timer !== null) { stop(); return; }
      if (step === scenario.events.length) step = 0;
      play.textContent = "Pause";
      tick();
    }, "network-next");
    const next = button("Next", () => { stop(); advance(); });
    const reset = button("Restart", () => { stop(); step = 0; render(false); });
    const progress = element("span", undefined, "network-caption");
    controls.append(play, next, reset, progress);
    root.replaceChildren(choices, comparison, explanation, controls,
      element("p", "TCP numbers mark byte ranges, not separate messages. Events are simplified.", "network-caption"));

    function render(animate) {
      const events = scenario.events.slice(0, step);
      const udp = events.filter(Number.isInteger);
      const tcp = events.flatMap(event => Number.isInteger(event) ? [event] : event === "retry" ? [2] : []);
      let contiguous = 0;
      while (tcp.includes(contiguous + 1)) contiguous++;
      const available = Array.from({ length: contiguous }, (_, i) => i + 1);
      const held = tcp.filter(n => n > contiguous);
      const current = events[events.length - 1];
      panels.forEach((panel, index) => {
        chips(panel.arrived, index ? tcp : udp);
        chips(panel.waiting, index ? held : [], "waiting");
        chips(panel.app, index ? available : udp, index ? "stream" : "messages");
        panel.wire.replaceChildren();
        if (current !== undefined && !(current === "retry" && !index)) {
          const lost = current === "lost";
          const token = element("span", lost ? "2 ×" : current === "retry" ? "2 ↻" : String(current),
            "network-packet" + (lost ? " is-lost" : "") + (animate ? " is-moving" : ""));
          panel.wire.append(token);
        }
      });
      const last = step === scenario.events.length;
      if (!step) explanation.textContent = "Both send 1 → 2 → 3. Press Play or Next.";
      else if (current === "lost") explanation.textContent = "2 is lost before reaching either receiver.";
      else if (held.length) explanation.textContent = "3 arrives before 2. UDP delivers 3; TCP holds it.";
      else if (last && scenario === scenarios[2]) explanation.textContent = "TCP resends 2 and releases 2 → 3. UDP still has only 1 → 3.";
      else if (last && scenario === scenarios[1]) explanation.textContent = "UDP: 1 → 3 → 2. TCP: 1 → 2 → 3.";
      else if (last) explanation.textContent = "Both deliver data in order in this example.";
      else explanation.textContent = current + " arrives. Both make it available to the application.";
      for (const control of choices.children) control.setAttribute("aria-pressed", String(control.textContent === scenario.name));
      next.disabled = last;
      progress.textContent = step + " / " + scenario.events.length;
    }
    for (const choice of scenarios) choices.append(button(choice.name, () => {
      stop(); scenario = choice; step = 0; render(false);
    }));
    render(false);
  }

  function initialize() {
    document.querySelectorAll("[data-network-stack]").forEach(stackExplorer);
    document.querySelectorAll("[data-network-delivery]").forEach(deliveryExplorer);
  }
  if (window.document$ && typeof window.document$.subscribe === "function") {
    window.document$.subscribe(initialize);
  } else if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", initialize, { once: true });
  } else {
    initialize();
  }
})();
