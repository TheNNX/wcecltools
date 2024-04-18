#include "def.h"
#include <fstream>
#include <vector>
#include <sstream>
#include <regex>
#include <assert.h>
#include <iostream>
#include <map>
#include <set>

/* This is effectively a mockup of a real parser - big WIP */
ExportsDef::ExportsDef(const std::string& path)
{
    std::ifstream file(path);

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
                    exprt.exportFrom = exportName;
                }
                exprt.exportAs = functionName;

                if (exprt.ordinal == 0)
                {
                    exprt.ordinal = freeOrdinal--;
                }
               
                auto it = std::find_if(
                    exports.begin(), 
                    exports.end(), 
                    [&](const DefExport& e2)
                    {
                        return exprt.ordinal == e2.ordinal;
                    });

                if (it != exports.end() && it->exportFrom != "")
                {
                    std::cout << "Duplicate " << exprt.exportAs << "=" << exprt.exportFrom << " @" << exprt.ordinal << "\n";
                    continue;
                }
                exports.push_back(exprt);
            }
            else
            {
                std::cout << line << " does not match.\n";
            }
        }
    }
}

std::vector<DefExport>& ExportsDef::GetExports()
{
    return exports;
}

const std::vector<DefExport>& ExportsDef::GetExports() const
{
    return exports;
}

DefExport::operator std::string() const
{
    std::string result = this->exportAs;
    if (exportFrom != "")
    {
        result += " = " + exportFrom;
    }
    if (ordinal > 0)
    {
        result += std::string(" @") + std::to_string(ordinal);
    }
    return result;
}

const std::string ExportsDef::Regenerate(const std::string& libraryName) const
{
    struct Group
    {
        using ExportSet = std::set<
            std::reference_wrapper<const DefExport>,
            decltype([](const DefExport& a, const DefExport& b)
                     {
                         return a < b;
                     })>;

        ExportSet exports;

        void Regenerate(std::string& result) const
        {
            for (const DefExport& e : exports)
            {
                result += "    " + (std::string)(e) + "\n";
            }
        }
    };

    std::map<std::string, Group> groups;
    std::set<std::ptrdiff_t> usedOrdinals;


    std::string result = "LIBRARY " + libraryName + "\nEXPORTS\n";

    for (auto& defExport : exports)
    {
        groups[defExport.group].exports.insert(std::ref(defExport));
    }

    for (auto& [groupName, group] : groups)
    {
        /* Do the unsorted group last. */
        if (groupName == "")
        {
            continue;
        }

        result += std::string("\n    ; ") + groupName + "\n";
        group.Regenerate(result);
    }
    result += "\n    ; Not found in the COREDLL code\n";
    groups[""].Regenerate(result);

    return result;
}