#include "Interpreter/Runtime/CommandProcessor.h"
#include "Interpreter/Runtime/Executor.h"
#include "Interpreter/Runtime/StorageEngine.h"

#include <iostream>
#include <string>

int main() {
    StorageEngine storage;
    Executor executor(storage);
    CommandProcessor processor(executor);

    std::cout << "redisserver ready" << std::endl;
    std::cout << "Enter commands or type EXIT to quit." << std::endl;

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }

        if (line == "EXIT" || line == "exit" || line == "QUIT" || line == "quit") {
            break;
        }

        processor.process_command(line);
    }

    return 0;
}
