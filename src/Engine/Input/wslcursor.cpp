#include "stdafx.h"

#include "wslcursor.h"

#include <atomic>
#include <cstdlib>
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
        constexpr char const * DOM = "WslCursor";
    }

    struct WslCursor::Shared
    {
        std::atomic<bool> held {false};
        std::atomic<bool> alive {true};
        std::atomic<int> dx {0};
        std::atomic<int> dy {0};
    };

    bool runningOnWsl()
    {
        static const bool wsl = std::getenv("WSL_INTEROP") != nullptr or std::getenv("WSL_DISTRO_NAME") != nullptr;
        return wsl;
    }

    bool WslCursor::held() const { return shared and shared->held.load(); }

    bool WslCursor::alive() const { return shared and shared->alive.load(); }

    void WslCursor::takeMotion(int& dx, int& dy)
    {
        dx = shared ? shared->dx.exchange(0) : 0;
        dy = shared ? shared->dy.exchange(0) : 0;
    }

#if defined(__linux__) && !defined(__EMSCRIPTEN__)

    namespace
    {
        // The Windows side. A heartbeat line "L" holds the cursor for 250 ms, "U" lets it go at once and
        // a closed stdin ends the helper. It answers "E" when it starts holding, "R" when it lets go
        // and "M dx dy" for each move it took back. A window other than the one it started holding on
        // coming to the front lets go until the game stops asking (it lost focus) and asks again.
        // C# 5 only: the compiler behind Add-Type is the one of the .NET Framework.
        const char* helperScript = R"PS(
Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;

public static class PgCursor
{
    [StructLayout(LayoutKind.Sequential)] struct Pt { public int X; public int Y; }
    [DllImport("user32.dll")] static extern bool GetCursorPos(out Pt p);
    [DllImport("user32.dll")] static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
    [DllImport("winmm.dll")] static extern uint timeBeginPeriod(uint ms);

    static long beat = long.MinValue / 2;
    static volatile bool closed;

    public static void Run()
    {
        timeBeginPeriod(1);
        var clock = System.Diagnostics.Stopwatch.StartNew();
        var input = new StreamReader(Console.OpenStandardInput());
        var output = new StreamWriter(Console.OpenStandardOutput());
        output.NewLine = "\n";

        var reader = new Thread(delegate() {
            string line;
            while ((line = input.ReadLine()) != null)
                Interlocked.Exchange(ref beat, line == "L" ? clock.ElapsedMilliseconds : long.MinValue / 2);
            closed = true;
        });
        reader.IsBackground = true;
        reader.Start();

        output.WriteLine("ready");
        output.Flush();

        bool held = false, blocked = false;
        Pt anchor = new Pt();
        IntPtr front = IntPtr.Zero;

        while (!closed)
        {
            bool want = clock.ElapsedMilliseconds - Interlocked.Read(ref beat) < 250;

            if (!want)
                blocked = false;

            if (held && (!want || GetForegroundWindow() != front))
            {
                held = false;
                blocked = want;
                output.WriteLine("R");
                output.Flush();
            }
            else if (!held && want && !blocked)
            {
                if (GetCursorPos(out anchor))
                {
                    front = GetForegroundWindow();
                    held = true;
                    output.WriteLine("E");
                    output.Flush();
                }
            }
            else if (held)
            {
                Pt p;

                if (GetCursorPos(out p) && (p.X != anchor.X || p.Y != anchor.Y))
                {
                    SetCursorPos(anchor.X, anchor.Y);
                    output.WriteLine("M " + (p.X - anchor.X) + " " + (p.Y - anchor.Y));
                    output.Flush();
                }
            }

            Thread.Sleep(1);
        }
    }
}
'@
[PgCursor]::Run()
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

        void readHelper(std::shared_ptr<WslCursor::Shared> shared, int fromHelper)
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

                    if (line == "E")
                        shared->held = true;
                    else if (line == "R")
                        shared->held = false;
                    else if (line == "ready")
                        LOG_INFO(DOM, "Windows cursor helper ready");
                    else if (line.size() > 2 and line[0] == 'M')
                    {
                        char* rest = nullptr;
                        const long dx = std::strtol(line.c_str() + 2, &rest, 10);
                        const long dy = std::strtol(rest, nullptr, 10);

                        shared->dx += static_cast<int>(dx);
                        shared->dy += static_cast<int>(dy);
                    }
                }
            }

            shared->held = false;
            shared->alive = false;
            close(fromHelper);
        }
    }

    std::unique_ptr<WslCursor> WslCursor::start()
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
            LOG_ERROR(DOM, "Could not start powershell.exe for mouse capture on WSL (error " << error << ")");
            close(toChild[1]);
            close(fromChild[0]);
            return nullptr;
        }

        // A frame never waits on the helper: a heartbeat that does not fit is dropped
        fcntl(toChild[1], F_SETFL, fcntl(toChild[1], F_GETFL) | O_NONBLOCK);

        std::unique_ptr<WslCursor> cursor(new WslCursor());
        cursor->shared = std::make_shared<Shared>();
        cursor->toHelper = toChild[1];
        cursor->pid = pid;

        // Detached, it holds the shared state, not the cursor: it ends when the helper closes its output
        std::thread(readHelper, cursor->shared, fromChild[0]).detach();

        LOG_INFO(DOM, "Started the Windows cursor helper for mouse capture on WSL");

        return cursor;
    }

    WslCursor::~WslCursor()
    {
        // Closing its input ends the helper, letting go of the cursor if it held it
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

    void WslCursor::hold(bool want)
    {
        // A heartbeat each frame while wanted, one release when it stops
        if (toHelper < 0 or (not want and not sentHold))
            return;

        const char* line = want ? "L\n" : "U\n";
        const ssize_t written = write(toHelper, line, 2);

        (void)written;
        sentHold = want;
    }

#else

    std::unique_ptr<WslCursor> WslCursor::start() { return nullptr; }

    WslCursor::~WslCursor() = default;

    void WslCursor::hold(bool) {}

#endif
}
