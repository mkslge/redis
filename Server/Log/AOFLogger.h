#ifndef AOFLOGGER_H
#define AOFLOGGER_H

#include <fstream>
#include <string>

class AOFLogger {
public:
    explicit AOFLogger(const std::string& file_path = "Log/appendonlylog.txt");
    void enqueue(const std::string& log_entry);

private:
    std::ofstream stream_;
};

#endif //AOFLOGGER_H
