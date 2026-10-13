#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <linux/gpio.h>
#include <sys/ioctl.h>
#include <thread>
#include <unistd.h>

namespace {
volatile std::sig_atomic_t stopped = 0;
void stop(int) { stopped = 1; }
}

int main(int argc, char* argv[])
{
    constexpr unsigned offset = 24;
    if (argc < 2 || argc > 3 ||
        (argc == 3 && std::strcmp(argv[2], "--apply") != 0)) {
        std::cerr << "Usage: gpio_led /dev/gpiochipN [--apply]\n";
        return 1;
    }
    if (argc == 2) {
        std::cout << "Dry run: GPIO24, physical pin 18, ten 500 ms blinks.\n"
                     "Verify chip and wiring before adding --apply.\n";
        return 0;
    }
    std::signal(SIGINT, stop);
    std::signal(SIGTERM, stop);
    const int chip = open(argv[1], O_RDONLY | O_CLOEXEC);
    if (chip < 0) {
        std::cerr << "Open chip: " << std::strerror(errno) << '\n';
        return 1;
    }
    gpio_v2_line_info info{};
    info.offset = offset;
    if (ioctl(chip, GPIO_V2_GET_LINEINFO_IOCTL, &info) < 0 ||
        std::strcmp(info.name, "GPIO24") != 0) {
        std::cerr << "Cannot confirm GPIO24 at offset 24; no line requested.\n";
        close(chip);
        return 1;
    }
    gpio_v2_line_request request{};
    request.offsets[0] = offset;
    request.num_lines = 1;
    request.config.flags = GPIO_V2_LINE_FLAG_OUTPUT;
    request.config.num_attrs = 1;
    request.config.attrs[0].attr.id = GPIO_V2_LINE_ATTR_ID_OUTPUT_VALUES;
    request.config.attrs[0].attr.values = 0; // Initially low.
    request.config.attrs[0].mask = 1;       // Index 0 in the requested set.
    std::strcpy(request.consumer, "pi-course-led");
    if (ioctl(chip, GPIO_V2_GET_LINE_IOCTL, &request) < 0) {
        std::cerr << "Request GPIO24: " << std::strerror(errno) << '\n';
        close(chip);
        return 1;
    }
    auto set_value = [&](bool high) {
        gpio_v2_line_values values{};
        values.mask = 1;
        values.bits = high ? 1 : 0;
        if (ioctl(request.fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &values) < 0) {
            std::cerr << "Set value: " << std::strerror(errno) << '\n';
            return false;
        }
        return true;
    };
    bool ok = true;
    for (int blink = 0; blink < 10 && !stopped; ++blink) {
        if (!set_value(true)) { ok = false; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if (!set_value(false)) { ok = false; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    if (!set_value(false)) ok = false;
    close(request.fd);
    close(chip);
    std::cout << "Line released; inspect the LED before disconnecting power.\n";
    return ok ? 0 : 1;
}
