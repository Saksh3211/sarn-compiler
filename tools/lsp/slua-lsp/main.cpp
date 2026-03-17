#include <iostream>
#include <string>
#include <cstdio>

std::string runCompiler(const std::string& file)
{
    std::string command = "build/compiler/sluac.exe " + file + " -o temp.ll 2>&1";

    char buffer[256];
    std::string result;

    FILE* pipe = _popen(command.c_str(), "r");

    if (!pipe)
        return "Failed to run compiler.";

    while (fgets(buffer, sizeof(buffer), pipe))
        result += buffer;

    _pclose(pipe);

    return result;
}

int main()
{
    std::string line;

    while (std::getline(std::cin, line))
    {
        if (line.find("initialize") != std::string::npos)
        {
            std::cout <<R"({"id":1,
"result":{"capabilities":{
"completionProvider":{},"hoverProvider":true,
"textDocumentSync":1
}}
})" << std::endl;
        }

        if (line.find("didSave") != std::string::npos)
        {
            std::string output = runCompiler("current.slua");
            std::cout << output << std::endl;
        }
    }
}
