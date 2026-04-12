#include <iostream>
#include "Interpreter/Runtime/CommandHandler.h"
#include "Interpreter/Runtime/Executor.h"
#include "Interpreter/Runtime/StorageEngine.h"
#include "Log/AOFLogger.h"
#include "Log/LogConfig.h"
#include "Log/LogRunner.h"
#include "TCP/Server.h"

int main() {
    try {
        constexpr std::uint16_t kServerPort = Server::kDefaultPort;
        const std::string aof_path(LogConfig::kDefaultAofPath);

        StorageEngine storage;
        Executor executor(storage);
        CommandHandler command_handler(executor);
        AOFLogger logger(aof_path);
        LogRunner log_runner(aof_path);
        log_runner.run_log(command_handler);
        Server server(logger, command_handler, kServerPort);
        server.run();
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }

    return 0;
}
