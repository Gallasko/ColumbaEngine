#include "stdafx.h"

#include "wslgamepad.h"

#include <atomic>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>

#include "logger.h"

#if defined(__linux__) && !defined(__EMSCRIPTEN__)
#include <csignal>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;
#endif

namespace pg
{
    namespace
    {
        constexpr char const * DOM = "WslGamepad";
    }

    struct WslGamepad::Shared
    {
        std::mutex mutex;
        std::vector<Event> events;
        std::atomic<bool> alive {true};
    };

    std::vector<WslGamepad::Event> WslGamepad::take()
    {
        std::vector<Event> out;
        if (shared)
        {
            std::lock_guard<std::mutex> lock(shared->mutex);
            out.swap(shared->events);
        }
        return out;
    }

    bool WslGamepad::alive() const { return shared and shared->alive.load(); }

#if defined(__linux__) && !defined(__EMSCRIPTEN__)

    namespace
    {
        // The Windows side. It polls XInput's four slots every 8 ms and writes what changed: "B slot button 1|0" (SDL's
        // button numbers), "A slot axis value" (SDL's axes and ranges: Y down, triggers 0..32767), "C slot" / "D slot" as
        // a controller comes and goes (going: its buttons released and its axes back to 0 first). It reads "V slot low
        // high ms" (rumble, 0..65535 each motor) from its input; a closed input ends it, the motors stopped.
        // C# 5 only: the compiler behind Add-Type is the one of the .NET Framework.
        const char* helperScript = R"PS(
Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;

public static class PgPad
{
    [StructLayout(LayoutKind.Sequential)] struct Pad { public ushort Buttons; public byte LeftTrigger; public byte RightTrigger; public short LX; public short LY; public short RX; public short RY; }
    [StructLayout(LayoutKind.Sequential)] struct State { public uint Packet; public Pad Pad; }
    [StructLayout(LayoutKind.Sequential)] struct Vibration { public ushort Left; public ushort Right; }
    [DllImport("xinput1_4.dll")] static extern uint XInputGetState(uint slot, out State state);
    [DllImport("xinput1_4.dll")] static extern uint XInputSetState(uint slot, ref Vibration vibration);
    [DllImport("winmm.dll")] static extern uint timeBeginPeriod(uint ms);

    // XInput's button bits, in SDL's order (SDL_GameControllerButton: A, B, X, Y, back, guide, start, left stick,
    // right stick, left shoulder, right shoulder, d-pad up, down, left, right); 0: none (the guide)
    static readonly int[] Bits = { 0x1000, 0x2000, 0x4000, 0x8000, 0x20, 0, 0x10, 0x40, 0x80, 0x100, 0x200, 0x1, 0x2, 0x4, 0x8 };

    static readonly object Lock = new object();
    static StreamWriter output;
    static volatile bool closed;
    static readonly long[] rumbleUntil = new long[4];

    static void Say(string line)
    {
        lock (Lock) { output.WriteLine(line); output.Flush(); }
    }

    static int Stick(short v, bool flip)
    {
        int n = flip ? -(int)v - 1 : (int)v;
        return Math.Max(-32768, Math.Min(32767, n));
    }

    public static void Run()
    {
        timeBeginPeriod(1);
        var clock = System.Diagnostics.Stopwatch.StartNew();
        var input = new StreamReader(Console.OpenStandardInput());
        output = new StreamWriter(Console.OpenStandardOutput());
        output.NewLine = "\n";

        var reader = new Thread(delegate() {
            string line;
            while ((line = input.ReadLine()) != null)
            {
                string[] p = line.Split(' ');
                if (p.Length == 5 && p[0] == "V")
                {
                    uint slot = uint.Parse(p[1]);
                    if (slot > 3) continue;
                    var v = new Vibration { Left = ushort.Parse(p[2]), Right = ushort.Parse(p[3]) };
                    XInputSetState(slot, ref v);
                    Interlocked.Exchange(ref rumbleUntil[slot], clock.ElapsedMilliseconds + long.Parse(p[4]));
                }
            }
            closed = true;
        });
        reader.IsBackground = true;
        reader.Start();

        Say("ready");

        var connected = new bool[4];
        var buttons = new ushort[4];
        var axes = new int[4, 6];

        while (!closed)
        {
            for (uint slot = 0; slot < 4; ++slot)
            {
                State s;
                bool here = XInputGetState(slot, out s) == 0;

                if (here != connected[slot])
                {
                    if (!here)
                    {
                        for (int b = 0; b < Bits.Length; ++b)
                            if (Bits[b] != 0 && (buttons[slot] & Bits[b]) != 0)
                                Say("B " + slot + " " + b + " 0");
                        for (int a = 0; a < 6; ++a)
                            if (axes[slot, a] != 0)
                                Say("A " + slot + " " + a + " 0");
                        buttons[slot] = 0;
                        for (int a = 0; a < 6; ++a)
                            axes[slot, a] = 0;
                    }
                    connected[slot] = here;
                    Say((here ? "C " : "D ") + slot);
                }

                if (!here)
                    continue;

                ushort now = s.Pad.Buttons;
                for (int b = 0; b < Bits.Length; ++b)
                    if (Bits[b] != 0 && ((now ^ buttons[slot]) & Bits[b]) != 0)
                        Say("B " + slot + " " + b + ((now & Bits[b]) != 0 ? " 1" : " 0"));
                buttons[slot] = now;

                int[] v = { Stick(s.Pad.LX, false), Stick(s.Pad.LY, true), Stick(s.Pad.RX, false), Stick(s.Pad.RY, true),
                            s.Pad.LeftTrigger * 32767 / 255, s.Pad.RightTrigger * 32767 / 255 };
                for (int a = 0; a < 6; ++a)
                    if (Math.Abs(v[a] - axes[slot, a]) > 200 || (v[a] == 0 && axes[slot, a] != 0))
                    {
                        axes[slot, a] = v[a];
                        Say("A " + slot + " " + a + " " + v[a]);
                    }

                // A rumble played its length: the motors stop
                long until = Interlocked.Read(ref rumbleUntil[slot]);
                if (until > 0 && clock.ElapsedMilliseconds >= until)
                {
                    var off = new Vibration();
                    XInputSetState(slot, ref off);
                    Interlocked.Exchange(ref rumbleUntil[slot], 0);
                }
            }

            Thread.Sleep(8);
        }

        for (uint slot = 0; slot < 4; ++slot)
        {
            var off = new Vibration();
            XInputSetState(slot, ref off);
        }
    }
}
'@
[PgPad]::Run()
)PS";

        // powershell -EncodedCommand takes the script as base64 of its UTF-16LE text
        std::string encodeCommand(const std::string& script)
        {
            static const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

            std::string utf16;
            utf16.reserve(script.size() * 2);

            for (char c : script)
            {
                utf16 += c;
                utf16 += '\0';
            }

            std::string out;
            out.reserve((utf16.size() + 2) / 3 * 4);

            for (size_t i = 0; i < utf16.size(); i += 3)
            {
                const size_t left = utf16.size() - i;
                uint32_t n = static_cast<uint8_t>(utf16[i]) << 16;

                if (left > 1) n |= static_cast<uint8_t>(utf16[i + 1]) << 8;
                if (left > 2) n |= static_cast<uint8_t>(utf16[i + 2]);

                out += table[(n >> 18) & 63];
                out += table[(n >> 12) & 63];
                out += left > 1 ? table[(n >> 6) & 63] : '=';
                out += left > 2 ? table[n & 63] : '=';
            }

            return out;
        }

        void readHelper(std::shared_ptr<WslGamepad::Shared> shared, int fromHelper)
        {
            std::string pending;
            char buffer[512];

            for (;;)
            {
                const ssize_t n = read(fromHelper, buffer, sizeof(buffer));

                if (n < 0 and errno == EINTR)
                    continue;

                if (n <= 0)
                    break;

                pending.append(buffer, static_cast<size_t>(n));

                size_t end;

                while ((end = pending.find('\n')) != std::string::npos)
                {
                    const std::string line = pending.substr(0, end);
                    pending.erase(0, end + 1);

                    if (line == "ready")
                    {
                        LOG_INFO(DOM, "Windows gamepad helper ready");
                        continue;
                    }

                    if (line.size() > 2 and (line[0] == 'C' or line[0] == 'D'))
                    {
                        LOG_INFO(DOM, "Gamepad " << (line[0] == 'C' ? "connected" : "disconnected") << " in slot " << line.substr(2));
                        continue;
                    }

                    if (line.size() < 7 or (line[0] != 'B' and line[0] != 'A'))
                        continue;

                    char* rest = nullptr;
                    const long slot = std::strtol(line.c_str() + 2, &rest, 10);
                    const long which = std::strtol(rest, &rest, 10);
                    const long value = std::strtol(rest, nullptr, 10);

                    WslGamepad::Event e;
                    e.id = WslGamepad::ID_BASE + static_cast<int>(slot);
                    e.which = static_cast<int>(which);
                    e.value = static_cast<int>(value);
                    e.kind = line[0] == 'A' ? WslGamepad::Event::Kind::Axis : (value != 0 ? WslGamepad::Event::Kind::Pressed : WslGamepad::Event::Kind::Released);

                    std::lock_guard<std::mutex> lock(shared->mutex);
                    shared->events.push_back(e);
                }
            }

            shared->alive = false;
            close(fromHelper);
        }
    }

    std::unique_ptr<WslGamepad> WslGamepad::start()
    {
        int toChild[2], fromChild[2];

        if (pipe2(toChild, O_CLOEXEC) != 0)
            return nullptr;

        if (pipe2(fromChild, O_CLOEXEC) != 0)
        {
            close(toChild[0]);
            close(toChild[1]);
            return nullptr;
        }

        // A write to a helper that died must not kill the game
        std::signal(SIGPIPE, SIG_IGN);

        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_adddup2(&actions, toChild[0], 0);
        posix_spawn_file_actions_adddup2(&actions, fromChild[1], 1);
        posix_spawn_file_actions_addopen(&actions, 2, "/dev/null", O_WRONLY, 0);

        const std::string encoded = encodeCommand(helperScript);
        const char* system32 = "/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe";
        const char* program = access(system32, X_OK) == 0 ? system32 : "powershell.exe";

        char* argv[] = {const_cast<char*>(program), const_cast<char*>("-NoProfile"), const_cast<char*>("-NonInteractive"),
                        const_cast<char*>("-ExecutionPolicy"), const_cast<char*>("Bypass"),
                        const_cast<char*>("-EncodedCommand"), const_cast<char*>(encoded.c_str()), nullptr};

        pid_t pid = -1;
        const int error = posix_spawnp(&pid, program, &actions, nullptr, argv, environ);

        posix_spawn_file_actions_destroy(&actions);
        close(toChild[0]);
        close(fromChild[1]);

        if (error != 0)
        {
            LOG_ERROR(DOM, "Could not start powershell.exe for gamepads on WSL (error " << error << ")");
            close(toChild[1]);
            close(fromChild[0]);
            return nullptr;
        }

        // A frame never waits on the helper: a rumble that does not fit is dropped
        fcntl(toChild[1], F_SETFL, fcntl(toChild[1], F_GETFL) | O_NONBLOCK);

        std::unique_ptr<WslGamepad> pad(new WslGamepad());
        pad->shared = std::make_shared<Shared>();
        pad->toHelper = toChild[1];
        pad->pid = pid;

        std::thread(readHelper, pad->shared, fromChild[0]).detach();

        LOG_INFO(DOM, "Started the Windows gamepad helper (XInput) on WSL");

        return pad;
    }

    WslGamepad::~WslGamepad()
    {
        // Closing its input ends the helper (its motors stopped)
        if (toHelper >= 0)
            close(toHelper);

        if (pid > 0)
        {
            for (int i = 0; i < 20; ++i)
            {
                if (waitpid(pid, nullptr, WNOHANG) != 0)
                    return;

                usleep(10000);
            }

            kill(pid, SIGKILL);
            waitpid(pid, nullptr, 0);
        }
    }

    void WslGamepad::rumble(int id, float low, float high, int ms)
    {
        const int slot = id - ID_BASE;
        if (toHelper < 0 or slot < 0 or slot > 3)
            return;

        auto motor = [](float v) { return static_cast<int>(std::min(1.0f, std::max(0.0f, v)) * 65535.0f); };
        const std::string line = "V " + std::to_string(slot) + " " + std::to_string(motor(low)) + " " + std::to_string(motor(high)) + " " + std::to_string(std::max(0, ms)) + "\n";
        const ssize_t written = write(toHelper, line.data(), line.size());
        (void)written;
    }

#else

    std::unique_ptr<WslGamepad> WslGamepad::start() { return nullptr; }

    WslGamepad::~WslGamepad() = default;

    void WslGamepad::rumble(int, float, float, int) {}

#endif
}
