
#include "Persistence/LogConfig.h"
#include "Parsing/Tokenizer.h"
#include "Parsing/Parser.h"
#include <unordered_map>
#include <string>
#include <fstream>

using Line = int;

class LogCompactor {
    std::unordered_map<std::string,Line> pastLogs_;
    std::fstream stream_;
    Tokenizer tokenizer_;
    Parser parser_;
    public:

    LogCompactor(std::string_view file_path, Tokenizer& tokenizer, Parser& parser) 
    : tokenizer_(tokenizer), parser_(parser) {
        stream_.open(file_path, std::ios::in | std::ios::out);
        if(!stream_.is_open()) {
            throw "LogCompactor failed to open file.";
        }

        
    }

    void compact() {
        std::string command{};
        while(std::getline(stream_, command)) {
            //parse out command line by line
            auto tokens = tokenizer_.tokenize(command);
            if(tokens == std::nullopt) {
                continue;
            }
            //parse out tokenized output
            std::unique_ptr<Statement> statement = parser_.parse(tokens.value());

            //if the statement is mutating we check if theres anything with the same key
            if(statement.get()->mutates()) {
                switch(statement.get()->get_type()) {
                    case StatementType::GET:
                    
                    break;
                    case StatementType::DELETE:
                    break;
                    case StatementType::EXPIRE:
                    break;
                }

                //if its already in our map, we remove the older one, and replace it
                //otherwise we simpy add the key to our map with the line
            }


        }
    }
};