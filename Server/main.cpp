#include "Interpreter/Runtime/Executor.h"
#include "Interpreter/Runtime/StorageEngine.h"
#include "Interpreter/Parsing/Parser.h"
#include "Interpreter/Parsing/Tokenizer.h"
#include "Interpreter/Tokens/Token.h"
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>


int main() {
    StorageEngine storage;
    Executor executor(storage);
    

    std::cout << "redisserver ready" << std::endl;
    std::cout << "Enter commands or type EXIT to quit." << std::endl;

    std::string line;
    while (std::getline(std::cin, line)) {
        std::cout << "< ";
        if (line.empty()) {
            continue;
        }

        if (line == "EXIT" || line == "exit" || line == "QUIT" || line == "quit") {
            break;
        }

        std::optional<std::vector<Token>> tokens = Tokenizer::tokenize(line);
        if(!tokens.has_value()) {
            std::cout << "Invalid expression..." << std::endl;
            continue;
        }

        std::vector<Token> parsed_tokens = tokens.value();
        std::unique_ptr<Statement> statement = Parser::parse(parsed_tokens);
        if(statement == nullptr) {
            std::cout << "Parsing error..." << std::endl;
            continue;
        }
        if (!executor.execute(*statement)) {
            std::cout << "Execution error..." << std::endl;
        }
        
    }

    return 0;
}
