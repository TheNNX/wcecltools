#include "def.h"
#include <fstream>
#include <vector>
#include <sstream>
#include <regex>
#include <assert.h>
#include <iostream>

/* This is effectively a mockup of a real parser - big WIP */
ExportsDef::ExportsDef(const std::string& path)
{
    std::ifstream file(path);
    library = path;

    if (!file)
    {
        throw -1;
    }

    /* Scary regex :) */
    std::regex exportCommentRegex(R"(^[ \t]*(?:(\w+)\s*(?:(?:=\s*)(\w+))?)\s*(?:@(\d+))?\s*(?:;.*)?$)");
    std::regex exportRegex(R"(^[ \t]*(?:(\w+)\s*(?:(?:=\s*)(\w+))?)\s*(?:@(\d+))?\s*$)");
    const int functionNameGroup = 1;
    const int exportNameGroup = 2;
    const int ordinalGroup = 3;

    while (file.eof() == false)
    {
        std::string line;
        std::getline(file, line);

        auto toUpper = [](std::string result)
        {
            for (char& c : result)
            {
                c = std::toupper(c);
            }

            return result;
        };

        auto splitter = [](const std::string& line)
        {
            std::vector<std::string> split;
            std::string w;
            std::stringstream stream(line);
            
            char c;
            while (stream >> c)
            {
                if (std::isspace(c))
                {
                    split.push_back(w);
                    w = "";
                }
                else
                {
                    w += c;
                }
            }
            if (w != "")
            {
                split.push_back(w);
            }
            return split;
        };

        auto split = splitter(line);
        auto directive = split.size() == 0 ? "" : toUpper(split[0]);

        if (directive == "LIBRARY")
        {
            /* TODO */
        }
        else if (directive == "EXPORTS")
        {
            /* TODO */
            /* support export on the same line */
        }
        /* TODO: some other directives to match */
        else
        {
            /* WIP */
            DefExport exprt;

            /* TODO: DATA directives, forwarder strings and probably some other stuff */

            std::smatch matches;
            
            if (std::regex_search(line, matches, exportRegex) || 
                std::regex_search(line, matches, exportCommentRegex))
            {
                /* There are string/line anchors in place. 
                   Multiple lines matching would be a bug. */
                assert(matches.size() == 4);
                auto match = matches[0];

                std::string functionName = matches[functionNameGroup];
                std::string exportName = matches[exportNameGroup];
                std::string ordinalName = matches[ordinalGroup];

                if (ordinalName != "")
                {
                    exprt.ordinal = std::stoi(ordinalName);
                }
                if (exportName != "")
                {
                    exprt.exportAs = exportName;
                }
                exprt.name = functionName;

                if (exprt.ordinal == 0)
                {
                    exprt.ordinal = freeOrdinal--;
                }
               
                if (exports.count(exprt) && exports.find(exprt)->exportAs != "")
                {
                    std::cout << "Duplicate " << exprt.name << "=" << exprt.exportAs << " @" << exprt.ordinal << "\n";
                    continue;
                }
                exports.insert(exprt);
            }
            else
            {
                std::cout << line << " does not match.\n";
            }
        }
    }
}

const std::set<DefExport>& ExportsDef::GetExports() const
{
    return exports;
}

DefExport::operator std::string() const
{
    std::string result = this->name;
    if (exportAs != "")
    {
        result += " = " + exportAs;
    }
    if (ordinal > 0)
    {
        result += std::string(" @") + std::to_string(ordinal);
    }
    return result;
}

const std::string ExportsDef::Regenerate(const std::string& libraryName) const
{
    std::set<
        std::reference_wrapper<const DefExport>, 
        decltype([](const DefExport& a, const DefExport& b) 
                 {
                     return a < b; 
                 })> unasignedExports;

    std::set<std::ptrdiff_t> usedOrdinals;

    std::string result = "LIBRARY " + libraryName + "\nEXPORTS\n";

    for (auto& defExport : exports)
    {
        if (defExport.ordinal < 0)
        {
            unasignedExports.insert(defExport);
            continue;
        }
        
        usedOrdinals.insert(defExport.ordinal);
        result += "    " + (std::string)defExport + "\n";
    }

    std::ptrdiff_t tryOrdinal = 1;
    for (const DefExport& defExport : unasignedExports)
    {
        result += "   " + (std::string)defExport + "\n";
    }

    return result;
}