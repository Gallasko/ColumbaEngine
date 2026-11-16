#include "osmodule.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#ifndef _WIN32
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char** environ;
#endif

namespace pg
{
    namespace
    {
        std::string stringArgument(VM* vm, int argCount, Value* args, const char* name)
        {
            if (argCount != 1 or not IS_STRING(args[0]))
                throw std::runtime_error(std::string("os.") + name + " expects one string");
            return vm->asString(args[0]);
        }

        bool isTable(VM* vm, Value value)
        {
            return IS_INSTANCE(value) and vm->asInstance(value)->klass == vm->asClass(vm->tableClassValue());
        }

        Value callResult(VM* vm, int code, const std::string& out, const std::string& err)
        {
            Value table = vm->createTable();

            Value outValue = vm->createString(out);
            Value errValue = vm->createString(err);

            ObjInstance* result = vm->asInstance(table);
            result->setField("code", makeIntValue(code), vm);
            result->setField("out", outValue, vm);
            result->setField("err", errValue, vm);

            return table;
        }

#ifndef _WIN32
        // A pipe whose ends are not passed on to the commands this process starts
        bool openPipe(int ends[2])
        {
#ifdef __linux__
            return pipe2(ends, O_CLOEXEC) == 0;
#else
            if (pipe(ends) != 0)
                return false;
            fcntl(ends[0], F_SETFD, FD_CLOEXEC);
            fcntl(ends[1], F_SETFD, FD_CLOEXEC);
            return true;
#endif
        }

        void closeEnd(int& fd)
        {
            if (fd >= 0)
            {
                close(fd);
                fd = -1;
            }
        }

        // Writes `input` to the command and reads what it writes, together, until both its outputs
        // end: written and read one after the other, a large input and a large output would each
        // wait for the other. SIGPIPE (a command that stops reading) is blocked here and seen as EPIPE
        void exchange(int& in, int& out, int& err, const std::string& input, std::string& outText, std::string& errText)
        {
            sigset_t pipeSignal, previous;
            sigemptyset(&pipeSignal);
            sigaddset(&pipeSignal, SIGPIPE);
            pthread_sigmask(SIG_BLOCK, &pipeSignal, &previous);

            for (int fd : {in, out, err})
                if (fd >= 0)
                    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);

            size_t written = 0;
            if (input.empty())
                closeEnd(in);

            char buffer[65536];

            while (in >= 0 or out >= 0 or err >= 0)
            {
                pollfd watched[3];
                int count = 0;
                int* owners[3];

                if (in >= 0)  { watched[count] = {in, POLLOUT, 0};  owners[count++] = &in; }
                if (out >= 0) { watched[count] = {out, POLLIN, 0};  owners[count++] = &out; }
                if (err >= 0) { watched[count] = {err, POLLIN, 0};  owners[count++] = &err; }

                if (poll(watched, count, -1) < 0)
                {
                    if (errno == EINTR)
                        continue;
                    break;
                }

                for (int i = 0; i < count; ++i)
                {
                    int& fd = *owners[i];
                    if (watched[i].revents == 0 or fd < 0)
                        continue;

                    if (&fd == &in)
                    {
                        const size_t left = input.size() - written;
                        const ssize_t n = write(fd, input.data() + written, left < sizeof(buffer) ? left : sizeof(buffer));
                        if (n > 0)
                        {
                            written += static_cast<size_t>(n);
                            if (written == input.size())
                                closeEnd(fd);
                        }
                        else if (n < 0 and errno != EAGAIN and errno != EINTR)
                            closeEnd(fd); // EPIPE: the command does not want the rest
                        continue;
                    }

                    const ssize_t n = read(fd, buffer, sizeof(buffer));
                    if (n > 0)
                        (&fd == &out ? outText : errText).append(buffer, static_cast<size_t>(n));
                    else if (n == 0 or (errno != EAGAIN and errno != EINTR))
                        closeEnd(fd);
                }
            }

            // A SIGPIPE raised while it was blocked is taken here, so it never reaches the process
            sigset_t pending;
            sigpending(&pending);
            if (sigismember(&pending, SIGPIPE))
            {
                const timespec now = {0, 0};
                sigtimedwait(&pipeSignal, nullptr, &now);
            }

            pthread_sigmask(SIG_SETMASK, &previous, nullptr);
        }
#endif
    }

    Value OsModule::nativeCall(VM* vm, int argCount, Value* args)
    {
        if (argCount != 1 and argCount != 2)
            throw std::runtime_error("os.call expects a command and, optionally, a table of options");

        // The command: a string for the shell, or the program and its arguments
        std::vector<std::string> argv;
        if (IS_STRING(args[0]))
        {
            argv = {"sh", "-c", vm->asString(args[0])};
        }
        else if (IS_VECTOR(args[0]))
        {
            for (Value part : vm->asVector(args[0])->fields)
            {
                if (not IS_STRING(part))
                    throw std::runtime_error("os.call: every element of a command list must be a string");
                argv.push_back(vm->asString(part));
            }

            if (argv.empty())
                throw std::runtime_error("os.call: a command list needs at least the program");
        }
        else
        {
            throw std::runtime_error("os.call: the command is a string (run by /bin/sh) or a list of strings (run directly)");
        }

        const bool throughShell = IS_STRING(args[0]);

        std::string input, cwd;
        bool hasCwd = false;

        if (argCount == 2)
        {
            if (not isTable(vm, args[1]))
                throw std::runtime_error("os.call: the options are a table, {input: ..., cwd: ...}");

            ObjInstance* options = vm->asInstance(args[1]);
            if (options->intCount() > 0)
                throw std::runtime_error("os.call: the options take only the keys input and cwd");

            for (size_t slot = 0; slot < options->fieldCount(); ++slot)
            {
                const std::string& key = options->fieldName(slot);
                const Value value = options->fieldValue(slot);

                if (IS_UNDEFINED(value))
                    continue;

                if (key != "input" and key != "cwd")
                    throw std::runtime_error("os.call: unknown option '" + key + "' (input and cwd only)");

                if (not IS_STRING(value))
                    throw std::runtime_error("os.call: the option '" + key + "' must be a string");

                if (key == "input")
                {
                    input = vm->asString(value);
                }
                else
                {
                    cwd = vm->asString(value);
                    hasCwd = true;
                }
            }
        }

#ifdef _WIN32
        (void)throughShell;
        (void)hasCwd;
        throw std::runtime_error("os.call is not available on Windows yet");
#else
        if (hasCwd)
        {
            struct stat info;
            if (stat(cwd.c_str(), &info) != 0 or not S_ISDIR(info.st_mode))
                return callResult(vm, 127, "", "os.call: cannot run in '" + cwd + "': not a folder");
        }

        int in[2] = {-1, -1}, out[2] = {-1, -1}, err[2] = {-1, -1};
        if (not openPipe(in) or not openPipe(out) or not openPipe(err))
        {
            const std::string reason = std::strerror(errno);
            for (int* ends : {in, out, err})
            {
                closeEnd(ends[0]);
                closeEnd(ends[1]);
            }
            return callResult(vm, 127, "", "os.call: cannot open pipes: " + reason);
        }

        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_adddup2(&actions, in[0], STDIN_FILENO);
        posix_spawn_file_actions_adddup2(&actions, out[1], STDOUT_FILENO);
        posix_spawn_file_actions_adddup2(&actions, err[1], STDERR_FILENO);
        if (hasCwd)
            posix_spawn_file_actions_addchdir_np(&actions, cwd.c_str());

        // The command starts with SIGPIPE at its default and this thread's signal mask, whatever
        // this process does with them
        posix_spawnattr_t attributes;
        posix_spawnattr_init(&attributes);
        sigset_t defaults, mask;
        sigemptyset(&defaults);
        sigaddset(&defaults, SIGPIPE);
        pthread_sigmask(SIG_SETMASK, nullptr, &mask);
        posix_spawnattr_setsigdefault(&attributes, &defaults);
        posix_spawnattr_setsigmask(&attributes, &mask);
        posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETSIGMASK);

        std::vector<char*> pointers;
        for (auto& part : argv)
            pointers.push_back(part.data());
        pointers.push_back(nullptr);

        pid_t pid = 0;
        const int spawned = throughShell
            ? posix_spawn(&pid, "/bin/sh", &actions, &attributes, pointers.data(), environ)
            : posix_spawnp(&pid, pointers[0], &actions, &attributes, pointers.data(), environ);

        posix_spawn_file_actions_destroy(&actions);
        posix_spawnattr_destroy(&attributes);

        closeEnd(in[0]);
        closeEnd(out[1]);
        closeEnd(err[1]);

        if (spawned != 0)
        {
            closeEnd(in[1]);
            closeEnd(out[0]);
            closeEnd(err[0]);
            return callResult(vm, 127, "", "os.call: cannot start " + argv[throughShell ? 2 : 0] + ": " + std::strerror(spawned));
        }

        std::string outText, errText;
        exchange(in[1], out[0], err[0], input, outText, errText);

        int status = 0;
        while (waitpid(pid, &status, 0) < 0 and errno == EINTR)
        {
        }

        int code = -1;
        if (WIFEXITED(status))
            code = WEXITSTATUS(status);
        else if (WIFSIGNALED(status))
            code = 128 + WTERMSIG(status);

        return callResult(vm, code, outText, errText);
#endif
    }

    Value OsModule::nativeEnv(VM* vm, int argCount, Value* args)
    {
        const std::string name = stringArgument(vm, argCount, args, "env");
        const char* value = std::getenv(name.c_str());
        return value ? vm->createString(std::string(value)) : makeNoneValue();
    }

    Value OsModule::nativeTime(VM*, int argCount, Value*)
    {
        if (argCount != 0)
            throw std::runtime_error("os.time takes no argument");

        const auto since = std::chrono::system_clock::now().time_since_epoch();
        return makeIntValue(std::chrono::duration_cast<std::chrono::milliseconds>(since).count());
    }

    Value OsModule::nativeFormatTime(VM* vm, int argCount, Value* args)
    {
        if (argCount != 2 or not (IS_INT(args[0]) or IS_DOUBLE(args[0])) or not IS_STRING(args[1]))
            throw std::runtime_error("os.formatTime expects a time in milliseconds and a format string");

        const double ms = IS_INT(args[0]) ? static_cast<double>(AS_INT(args[0])) : AS_DOUBLE(args[0]);
        const std::time_t seconds = static_cast<std::time_t>(std::floor(ms / 1000.0));
        const std::string format = vm->asString(args[1]);

        std::tm utc{};
#ifdef _WIN32
        if (gmtime_s(&utc, &seconds) != 0)
#else
        if (gmtime_r(&seconds, &utc) == nullptr)
#endif
            throw std::runtime_error("os.formatTime: a time out of range");

        if (format.empty())
            return vm->createString(std::string());

        char buffer[256];
        const size_t length = std::strftime(buffer, sizeof(buffer), format.c_str(), &utc);
        if (length == 0)
            throw std::runtime_error("os.formatTime: the result is longer than 255 characters");

        return vm->createString(std::string(buffer, length));
    }

    Value OsModule::nativeListDir(VM* vm, int argCount, Value* args)
    {
        const std::string path = stringArgument(vm, argCount, args, "listDir");

        std::error_code error;
        if (not std::filesystem::is_directory(path, error))
            throw std::runtime_error("os.listDir: '" + path + "' is not a folder");

        std::vector<std::string> names;
        for (const auto& entry : std::filesystem::directory_iterator(path, error))
            names.push_back(entry.path().filename().string());

        if (error)
            throw std::runtime_error("os.listDir: cannot read '" + path + "': " + error.message());

        std::sort(names.begin(), names.end());

        Value list = vm->createVector();
        for (const auto& name : names)
        {
            Value item = vm->createString(name);
            vm->asVector(list)->fields.push_back(item);
        }
        return list;
    }

    Value OsModule::nativeIsDir(VM* vm, int argCount, Value* args)
    {
        const std::string path = stringArgument(vm, argCount, args, "isDir");
        std::error_code error;
        return makeBoolValue(std::filesystem::is_directory(path, error));
    }

    Value OsModule::nativeMakeDir(VM* vm, int argCount, Value* args)
    {
        const std::string path = stringArgument(vm, argCount, args, "makeDir");

        std::error_code error;
        std::filesystem::create_directories(path, error);
        if (error or not std::filesystem::is_directory(path))
            throw std::runtime_error("os.makeDir: cannot make '" + path + "'" + (error ? ": " + error.message() : std::string(": something else is there")));

        return makeNoneValue();
    }

    Value OsModule::nativeExit(VM*, int argCount, Value* args)
    {
        if (argCount != 1 or not IS_INT(args[0]))
            throw std::runtime_error("os.exit expects an int exit code");

        std::cout.flush();
        std::cerr.flush();
        std::exit(static_cast<int>(AS_INT(args[0])));
    }
}
