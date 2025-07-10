#include <csignal>
#include <cstring>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace shell {

extern char** environ;

struct Command {
    pid_t pid = -1;
    std::string path {};
    std::unique_ptr<char*[]> argv;
};

struct Context {
    int lastExited = 0;
};

namespace signals {
    void initSignalHandlers();
}

namespace parser {
    std::vector<Command> parse(const std::string& input);
}

namespace executor {
    void executeCmd(std::vector<Command>& cmds, Context& ctx);
}

class shell {
private:
    Context _ctx;

public:
    void mainLoop()
    {
        const std::string prompt = " > ";
        std::string line;

        while (std::cout << prompt, std::getline(std::cin, line)) {
            auto cmds = parser::parse(line);
            try {
                executor::executeCmd(cmds, _ctx);
            } catch (const std::exception& e) {
                std::cerr << "Error: " << e.what() << '\n';
            }
        }
    }
};

} // namespace shell

int main()
{
    shell::signals::initSignalHandlers();
    auto s = shell::shell();
    s.mainLoop();
}

namespace shell::parser {

/*
 * Parse input to a list of Command
 * since no support for pipe，the size of the return vec will always be 0 or 1
 */
std::vector<Command> parse(const std::string& input)
{
    // Should be a Tokenizer here but ...
    // No need since we dont support below：
    for (auto& c : input) {
        switch (c) {
        case '|':
            std::cerr << "Unsupported token: PIPE \n";
            return {};
        case '<':
        case '>':
            std::cerr << "Unsupported token: REDIRECT\n";
            return {};
        case '$':
            std::cerr << "Unsupported token: VARIABLE\n";
            return {};
        default:
            break;
        }
    }

    std::istringstream iss(input);
    auto parts = std::vector<std::string>(
        std::istream_iterator<std::string> { iss },
        std::istream_iterator<std::string> {});

    std::vector<Command> cmds;

    Command c;
    if (parts.empty())
        return cmds;

    c.path = parts[0];

    std::unique_ptr<char*[]> argv(new char*[parts.size() + 1]);

    for (size_t i = 0; i < parts.size(); ++i) {
        size_t len = parts[i].size() + 1;
        char* arg = new char[len];
        std::memcpy(arg, parts[i].c_str(), len);
        argv[i] = arg;
    }
    argv[parts.size()] = nullptr;

    c.argv = std::move(argv);
    cmds.emplace_back(std::move(c));

    return cmds;
}

}
namespace shell::executor {
void spawnProc(Command& cmd, Context& ctx);
bool isBuiltin(const std::string& cmd);
void handleBuiltin(const std::string& cmd, char* const* argv, Context& ctx);

void executeCmd(std::vector<Command>& cmds, Context& ctx)
{
    /*
     * can handle pipe & redir here
     * but no need for this lab
     */
    for (auto& cmd : cmds) {
        if (cmd.path.empty())
            continue;

        if (isBuiltin(cmd.path)) {
            handleBuiltin(cmd.path, cmd.argv.get(), ctx);
        } else {
            spawnProc(cmd, ctx);
        }
    }
}

void spawnProc(Command& cmd, Context& ctx)
{
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        // 让子进程应响应 Ctrl+C
        ::signal(SIGINT, SIG_DFL);
        execvpe(cmd.path.c_str(), cmd.argv.get(), ::environ);
        std::cerr << "execvpe failed: " << strerror(errno) << "\n";
        std::exit(EXIT_FAILURE);
    } else if (pid > 0) {
        // Parent process
        cmd.pid = pid;
        waitpid(pid, &(ctx.lastExited), 0);
    } else {
        throw std::runtime_error("fork() failed");
    }
}

bool isBuiltin(const std::string& cmd)
{
    return cmd == "cd" || cmd == "exit" || cmd == "help";
}

void handleBuiltin(const std::string& cmd, char* const* argv, Context& ctx)
{
    if (cmd == "cd") {
        const char* target = argv[1] ? argv[1] : std::getenv("HOME");
        if (!target || chdir(target) != 0) {
            std::cerr << "cd: failed to change directory\n";
        }
    } else if (cmd == "exit") {
        int code = argv[1] ? std::atoi(argv[1]) : ctx.lastExited;
        std::exit(code);
    } else if (cmd == "help") {
        std::cout << "Built-in commands:\n"
                  << "  cd [dir]    - Change directory\n"
                  << "  exit [code] - Exit shell\n"
                  << "  help        - Show this help message\n";
    }
}
}

namespace shell::signals {
void sigchldHandler(int)
{
    // SIGCHLD（"child terminated"）
    // Atcully no need we dont support background jobs (&)
    while (waitpid(-1, nullptr, WNOHANG) > 0) { }
}

void sigintHandler(int)
{
    // Handle Ctrl + C
    std::cout << "\n[INTERRUPTED] Press Ctrl+D or type 'exit' to quit\n > " << std::flush;
}

void initSignalHandlers()
{
    struct sigaction sa_chld {};
    sa_chld.sa_handler = sigchldHandler;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa_chld, nullptr);

    struct sigaction sa_int {};
    sa_int.sa_handler = sigintHandler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa_int, nullptr);
}
}