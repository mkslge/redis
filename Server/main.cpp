#include <iostream>
#include <thread>
#include "App/CommandProcessor.h"
#include "Persistence/AOFLogger.h"
#include "Persistence/LogConfig.h"
#include "Persistence/LogRunner.h"
#include "Runtime/Executor.h"
#include "Runtime/StorageEngine.h"
#include "TCP/Server.h"
#include "Runtime/ExpirationManager.h"
int main() {
    try {
        constexpr std::uint16_t kServerPort = Server::kDefaultPort;
        const std::string aof_path(LogConfig::kDefaultAofPath);

        StorageEngine storage;
        Executor executor(storage);
        CommandProcessor command_processor(executor);
        AOFLogger logger(aof_path);
        LogRunner log_runner(aof_path);
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
