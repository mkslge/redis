#include "App/CommandProcessor.h"
#include "Commands/Executor.h"
#include "Network/Server.h"
#include "Persistence/AofCompactor.h"
#include "Persistence/AofConfig.h"
#include "Persistence/AofReplayer.h"
#include "Persistence/AofWriter.h"
#include "Storage/StorageEngine.h"

#include <exception>
#include <iostream>
#include <string>

int main() {
    try {
        const std::string aof_path(AofConfig::kDefaultPath);

        // In-memory state and the command pipeline that operates on it.
        StorageEngine storage;
        Executor executor(storage);
        CommandProcessor command_processor(executor);

        // Restore state from disk before accepting clients.
        AofCompactor(aof_path).compact();
        AofReplayer(aof_path).replay(executor);

        // Persist new mutations and serve clients.
        AofWriter aof_writer(aof_path, AofFsyncPolicy::EVERY_SECOND);
        Server server(aof_writer, command_processor, storage, Server::kDefaultPort);
        server.run();
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }

    return 0;
}
