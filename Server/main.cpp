#include <iostream>
#include <thread>
#include "CommandProcessor.h"
#include "AOFLogger.h"
#include "LogConfig.h"
#include "LogRunner.h"
#include "LogCompactor.h"
#include "Executor.h"
#include "StorageEngine.h"
#include "Server.h"
#include "ExpirationManager.h"
int main() {
    try {
        constexpr std::uint16_t kServerPort = Server::kDefaultPort;
        const std::string aof_path(LogConfig::kDefaultAofPath);
        Tokenizer tokenizer;
        Parser parser;
        StorageEngine storage;
        Executor executor(storage);
        CommandProcessor command_processor(executor);
        AOFLogger logger(aof_path);
        LogRunner log_runner(aof_path);
        LogCompactor compactor(aof_path, tokenizer, parser);
        compactor.compact();
        log_runner.run_log(command_processor);
        Server server(logger, command_processor, kServerPort);
        ExpirationManager exp_manager(10000);
        std::thread exp_thread{&ExpirationManager::expiration_thread, &exp_manager,  std::ref(storage) };

        server.run();
        exp_manager.shutdown();
        exp_thread.join();
    } catch (const std::exception& error) {
        
        std::cerr << error.what() << std::endl;
        return 1;
    }
    

    return 0;
}
