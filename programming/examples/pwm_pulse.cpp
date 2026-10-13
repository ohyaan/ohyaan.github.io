#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
volatile std::sig_atomic_t stopped = 0;
void stop(int) { stopped = 1; }

std::uint64_t number(const char* text)
{
    const std::string input(text);
    std::uint64_t value = 0;
    const auto result = std::from_chars(input.data(), input.data() + input.size(), value);
    if (result.ec != std::errc{} || result.ptr != input.data() + input.size())
        throw std::runtime_error("Use unsigned decimal integers, without units.");
    return value;
}

void write_value(const std::filesystem::path& path, std::uint64_t value)
{
    std::ofstream file(path);
    if (!file) throw std::runtime_error("Cannot open " + path.string());
    file << value << '\n';
    file.flush();
    if (!file) throw std::runtime_error("Cannot write " + path.string());
    file.close();
    if (file.fail()) throw std::runtime_error("Cannot close " + path.string());
}
}

int main(int argc, char* argv[])
{
    if (argc != 6 && argc != 7) {
        std::cerr << "Usage: pwm_pulse CHIP CHANNEL PERIOD_NS DUTY_NS HOLD_MS [--apply]\n";
        return 1;
    }
    std::filesystem::path chip;
    std::filesystem::path pwm;
    std::uint64_t channel = 0;
    bool owned = false;
    bool ok = true;
    try {
        if (argc == 7 && std::string(argv[6]) != "--apply")
            throw std::runtime_error("Only --apply enables hardware writes.");
        chip = argv[1];
        channel = number(argv[2]);
        const auto period = number(argv[3]);
        const auto duty = number(argv[4]);
        const auto hold = number(argv[5]);
        if (channel > 255 || period == 0 || period > 1'000'000'000 ||
            duty == 0 || duty >= period || hold == 0 || hold > 60'000)
            throw std::runtime_error("Require 0<duty<period<=1e9 ns, 1..60000 ms, channel<=255.");
        std::cout << "Period " << period << " ns, duty " << duty
                  << " ns, hold " << hold << " ms\n";
        if (argc == 6) {
            std::cout << "Dry run only; no PWM files opened.\n";
            return 0;
        }
        pwm = chip / ("pwm" + std::to_string(channel));
        std::uint64_t channels = 0;
        std::ifstream count(chip / "npwm");
        if (!(count >> channels) || channel >= channels)
            throw std::runtime_error("Cannot confirm this channel from npwm.");
        if (std::filesystem::exists(pwm))
            throw std::runtime_error("Channel already exported; refuse to take it over.");
        std::signal(SIGINT, stop);
        std::signal(SIGTERM, stop);
        write_value(chip / "export", channel);
        owned = true; // Only a successful export gives this process ownership.
        for (int attempt = 0; attempt < 100 && !std::filesystem::exists(pwm); ++attempt) {
            if (stopped) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        if (!std::filesystem::exists(pwm) || stopped)
            throw std::runtime_error("Channel did not appear or operation interrupted.");
        write_value(pwm / "enable", 0);
        write_value(pwm / "duty_cycle", 0); // Clear old duty before choosing a period.
        write_value(pwm / "period", period);
        write_value(pwm / "duty_cycle", duty);
        if (stopped) throw std::runtime_error("Interrupted before enable.");
        write_value(pwm / "enable", 1);
        const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(hold);
        while (!stopped && std::chrono::steady_clock::now() < end)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        ok = false;
    }
    if (owned) {
        // Try every cleanup action even if an earlier one fails.
        for (const auto& path : {pwm / "enable", pwm / "duty_cycle"}) {
            try { write_value(path, 0); }
            catch (const std::exception& error) {
                std::cerr << "Cleanup: " << error.what() << '\n';
                ok = false;
            }
        }
        try { write_value(chip / "unexport", channel); }
        catch (const std::exception& error) {
            std::cerr << "Cleanup: " << error.what() << '\n';
            ok = false;
        }
    }
    return ok ? 0 : 1;
}
