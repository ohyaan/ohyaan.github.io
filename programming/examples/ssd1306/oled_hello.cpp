/*
 * MIT License
 * Copyright (c) 2025-2026 ohyaan
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * SSD1306 initialization/frame transfer adapted from ohyaan/ssd1306.
 * Minimal HELLO PI glyphs below are defined here; no external font is required.
 */
#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <linux/i2c-dev.h>
#include <stdexcept>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>

constexpr int width = 128;
constexpr int height = 64;
using Frame = std::array<std::uint8_t, width * height / 8>;

void pixel(Frame& frame, int x, int y)
{
    if (x >= 0 && x < width && y >= 0 && y < height)
        frame[static_cast<std::size_t>(x + (y / 8) * width)] |=
            static_cast<std::uint8_t>(1u << (y % 8));
}

std::array<std::uint8_t, 5> glyph(char letter)
{
    // Each byte describes one vertical column; bit 0 is its top pixel.
    switch (letter) {
    case 'H': return {0x7f, 0x08, 0x08, 0x08, 0x7f};
    case 'E': return {0x7f, 0x49, 0x49, 0x49, 0x41};
    case 'L': return {0x7f, 0x40, 0x40, 0x40, 0x40};
    case 'O': return {0x3e, 0x41, 0x41, 0x41, 0x3e};
    case 'P': return {0x7f, 0x09, 0x09, 0x09, 0x06};
    case 'I': return {0x00, 0x41, 0x7f, 0x41, 0x00};
    case ' ': return {};
    default: throw std::runtime_error("This example only defines HELLO PI glyphs.");
    }
}

Frame greeting()
{
    Frame frame{};
    int x = 16;
    constexpr int y = 24;
    for (char letter : std::string("HELLO PI")) {
        const auto columns = glyph(letter);
        for (int column = 0; column < 5; ++column)
            for (int row = 0; row < 7; ++row)
                if ((columns[static_cast<std::size_t>(column)] >> row) & 1u)
                    for (int dx = 0; dx < 2; ++dx)
                        for (int dy = 0; dy < 2; ++dy)
                            pixel(frame, x + column * 2 + dx, y + row * 2 + dy);
        x += 12;
    }
    return frame;
}

class Bus {
public:
    Bus(const char* device, unsigned address)
    {
        fd_ = open(device, O_RDWR | O_CLOEXEC);
        if (fd_ < 0) throw std::runtime_error(std::string("Open I2C: ") + std::strerror(errno));
        if (ioctl(fd_, I2C_SLAVE, address) < 0) {
            const std::string error = std::strerror(errno);
            close(fd_);
            throw std::runtime_error("Select I2C address: " + error);
        }
    }
    ~Bus() { close(fd_); } // Closing the file does not turn the OLED off.
    Bus(const Bus&) = delete;
    Bus& operator=(const Bus&) = delete;

    void command(std::uint8_t value)
    {
        const std::array<std::uint8_t, 2> bytes{0x00, value};
        transfer(bytes.data(), bytes.size());
    }
    void show(const Frame& frame)
    {
        // 128x64 SSD1306 module with an internal charge pump.
        // Keep the panel off until all 1024 framebuffer bytes have been sent.
        constexpr std::uint8_t init[] = {
            0xae, 0xd5, 0x80, 0xa8, 0x3f, 0xd3, 0x00, 0x40,
            0x8d, 0x14, 0x20, 0x00, 0xa1, 0xc8, 0xda, 0x12,
            0x81, 0x40, 0xd9, 0xf1, 0xdb, 0x40, 0xa4, 0xa6, 0x2e,
            0x21, 0x00, 0x7f, 0x22, 0x00, 0x07
        };
        for (auto value : init) command(value);
        for (std::size_t offset = 0; offset < frame.size();) {
            std::array<std::uint8_t, 32> chunk{};
            chunk[0] = 0x40; // Data control byte, not the I2C address.
            const auto count = std::min<std::size_t>(31, frame.size() - offset);
            std::copy_n(frame.data() + offset, count, chunk.data() + 1);
            transfer(chunk.data(), count + 1);
            offset += count;
        }
        command(0xaf);
    }
private:
    int fd_ = -1;
    void transfer(const std::uint8_t* bytes, std::size_t count)
    {
        const auto written = write(fd_, bytes, count);
        if (written != static_cast<ssize_t>(count)) {
            if (written < 0)
                throw std::runtime_error(std::string("I2C write: ") + std::strerror(errno));
            throw std::runtime_error("Short I2C write; stop and check the connection.");
        }
    }
};

int main(int argc, char* argv[])
{
    try {
        if (argc < 3 || argc > 4)
            throw std::runtime_error("Usage: oled_hello DEVICE 0x3c|0x3d [--show|--off]");
        const std::string input(argv[2]);
        unsigned address = 0;
        if (input.size() != 4 || input.substr(0, 2) != "0x")
            throw std::runtime_error("Use a 7-bit address written as 0x3c or 0x3d.");
        const auto result = std::from_chars(input.data() + 2, input.data() + input.size(), address, 16);
        if (result.ec != std::errc{} || result.ptr != input.data() + input.size() ||
            (address != 0x3c && address != 0x3d))
            throw std::runtime_error("This example permits only SSD1306 addresses 0x3c and 0x3d.");
        const std::string mode = argc == 4 ? argv[3] : "dry-run";
        if (mode != "dry-run" && mode != "--show" && mode != "--off")
            throw std::runtime_error("Only --show and --off enable I2C writes.");
        if (mode == "dry-run") {
            const auto frame = greeting();
            const auto bytes = std::count_if(frame.begin(), frame.end(), [](auto b) { return b != 0; });
            std::cout << "Dry run: HELLO PI, 128x64, " << frame.size()
                      << " framebuffer bytes, " << bytes << " nonzero bytes.\n"
                         "No I2C device opened.\n";
            return 0;
        }
        Bus bus(argv[1], address);
        if (mode == "--off") {
            bus.command(0xae);
            std::cout << "PASS: Display Off command sent.\n";
        } else {
            bus.show(greeting());
            std::cout << "PASS: initialization, 1024 framebuffer bytes and Display On sent.\n"
                         "Check the panel visually; this is not pixel readback.\n"
                         "No process remains; use --off when finished.\n";
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
