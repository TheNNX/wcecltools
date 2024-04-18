#include "pefiles.hpp"
#include "def.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <regex>

std::map<std::string, std::weak_ptr<PeFile>> PeFile::LoadedPeFiles;

void TryMatchExports(const std::string& coreSrc, std::map<std::string, DefExport*>& mapping)
{
    std::regex word("\\w+");
    for (auto& dirEntry : std::filesystem::recursive_directory_iterator(coreSrc))
    {
        /* TODO: parse the file instead of doing this (because this can result 
           in false positives). */
        if (dirEntry.path().string().ends_with(".cpp"))
        {
            std::ifstream stream(dirEntry.path());

            std::string contents = std::string(
                std::istreambuf_iterator<char>(stream),
                std::istreambuf_iterator<char>());

            std::smatch matches;

            std::cout << "Processing " << dirEntry.path().string() << "\n";
            std::string::const_iterator searchStart(contents.cbegin());
            while (std::regex_search(searchStart, contents.cend(), matches, word))
            {
                for (auto& match : matches)
                {
                    auto find = mapping.find(match.str());
                    if (find == mapping.end())
                    {
                        continue;
                    }

                    std::string filename = dirEntry.path().filename().string();
                    if (find->second->group != "")
                    {
                        if (find->second->group != filename)
                        std::cout 
                            << "Warning " 
                            << find->first 
                            << " already mapped to file " 
                            << find->second->group
                            << ", ignoring " 
                            << filename
                            << "\n";
                        continue;
                    }

                    find->second->group = filename;
                }
                searchStart = matches.suffix().first;
            }
        }
    }
}

int main(int argc, const char* argv[])
{
    if (argc > 4 || argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <coredllSrcPath> <path[exports.def]> <exportpath[exports.def]>\n";
        return -1;
    }

    std::string exportsIn = "exports.def";
    std::string exportsOut = "exports.def";
    if (argc > 2)
    {
        exportsIn = argv[2];
    }
    if (argc > 3)
    {
        exportsOut = argv[3];
    }

    std::string coreSrc(argv[1]);

    auto defParse = ExportsDef(exportsIn);
    std::vector<DefExport>& exports = defParse.GetExports();

    std::map<std::string, DefExport*> exportDefToSrcFile;

    for (auto& e : exports)
    {
        exportDefToSrcFile[e.exportFrom] = &e;
    }

    TryMatchExports(coreSrc, exportDefToSrcFile);

    std::string result = defParse.Regenerate();

    std::ofstream output(exportsOut);

    if (!output)
    {
        std::cerr << "Couldn't open output file.\n";
        return -1;
    }

    output << result;

    return 0;
}