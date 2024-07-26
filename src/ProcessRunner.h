#ifndef C_EDIT_PROCESSRUNNER_H
#define C_EDIT_PROCESSRUNNER_H

#include <string>
#include <vector>
#include <functional>
#include <boost/process.hpp>

namespace bp = boost::process;

class ProcessRunner {
private:
    bp::child currentProcess;
    bp::async_pipe in;
    std::vector<char> line;
public:
    ProcessRunner();

    inline bool IsRunning() { return currentProcess.running(); }

    std::error_code Terminate();

    void Flush();

    void BuildAndRun(const std::string& filename,
                     const std::function<void(const std::string&)>& print,
                     const std::function<void(const std::string&)>& status);

    void Input(char ch);
};


#endif //C_EDIT_PROCESSRUNNER_H
