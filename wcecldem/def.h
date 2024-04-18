#pragma once

#include <string>
#include <Windows.h>
#include <vector>

struct DefExport
{
    std::string exportFrom;
    std::string exportAs;
    std::ptrdiff_t ordinal = 0;

    /* When regenerating the .def file, this export will be placed together with
       other exports with the same group, which will be signified by the group
       name in a comment above them. */
    std::string group = "";

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
    const std::vector<DefExport>& GetExports() const;
    std::vector<DefExport>& GetExports();
    const std::string Regenerate(const std::string& libraryName) const;
    const std::string Regenerate() const
    {
        return Regenerate(library);
    }
private:
    std::string library = "COREDLL"; // FIXME
    std::vector<DefExport> exports;
    std::ptrdiff_t freeOrdinal = -1;
};