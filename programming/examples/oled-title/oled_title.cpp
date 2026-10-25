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
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * I2C initialization/transfer adapted from ohyaan/ssd1306 and oled_hello.
 * Animation and compact title glyphs are defined here, not an external font.
 * Recreates the site's title effect; not the recovered original demo source.
 */
#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <linux/i2c-dev.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/ioctl.h>
#include <thread>
#include <unistd.h>
#include <vector>

constexpr int width = 128;
constexpr int height = 64;
constexpr unsigned cycle_frames = 128;
using Frame = std::array<std::uint8_t, width * height / 8>;
struct Point {
    int x;
    int y;
};

volatile std::sig_atomic_t interrupted = 0;
extern "C" void stop_animation(int) {
    interrupted = 1;
}

void pixel(Frame& frame, int x, int y) {
    if (x >= 0 && x < width && y >= 0 && y < height) {
        frame[static_cast<std::size_t>(x + y / 8 * width)] |=
            static_cast<std::uint8_t>(1u << (y % 8));
    }
}

bool lit(const Frame& frame, int x, int y) {
    return (frame[static_cast<std::size_t>(x + y / 8 * width)] &
            (1u << (y % 8))) != 0;
}

std::array<unsigned, 7> glyph(char c) {
    // Four-bit rows (five for W), most significant bit on the left.
    switch (c) {
    case 'W':
        return {17, 17, 17, 21, 21, 21, 10};
    case 'I':
        return {15, 6, 6, 6, 6, 6, 15};
    case 'T':
        return {15, 6, 6, 6, 6, 6, 6};
    case 'H':
        return {9, 9, 9, 15, 9, 9, 9};
    case 'R':
        return {14, 9, 9, 14, 10, 9, 9};
    case 'A':
        return {6, 9, 9, 15, 9, 9, 9};
    case 'S':
        return {7, 8, 8, 6, 1, 1, 14};
    case 'P':
        return {14, 9, 9, 14, 8, 8, 8};
    case 'B':
        return {14, 9, 9, 14, 9, 9, 14};
    case 'E':
        return {15, 8, 8, 14, 8, 8, 15};
    case 'Y':
        return {9, 9, 9, 6, 6, 6, 6};
    case ' ':
        return {};
    default:
        throw std::runtime_error("Only WITH RASPBERRY PI glyphs are defined.");
    }
}

std::vector<Point> title_pixels() {
    Frame title{};
    const std::array<std::string_view, 2> lines{"WITH", "RASPBERRY PI"};
    for (std::size_t line = 0; line < lines.size(); ++line) {
        const auto text = lines[line];
        int text_width = -2;
        for (char c : text) {
            text_width += (c == 'W' ? 5 : 4) * 2 + 2;
        }
        int x = (width - text_width) / 2;
        const int y = line == 0 ? 12 : 34;
        for (char c : text) {
            const auto rows = glyph(c);
            for (int row = 0; row < 7; ++row) {
                for (int column = 0; column < (c == 'W' ? 5 : 4); ++column) {
                    if (rows[static_cast<std::size_t>(row)] &
                        ((c == 'W' ? 16u : 8u) >> column)) {
                        for (int dy = 0; dy < 2; ++dy) {
                            for (int dx = 0; dx < 2; ++dx) {
                                pixel(title, x + column * 2 + dx,
                                      y + row * 2 + dy);
                            }
                        }
                    }
                }
            }
            x += (c == 'W' ? 5 : 4) * 2 + 2;
        }
    }
    std::vector<Point> result;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (lit(title, x, y)) {
                result.push_back({x, y});
            }
        }
    }
    return result;
}

std::uint32_t mix(std::uint32_t value) {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

Frame render(unsigned number, const std::vector<Point>& targets) {
    Frame frame{};
    const unsigned phase = number % cycle_frames;
    for (unsigned star = 0; star < 24; ++star) {
        const auto seed = mix(star + 19);
        const int x = static_cast<int>(
            (seed % width + phase * (1 + star % 3) / 4) % width);
        const int y =
            static_cast<int>(((seed >> 8) % height + phase / 8) % height);
        // Keep the two held title lines readable; stars are background only.
        if (phase >= 48 && phase < 96 && y >= 10 && y <= 49) {
            continue;
        }
        if ((phase / 4 + star) % 4 != 0) {
            pixel(frame, x, y);
        }
    }
    if (phase < 16 || phase >= 112) {
        return frame;
    }
    const unsigned t =
        phase < 48 ? phase - 16 : (phase < 96 ? 32 : (112 - phase) * 2);
    const int eased = static_cast<int>(t * t * (96 - 2 * t));
    constexpr int denominator = 32 * 32 * 32;
    for (std::size_t i = 0; i < targets.size(); ++i) {
        const auto seed = mix(static_cast<std::uint32_t>(i) + 937);
        const int start_x = static_cast<int>(seed % width);
        const int start_y = static_cast<int>((seed >> 8) % height);
        const auto target = targets[i];
        pixel(frame, start_x + (target.x - start_x) * eased / denominator,
              start_y + (target.y - start_y) * eased / denominator);
    }
    return frame;
}

void preview(const Frame& frame) {
    // PBM P4 uses row-major, most-significant-bit-first pixels, unlike SSD1306.
    std::cout << "P4\n128 64\n";
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; x += 8) {
            std::uint8_t byte = 0;
            for (int bit = 0; bit < 8; ++bit) {
                if (lit(frame, x + bit, y)) {
                    byte |= static_cast<std::uint8_t>(0x80u >> bit);
                }
            }
            std::cout.put(static_cast<char>(byte));
        }
    }
    if (!std::cout) {
        throw std::runtime_error("Write preview failed.");
    }
}

class Bus {
  public:
    Bus(const char* device, unsigned address) {
        fd_ = open(device, O_RDWR | O_CLOEXEC);
        if (fd_ < 0) {
            throw std::runtime_error(std::string("Open I2C: ") +
                                     std::strerror(errno));
        }
        if (ioctl(fd_, I2C_SLAVE, address) < 0) {
            const std::string error = std::strerror(errno);
            close(fd_);
            throw std::runtime_error("Select I2C address: " + error);
        }
    }
    ~Bus() {
        // Best-effort cleanup during an exception. A disconnected bus can fail.
        if (touched_) {
            try {
                command(0xae);
            } catch (...) {
            }
        }
        close(fd_);
    }
    Bus(const Bus&) = delete;
    Bus& operator=(const Bus&) = delete;

    void command(std::uint8_t value) {
        touched_ = true;
        const std::array<std::uint8_t, 2> bytes{0x00, value};
        transfer(bytes.data(), bytes.size());
    }
    void initialize() {
        constexpr std::uint8_t commands[] = {
            0xae, 0xd5, 0x80, 0xa8, 0x3f, 0xd3, 0x00, 0x40, 0x8d,
            0x14, 0x20, 0x00, 0xa1, 0xc8, 0xda, 0x12, 0x81, 0x30,
            0xd9, 0xf1, 0xdb, 0x40, 0xa4, 0xa6, 0x2e};
        for (auto value : commands) {
            command(value);
        }
    }
    void show(const Frame& frame) {
        // Reset the full addressing window for each 1024-byte frame.
        for (auto value : {0x21, 0x00, 0x7f, 0x22, 0x00, 0x07}) {
            command(static_cast<std::uint8_t>(value));
        }
        for (std::size_t offset = 0; offset < frame.size();) {
            std::array<std::uint8_t, 32> chunk{};
            chunk[0] = 0x40;
            const auto count = std::min<std::size_t>(31, frame.size() - offset);
            std::copy_n(frame.data() + offset, count, chunk.data() + 1);
            transfer(chunk.data(), count + 1);
            offset += count;
        }
    }
    void off() {
        command(0xae);
        touched_ = false;
    }

  private:
    int fd_ = -1;
    bool touched_ = false;
    void transfer(const std::uint8_t* bytes, std::size_t count) {
        const auto written = write(fd_, bytes, count);
        if (written < 0) {
            throw std::runtime_error(std::string("I2C write: ") +
                                     std::strerror(errno));
        }
        if (written != static_cast<ssize_t>(count)) {
            throw std::runtime_error(
                "Short I2C write; stop and check the connection.");
        }
    }
};

unsigned number(std::string_view value, unsigned minimum, unsigned maximum) {
    unsigned result = 0;
    const auto parsed =
        std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
        result < minimum || result > maximum) {
        throw std::runtime_error(
            "Numeric argument outside its permitted range.");
    }
    return result;
}

int main(int argc, char* argv[]) {
    try {
        if (argc < 3) {
            throw std::runtime_error(
                "Usage: oled_title DEVICE 0x3c|0x3d [--show|--off|--preview "
                "FRAME] [--frames N] [--fps N]");
        }
        const std::string_view address_text(argv[2]);
        if (address_text != "0x3c" && address_text != "0x3d") {
            throw std::runtime_error(
                "Use the verified 7-bit address 0x3c or 0x3d.");
        }
        const unsigned address = address_text == "0x3c" ? 0x3c : 0x3d;
        std::string_view mode = "dry-run";
        unsigned frames = cycle_frames;
        unsigned fps = 16;
        unsigned preview_frame = 64;
        bool frames_set = false;
        bool fps_set = false;
        for (int i = 3; i < argc; ++i) {
            const std::string_view flag(argv[i]);
            if (flag == "--show" || flag == "--off" || flag == "--preview") {
                if (mode != "dry-run") {
                    throw std::runtime_error("Choose only one output mode.");
                }
                mode = flag;
                if (flag == "--preview") {
                    if (++i >= argc) {
                        throw std::runtime_error("Missing preview frame.");
                    }
                    preview_frame = number(argv[i], 0, cycle_frames - 1);
                }
            } else if (flag == "--frames" || flag == "--fps") {
                if (++i >= argc) {
                    throw std::runtime_error("Missing numeric argument.");
                }
                bool& specified = flag == "--frames" ? frames_set : fps_set;
                if (specified) {
                    throw std::runtime_error("Repeated numeric option.");
                }
                specified = true;
                if (flag == "--frames") {
                    frames = number(argv[i], 1, 600);
                } else {
                    fps = number(argv[i], 1, 30);
                }
            } else {
                throw std::runtime_error("Unknown option.");
            }
        }
        if ((mode == "--off" || mode == "--preview") &&
            (frames_set || fps_set)) {
            throw std::runtime_error(
                "Frame count and rate are for animation or dry run only.");
        }
        if (mode != "--off" && mode != "--preview" && frames > fps * 30) {
            throw std::runtime_error(
                "Requested animation must be no longer than 30 seconds.");
        }
        const auto targets = title_pixels();
        if (mode == "--preview") {
            preview(render(preview_frame, targets));
            return 0;
        }
        if (mode == "dry-run") {
            std::uint32_t checksum = 2166136261u;
            for (unsigned f = 0; f < frames; ++f) {
                for (auto byte : render(f, targets)) {
                    checksum = (checksum ^ byte) * 16777619u;
                }
            }
            std::cout << "Dry run: WITH / RASPBERRY PI, 128x64, " << frames
                      << " frames, target " << fps << " fps, " << Frame{}.size()
                      << " framebuffer bytes.\nchecksum=" << checksum
                      << "\nNo I2C device opened.\n";
            return 0;
        }
        Bus bus(argv[1], address);
        if (mode == "--off") {
            bus.off();
            std::cout << "Display Off command sent.\n";
            return 0;
        }
        std::signal(SIGINT, stop_animation);
        std::signal(SIGTERM, stop_animation);
        using Clock = std::chrono::steady_clock;
        const auto period = std::chrono::microseconds(1000000 / fps);
        auto next = Clock::now();
        const auto deadline = next + std::chrono::seconds(30);
        bus.initialize();
        unsigned sent = 0;
        for (; sent < frames && !interrupted && Clock::now() < deadline;
             ++sent) {
            bus.show(render(sent, targets));
            if (sent == 0) {
                bus.command(0xaf);
            }
            next = std::max(next + period, Clock::now());
            // Short sleeps make Ctrl+C responsive without doing I2C in a
            // handler.
            while (!interrupted && Clock::now() < next) {
                std::this_thread::sleep_until(std::min(
                    next, Clock::now() + std::chrono::milliseconds(10)));
            }
        }
        bus.off();
        std::cout
            << "frames_sent=" << sent << " target_fps=" << fps << " stopped="
            << (interrupted ? "signal"
                            : (sent == frames ? "complete" : "deadline"))
            << "\nDisplay Off command sent; no background process remains.\n"
               "I2C writes are not pixel readback or an achieved-FPS "
               "measurement.\n";
        return interrupted ? 130 : (sent == frames ? 0 : 1);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
