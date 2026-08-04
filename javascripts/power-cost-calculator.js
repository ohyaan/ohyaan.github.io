document.addEventListener("DOMContentLoaded", () => {
  const form = document.getElementById("pi-power-calculator");
  if (!form) return;

  const fields = {
    idleWatts: form.querySelector("#power-idle-watts"),
    loadWatts: form.querySelector("#power-load-watts"),
    poweredHours: form.querySelector("#power-powered-hours"),
    loadHours: form.querySelector("#power-load-hours"),
    days: form.querySelector("#power-days"),
    rate: form.querySelector("#power-rate"),
    devices: form.querySelector("#power-devices"),
    carbon: form.querySelector("#power-carbon"),
  };
  const message = form.querySelector("#power-message");
  const results = form.querySelector("#power-results");

  const format = (value, maximumFractionDigits = 2) =>
    new Intl.NumberFormat(undefined, {
      maximumFractionDigits,
      minimumFractionDigits: 0,
    }).format(value);
  const read = (field) => Number.parseFloat(field.value);

  const calculate = () => {
    const values = Object.fromEntries(
      Object.entries(fields).map(([name, field]) => [name, read(field)])
    );
    const required = [
      "idleWatts",
      "loadWatts",
      "poweredHours",
      "loadHours",
      "days",
      "rate",
      "devices",
    ];
    if (required.some((name) => !Number.isFinite(values[name]))) {
      message.textContent = "Enter a valid number in every required field.";
      results.hidden = true;
      return;
    }
    if (
      values.idleWatts < 0 ||
      values.loadWatts < 0 ||
      values.poweredHours <= 0 ||
      values.poweredHours > 24 ||
      values.loadHours < 0 ||
      values.loadHours > values.poweredHours ||
      values.days <= 0 ||
      values.days > 31 ||
      values.rate < 0 ||
      values.devices < 1
    ) {
      message.textContent =
        "Use non-negative watts and rate, 1–24 powered hours, load hours no greater than powered hours, 1–31 days, and at least one device.";
      results.hidden = true;
      return;
    }

    const idleHours = values.poweredHours - values.loadHours;
    const deviceWhDay =
      values.loadWatts * values.loadHours + values.idleWatts * idleHours;
    const totalKwhDay = (deviceWhDay * values.devices) / 1000;
    const monthlyKwh = totalKwhDay * values.days;
    const annualKwh = totalKwhDay * values.days * 12;
    const monthlyCost = monthlyKwh * values.rate;
    const annualCost = annualKwh * values.rate;
    const averageWatts =
      (deviceWhDay / values.poweredHours) * values.devices;

    form.querySelector("#result-average-watts").textContent = `${format(averageWatts)} W`;
    form.querySelector("#result-daily-kwh").textContent = `${format(totalKwhDay, 3)} kWh`;
    form.querySelector("#result-monthly-kwh").textContent = `${format(monthlyKwh, 3)} kWh`;
    form.querySelector("#result-annual-kwh").textContent = `${format(annualKwh, 3)} kWh`;
    form.querySelector("#result-monthly-cost").textContent = format(monthlyCost, 2);
    form.querySelector("#result-annual-cost").textContent = format(annualCost, 2);

    const carbonRow = form.querySelector("#result-carbon-row");
    if (Number.isFinite(values.carbon) && values.carbon >= 0) {
      const annualCarbonKg = (annualKwh * values.carbon) / 1000;
      form.querySelector("#result-annual-carbon").textContent = `${format(annualCarbonKg, 2)} kg CO₂e`;
      carbonRow.hidden = false;
    } else {
      carbonRow.hidden = true;
    }

    message.textContent =
      "Calculated locally from your inputs. Cost values use the currency of the rate you entered.";
    results.hidden = false;
  };

  form.addEventListener("submit", (event) => {
    event.preventDefault();
    calculate();
  });
  form.querySelectorAll("[data-power-preset]").forEach((button) => {
    button.addEventListener("click", () => {
      if (button.dataset.powerPreset === "always-on") {
        fields.poweredHours.value = "24";
        fields.loadHours.value = "4";
        fields.days.value = "30";
      } else {
        fields.poweredHours.value = "8";
        fields.loadHours.value = "2";
        fields.days.value = "22";
      }
      calculate();
    });
  });
  calculate();
});
