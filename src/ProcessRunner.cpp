#include "ProcessRunner.h"

#include <iostream>
#include <sstream>
#include <boost/process/windows.hpp>
#include <boost/filesystem.hpp>
#include <boost/asio.hpp>
#include "Settings.h"

namespace bf = boost::filesystem;
namespace ba = boost::asio;

auto ioc = ba::io_context();

ProcessRunner::ProcessRunner() : in{ioc} {
    in.close();
}

void ProcessRunner::Input(char ch) {
    if (!in.is_open()) {
        std::cout << "closed";
        return;
    } else if (ch == '\x3') {
        if (auto e = Terminate(); e) std::cerr << "Terminate error\n" << e.message() << std::endl;
    } else if (ch == '\b' && !line.empty()) {
        line.pop_back();
    } else {
        line.push_back(ch);
        if (ch == '\n') {
            Flush();
        }
    }
}

std::vector<std::string> makeArgs(const std::string& filename, const std::string& exe) {
    auto args = std::vector<std::string>{filename, "-o", exe};

    if (!Settings::settings.flags.empty()) {
        auto stream = std::stringstream(Settings::settings.flags);
        for (auto token = std::string(); stream >> token;) {
            args.emplace_back(token);
        }
    }

    if (!Settings::settings.includePath.empty()) {
        args.emplace_back("-I");
        args.emplace_back(Settings::settings.includePath);
    }

    if (!Settings::settings.libPath.empty()) {
        args.emplace_back("-L");
        args.emplace_back(Settings::settings.libPath);
    }

    return args;
}

template <typename ...Args>
std::pair<std::error_code, bp::child> spawn(const bf::path& process, Args&&... args) {
    auto err = std::error_code();
    auto child = bp::child(process, args..., err, bp::windows::create_no_window);
    return {err, std::move(child)};
}

void ProcessRunner::BuildAndRun(const std::string& filename,
                                const std::function<void(const std::string&)>& print,
                                const std::function<void(const std::string&)>& status) {
    using std::tie;
    auto err = std::error_code();
    auto out = bp::ipstream();
    auto exe = (bf::temp_directory_path() / bf::unique_path()).string();
    auto compiler = Settings::settings.lookUpCompiler
                    ? bp::search_path(Settings::settings.compiler)
                    : bf::path(Settings::settings.compilerPath) / Settings::settings.compiler;
    status("Компілюється");
    tie(err, currentProcess) = spawn(compiler, makeArgs(filename, exe), (bp::std_err & bp::std_out) > out);
    auto read = std::string();
    while (std::getline(out, read)) {
        if (!read.empty()) print(read + "\\n");
    }

    currentProcess.wait();
    if (err || currentProcess.exit_code()) {
        print("Компіляція провалилась\\n" + err.message());
        status("Помилка");
        return;
    }

    in = bp::async_pipe(ioc);
    out = bp::ipstream();
    status("Запущено");
    tie(err, currentProcess) = spawn(exe + ".exe", (bp::std_err & bp::std_out) > out, bp::std_in < in, ioc);
    while (std::getline(out, read)) {
        if (!read.empty()) print(read);
    }

    if (err) {
        print("Запуск провалено\\n" + err.message());
        status("Помилка");
    } else {
        currentProcess.wait();
        print(std::format("\\nПроцес завершився з кодом {}", currentProcess.exit_code()));
        status("Завершено");
    }

    in.async_close();
    line.clear();
}

std::error_code ProcessRunner::Terminate() {
    auto err = std::error_code();
    currentProcess.terminate(err);
    return err;
}

void ProcessRunner::Flush() {
    ba::async_write(in, ba::buffer(line), [](const std::error_code &err, std::size_t) {
        if (err) std::cerr << "Input error\n" << err.message() << std::endl;
    });
    line.clear();
}
