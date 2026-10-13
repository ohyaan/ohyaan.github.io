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
    constexpr unsigned offset = 22;
    if (argc < 2 || argc > 3 ||
        (argc == 3 && std::strcmp(argv[2], "--apply") != 0)) {
        std::cerr << "Usage: gpio_button /dev/gpiochipN [--apply]\n";
        return 1;
    }
    if (argc == 2) {
        std::cout << "Dry run: GPIO22, physical pin 15, input with pull-up.\n"
                     "Samples every 5 ms, accepts 25 ms stable changes for 30 s.\n";
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
        std::strcmp(info.name, "GPIO22") != 0) {
        std::cerr << "Cannot confirm GPIO22 at offset 22; no line requested.\n";
        close(chip);
        return 1;
    }
    gpio_v2_line_request request{};
    request.offsets[0] = offset;
    request.num_lines = 1;
    request.config.flags = GPIO_V2_LINE_FLAG_INPUT | GPIO_V2_LINE_FLAG_BIAS_PULL_UP;
    std::strcpy(request.consumer, "pi-course-button");
    if (ioctl(chip, GPIO_V2_GET_LINE_IOCTL, &request) < 0) {
        std::cerr << "Request GPIO22: " << std::strerror(errno) << '\n';
        close(chip);
        return 1;
    }
    using Clock = std::chrono::steady_clock;
    bool candidate = true; // Electrical high = released.
    bool stable = true;
    bool ok = true;
    auto changed = Clock::now();
    const auto end = changed + std::chrono::seconds(30);
    std::cout << "Release the button, then press/release. Ctrl+C stops.\n";
    while (!stopped && Clock::now() < end) {
        gpio_v2_line_values values{};
        values.mask = 1;
        if (ioctl(request.fd, GPIO_V2_LINE_GET_VALUES_IOCTL, &values) < 0) {
            std::cerr << "Read value: " << std::strerror(errno) << '\n';
            ok = false;
            break;
        }
        const bool sample = (values.bits & 1) != 0;
        const auto now = Clock::now();
        if (sample != candidate) {
            candidate = sample;
            changed = now;
        }
        if (candidate != stable && now - changed >= std::chrono::milliseconds(25)) {
            stable = candidate;
            std::cout << (stable ? "Released\n" : "Pressed\n") << std::flush;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    close(request.fd);
    close(chip);
    return ok ? 0 : 1;
}
