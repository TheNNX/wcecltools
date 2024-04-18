#pragma once

#include <string>
#include <Windows.h>
#include <set>

struct DefExport
{
    std::string exportAs;
    std::string name;
    std::ptrdiff_t ordinal = 0;

    auto operator<=>(const DefExport& other) const
    {
        return ordinal <=> other.ordinal;
    }

    operator std::string() const;
};

class ExportsDef
{
public:
    ExportsDef(const std::string& path);
    const std::set<DefExport>& GetExports() const;
    const std::string Regenerate(const std::string& libraryName) const;
    const std::string Regenerate() const
    {
        return Regenerate(library);
    }
private:
    std::string library;
    std::set<DefExport> exports;
    std::ptrdiff_t freeOrdinal = -1;
};