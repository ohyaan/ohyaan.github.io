document.addEventListener("DOMContentLoaded", () => {
  const form = document.getElementById("headless-troubleshooter");
  if (!form) return;

  const connection = form.querySelector("#headless-connection");
  const symptom = form.querySelector("#headless-symptom");
  const result = form.querySelector("#headless-result");
  const title = form.querySelector("#headless-result-title");
  const summary = form.querySelector("#headless-result-summary");
  const steps = form.querySelector("#headless-result-steps");
  const commands = form.querySelector("#headless-result-commands");
  const message = form.querySelector("#headless-message");
  const copyButton = form.querySelector("#copy-headless-commands");

  const paths = {
    "no-boot": {
      title: "Prove boot before changing the network",
      summary: "A network fix cannot help a system that did not read and start the image.",
      steps: [
        "Disconnect optional USB devices and HATs, then use a known-good official power supply.",
        "Confirm that Imager completed verification and that the storage is assigned to the intended Pi model.",
        "Check model-specific boot diagnostics with a display or serial console before rewriting configuration files.",
        "Test separate known-good storage so image failure and hardware failure are not conflated.",
      ],
      commands: "# Run after obtaining a local console\ncat /etc/os-release\nuname -a\nsystemd-analyze time\njournalctl -b -p warning --no-pager",
    },
    "not-in-router": {
      title: "Check link and DHCP before SSH",
      summary: "No lease normally means the Pi has not joined the expected network or has not completed DHCP.",
      steps: [
        "Confirm the Pi booted and inspect the router's authorised DHCP lease list.",
        "For Ethernet, check link LEDs, cable, switch port, VLAN, and port security.",
        "For Wi-Fi, verify country, exact SSID, security type, and password in Imager.",
        "Use a local console to inspect NetworkManager state without printing saved secrets.",
      ],
      commands: "ip -brief link\nip -brief address\nip route\nnmcli device status\nnmcli connection show\njournalctl -u NetworkManager -b --no-pager | tail -80",
    },
    mdns: {
      title: "Treat this as name resolution, not an SSH failure",
      summary: "A working connection by IP proves that boot, basic networking, and SSH are already functioning.",
      steps: [
        "Continue with the verified IP address while diagnosing mDNS on the client.",
        "Confirm the configured hostname and that both devices are on a network that permits multicast DNS.",
        "Check client resolver and firewall policy; guest Wi-Fi and VLAN boundaries often block multicast.",
      ],
      commands: "# On the Raspberry Pi\nhostnamectl hostname\nip -brief address\nsystemctl status avahi-daemon --no-pager\n\n# On a Linux client\ngetent hosts <hostname>.local\nresolvectl query <hostname>.local",
    },
    timeout: {
      title: "A timeout occurs before authentication",
      summary: "Check endpoint, route, segmentation, and port filtering before changing passwords or keys.",
      steps: [
        "Verify the address against the router lease and Pi console; do not reuse a remembered address blindly.",
        "Confirm that the client has a route and that guest isolation or a VLAN does not block the path.",
        "On the Pi, check that ssh.service is running and a socket is listening on the configured port.",
      ],
      commands: "# On the client\nip route get <known-ip>\nssh -vv -o ConnectTimeout=10 <username>@<known-ip>\n\n# On the Raspberry Pi console\nsystemctl status ssh --no-pager\nss -ltnp | grep ':22'\nsudo nft list ruleset",
    },
    refused: {
      title: "The host answered, but SSH is not accepting connections",
      summary: "Connection refused usually means nothing is listening on that port, or a firewall actively rejected it.",
      steps: [
        "Verify that the responding address is the intended Raspberry Pi.",
        "Use a local console to validate sshd configuration and start the service.",
        "Inspect the boot journal before enabling or reinstalling anything.",
      ],
      commands: "hostnamectl\nsudo sshd -t\nsystemctl status ssh --no-pager\nsudo systemctl enable --now ssh\nss -ltnp | grep ':22'\njournalctl -u ssh -b --no-pager | tail -80",
    },
    publickey: {
      title: "Verify username, offered key, and server permissions",
      summary: "The SSH server is reachable; authentication rejected the account or key presented by the client.",
      steps: [
        "Use the username created in Imager, not an assumed pi account.",
        "Check which identity the client actually offers with verbose SSH output.",
        "On a local console, verify the account, ownership, and permissions of authorized_keys.",
        "Do not disable key checks or enable broad password login as a shortcut.",
      ],
      commands: "# On the client\nssh -G <username>@<known-ip> | grep -E '^(user|hostname|identityfile) '\nssh -vv <username>@<known-ip>\n\n# On the Raspberry Pi console\ngetent passwd <username>\nnamei -l /home/<username>/.ssh/authorized_keys\nsudo sshd -t\njournalctl -u ssh -b --no-pager | tail -80",
    },
    "host-key": {
      title: "Confirm identity before removing the old host key",
      summary: "A re-image can change a key, but the warning also protects against a wrong device or interceptor.",
      steps: [
        "Check the address in the router lease and identify the Raspberry Pi locally.",
        "Display the server ED25519 fingerprint on the Pi console and compare it with the client warning.",
        "Only after verification, remove the old entry for that exact endpoint and reconnect.",
      ],
      commands: "# On the Raspberry Pi console\nsudo ssh-keygen -lf /etc/ssh/ssh_host_ed25519_key.pub\n\n# On the client, only after verification\nssh-keygen -R <verified-host-or-ip>\nssh <username>@<verified-host-or-ip>",
    },
    "usb-missing": {
      title: "Separate USB data-path and Gadget configuration failures",
      summary: "A charging-only cable, wrong connector, unsupported topology, or incomplete configuration can look identical.",
      steps: [
        "Confirm that the Pi model and selected USB port support the intended device-mode path.",
        "Use a known data-capable cable connected directly to the host before adding hubs or adapters.",
        "Inspect the host kernel log while reconnecting, then verify the Pi's current Gadget configuration locally.",
        "Do not apply instructions for a different Pi model or older boot-file layout.",
      ],
      commands: "# On a Linux host while reconnecting\nsudo dmesg --follow\nip -brief link\n\n# On the Raspberry Pi console\ncat /proc/device-tree/model; echo\nls /sys/class/udc\nip -brief link\njournalctl -b --no-pager | grep -Ei 'usb|gadget|dwc' | tail -80",
    },
  };

  const render = () => {
    const path = paths[symptom.value];
    title.textContent = path.title;
    summary.textContent = path.summary;
    steps.replaceChildren();
    path.steps.forEach((text) => {
      const item = document.createElement("li");
      item.textContent = text;
      steps.append(item);
    });
    const connectionNote = {
      wifi: "Selected path: Wi-Fi. Include NetworkManager and radio state in your evidence.",
      ethernet: "Selected path: Ethernet. Include link, switch port, VLAN, and DHCP state in your evidence.",
      usb: "Selected path: USB Gadget Mode. Include model, USB port, cable, host log, and UDC state in your evidence.",
    }[connection.value];
    commands.textContent = path.commands;
    message.textContent = connectionNote;
    result.hidden = false;
    copyButton.disabled = false;
  };

  form.addEventListener("submit", (event) => {
    event.preventDefault();
    render();
  });

  copyButton.addEventListener("click", async () => {
    try {
      await navigator.clipboard.writeText(commands.textContent);
      message.textContent = "Commands copied. Replace every <placeholder> before running them.";
    } catch (_error) {
      message.textContent = "Copy was unavailable. Select the command block and copy it manually.";
    }
  });

  render();
});
