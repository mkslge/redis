#ifndef LOGCOMPACTOR_H
#define LOGCOMPACTOR_H

#include "Persistence/LogConfig.h"
#include "Parsing/Tokenizer.h"
#include "Parsing/Parser.h"
#include <unordered_map>
#include <string>
#include <fstream>

using Line = int;

namespace codes {
    std::string DELETED_LINE = "";
}

class LogCompactor {
    std::vector<std::string> lines_;
    std::unordered_map<std::string,Line> setLog_;
    std::unordered_map<std::string,Line> expireLog_;
    std::unordered_map<std::string,Line> deleteLog_;
    std::string file_path_;

    
    Tokenizer tokenizer_;
    Parser parser_;
    public:

    LogCompactor(std::string_view file_path, Tokenizer& tokenizer, Parser& parser) 
    : file_path_(file_path), tokenizer_(tokenizer), parser_(parser) {
        

        
    }

    void compact() {
        std::string command{};

        int line_num{};
        std::fstream stream;
        stream.open(file_path_, std::ios::in);
        if(!stream.is_open()) {
            throw "Could not open AOFLog.";
        }


        while(std::getline(stream, command)) {
            lines_.push_back(command);
            //parse out command line by line
            auto tokens = tokenizer_.tokenize(command);
            if(tokens == std::nullopt) {
                continue;
            }
            //parse out tokenized output
            std::unique_ptr<Statement> statement = parser_.parse(tokens.value());

            //if the statement is mutating we check if theres anything with the same key
            if(statement.get()->mutates()) {
                std::string key = statement.get()->get_key().value();
                auto type = statement.get()->get_type();
                if(type == StatementType::SET) {
                    //if its set we check if the key exists in our map,
                    //if it does, we cut it off
                    if(setLog_.contains(key)) {
                        deleteLine(setLog_[key]);
                    }
                    //replace the set with current line
                    setLog_[key] = line_num;
                    
                    
                } else if(type == StatementType::EXPIRE) {
                    //if its set we check if the key exists in our map,
                    //if it does, we cut it off
                    if(expireLog_.contains(key)) {
                        deleteLine(expireLog_[key]);
                        expireLog_.erase(key);
                    }
                    //again, replace the set with current line
                    expireLog_[key] = line_num;
                } else if(type == StatementType::DELETE) {
                    if(expireLog_.contains(key)) {
                        deleteLine(expireLog_[key]);
                        expireLog_.erase(key);
                    }
                    if(deleteLog_.contains(key)) {
                        deleteLine(deleteLog_[key]);
                        deleteLog_.erase(key);
                    }

                    if(setLog_.contains(key)) {
                        deleteLine(setLog_[key]);
                        setLog_.erase(key);
                    }
                    deleteLog_[key] = line_num;
                }
            }

            line_num++;
        }

        writeBack(lines_);
    }

    void deleteLine(int line_num) {
        lines_[line_num] = codes::DELETED_LINE;
    }

    void writeBack(std::vector<std::string>& lines) {
        std::fstream stream;

        stream.open(file_path_, std::ios::out);
        if(!stream.is_open()) {
            throw "LogCompactor failed to open file.";
        }
        
        for(const auto& line : lines) {
            if(line != codes::DELETED_LINE) {
                stream << line << "\n";
            }
        }

        stream.close();
    }
};


#endif