#include <iostream>
#include "CommandProcessor.h"
#include "AOFLogger.h"
#include "LogConfig.h"
#include "LogRunner.h"
#include "LogCompactor.h"
#include "Executor.h"
#include "StorageEngine.h"
#include "Server.h"
int main() {
    try {
        constexpr std::uint16_t kServerPort = Server::kDefaultPort;
        const std::string aof_path(LogConfig::kDefaultAofPath);
        StorageEngine storage;
        Executor executor(storage);
        CommandProcessor command_processor(executor);
        LogCompactor compactor(aof_path);
        compactor.compact();
        LogRunner log_runner(aof_path);
        log_runner.run_log(command_processor);
        AOFLogger logger(aof_path, AOFFsyncPolicy::EVERY_SECOND);
        Server server(logger, command_processor, storage, kServerPort);

        server.run();
    } catch (const std::exception& error) {
        
        std::cerr << error.what() << std::endl;
        return 1;
    }
    

    return 0;
}
