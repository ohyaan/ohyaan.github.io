(() => {
  const flags = [
    { bit: 0n, label: "Undervoltage detected" },
    { bit: 1n, label: "Arm frequency capped" },
    { bit: 2n, label: "Throttled" },
    { bit: 3n, label: "Soft temperature limit" },
  ];
  const knownMask = 0x000f000fn;

  function initialise() {
    const form = document.getElementById("pi-throttled-decoder");
    if (!form || form.dataset.initialised) return;
    form.dataset.initialised = "true";
    const input = form.querySelector("#throttled-value");
    const message = form.querySelector("#throttled-message");
    const results = form.querySelector("#throttled-results");

    function appendItem(list, text, anchor) {
      const item = document.createElement("li");
      if (anchor) {
        const link = document.createElement("a");
        link.href = `#${anchor}`;
        link.textContent = text;
        item.append(link);
      } else {
        item.textContent = text;
      }
      list.append(item);
    }

    function decode() {
      const match = input.value.trim().match(/^(?:throttled=)?(?:0x)?([0-9a-f]{1,8})$/i);
      results.hidden = true;
      if (!match) {
        input.setAttribute("aria-invalid", "true");
        message.textContent = "Enter one hexadecimal reading, such as 0x50000 or throttled=0x50000 (up to 8 hex digits).";
        return;
      }
      input.removeAttribute("aria-invalid");
      const value = BigInt(`0x${match[1]}`);
      const current = value & 0xfn;
      const history = (value >> 16n) & 0xfn;
      const unknown = value & ~knownMask;
      form.querySelector("#throttled-mask").textContent = `0x${value.toString(16)}`;

      for (const [id, mask, empty] of [
        ["#throttled-current", current, "No documented current flags set in this reading."],
        ["#throttled-history", history, "No documented historical flags set in this reading."],
      ]) {
        const list = form.querySelector(id);
        list.replaceChildren();
        for (const flag of flags) {
          if (mask & (1n << flag.bit)) appendItem(list, flag.label);
        }
        if (mask === 0n) appendItem(list, empty);
      }

      const unknownOutput = form.querySelector("#throttled-unknown");
      unknownOutput.hidden = unknown === 0n;
      unknownOutput.textContent = unknown === 0n ? "" :
        `Unknown bits: 0x${unknown.toString(16)}. These bits are outside the documented flags; check firmware documentation before interpreting them.`;

      const next = form.querySelector("#throttled-next");
      next.replaceChildren();
      if (current & 1n) {
        appendItem(next, "Check the power supply, cable, connectors, and peripheral load.", "if-undervoltage-is-active-inspect-the-power-path");
      }
      if (current & 8n) {
        appendItem(next, "Check temperature, airflow, and cooling during the affected workload.", "if-temperature-is-high-verify-cooling-under-load");
      }
      if (current & 6n) {
        appendItem(next, "Compare temperature and power readings during the slowdown. Capping or throttling alone does not identify the cause.", "read-get_throttled-alongside-temperature");
      }
      if (current === 0n && history !== 0n) {
        appendItem(next, "Repeat the reading during the affected workload. Historical flags do not date the event or show that it is still active.", "verify-recovery-without-confusing-history-with-an-active-fault");
      }
      if (current === 0n && history === 0n) {
        appendItem(next, "Repeat during the slowdown and check other bottlenecks. Clear documented flags in one snapshot do not prove the power supply is good.", "verify-recovery-without-confusing-history-with-an-active-fault");
      }
      message.textContent = "Decoded locally. Current flags describe this snapshot; historical flags record earlier events.";
      results.hidden = false;
    }

    form.addEventListener("submit", (event) => {
      event.preventDefault();
      decode();
    });
    input.addEventListener("input", () => {
      results.hidden = true;
      input.removeAttribute("aria-invalid");
      message.textContent = "Select Decode reading to update the result.";
    });
    form.querySelectorAll("[data-throttled-example]").forEach((button) => {
      button.addEventListener("click", () => {
        input.value = button.dataset.throttledExample;
        decode();
      });
    });
    form.hidden = false;
  }

  if (typeof document$ !== "undefined") {
    document$.subscribe(initialise);
  } else if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", initialise);
  } else {
    initialise();
  }
})();
