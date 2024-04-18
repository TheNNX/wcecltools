#include "pefiles.hpp"
#include "def.h"
#include <fstream>
#include <iostream>

std::map<std::string, std::weak_ptr<PeFile>> PeFile::LoadedPeFiles;

/* TODO: reference counting done right */
int main(int argc, const char* argv[])
{
    if (argc > 3)
    {
        std::cerr << "Usage: " << argv[0] << " <path[exports.def]> <exportpath[exports.def]>\n";
        return -1;
    }

    std::string param1 = "exports.def";
    std::string param2 = "exports.def";
    if (argc > 1)
    {
        param1 = argv[1];
    }
    if (argc > 2)
    {
        param2 = argv[2];
    }

    auto defParse = ExportsDef(param1);
    auto a = defParse.GetExports();

    std::string result = defParse.Regenerate();
    std::cout << result;

    std::ofstream output(param2);

    if (!output)
    {
        std::cerr << "Couldn't open output file.\n";
        return -1;
    }

    output << result;

    return 0;
}