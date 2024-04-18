#include "pefiles.hpp"
#include <stdio.h>
#include <assert.h>
#include <iostream>
#include <ranges>

static std::string GetFilenameFromPath(const std::string& path)
{
    return path.substr(path.find_last_of("/\\") + 1);
}

PeFile::PeFile(const std::string& path) :
    peFile(fopen(path.c_str(), "rb")),
    exportAs(GetFilenameFromPath(path))
{

    if (std::fread(&dosHeader, sizeof(dosHeader), 1, peFile) != 1)
    {
        throw (-1);
    }

    if (std::fseek(peFile, dosHeader.e_lfanew, SEEK_SET) != 0)
    {
        throw (-1);
    }

    if (std::fread(&ntHeaders, sizeof(ntHeaders), 1, peFile) != 1)
    {
        throw (-1);
    }

    if (ntHeaders.OptionalHeader.Subsystem != IMAGE_SUBSYSTEM_WINDOWS_CE_GUI)
    {
        throw (-1);
    }
}

void PeFile::LoadSections()
{
    for (int i = 0; i < ntHeaders.FileHeader.NumberOfSections; i++)
    {
        if (std::fseek(
            peFile,
            dosHeader.e_lfanew +
            sizeof(ntHeaders) -
            sizeof(ntHeaders.OptionalHeader.DataDirectory[0]) *
            (IMAGE_NUMBEROF_DIRECTORY_ENTRIES -
             ntHeaders.OptionalHeader.NumberOfRvaAndSizes) +
            sizeof(IMAGE_SECTION_HEADER) * i,
            SEEK_SET) != 0)
        {
            throw (-1);
        }

        IMAGE_SECTION_HEADER sectionHeader;
        if (fread(&sectionHeader, sizeof(sectionHeader), 1, peFile) != 1)
        {
            throw (-1);
        }

        this->sections.insert(Section(sectionHeader, peFile));
    }
}

std::string PeFile::GetStringFromRva(DWORD rva)
{
    std::string str = "";
    char c;
    while ((c = Read(rva + str.length())) != 0)
    {
        str += c;
    }

    return str;
}

void PeFile::HandleImports(PeUsage& peUsage)
{
    IMAGE_DATA_DIRECTORY& dataDirectory =
        ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

    if (ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress == 0 ||
        IMAGE_DIRECTORY_ENTRY_IMPORT >= ntHeaders.OptionalHeader.NumberOfRvaAndSizes)
    {
        return;
    }

    int nDirectories = dataDirectory.Size / sizeof(IMAGE_IMPORT_DESCRIPTOR);

    assert(dataDirectory.Size % sizeof(IMAGE_IMPORT_DESCRIPTOR) == 0);
    auto importDirectories = new IMAGE_IMPORT_DESCRIPTOR[nDirectories];

    Read(importDirectories, dataDirectory.VirtualAddress, dataDirectory.Size);

    for (int i = 0; i < nDirectories - 1; i++)
    {
        std::string dllName = GetStringFromRva(importDirectories[i].Name);

        auto dll = Load(dllName, peUsage);

        auto dependency = std::make_shared<Dependency>();
        dependency->library = dll;
        dependency->libraryName = dllName;

        dependencies[dllName] = dependency;

        if (dll == nullptr)
        {
            continue;
        }

        DWORD thunk = importDirectories->FirstThunk;
        DWORD iltEntry;
        do
        {
            decltype(GetExport("")) exprt = nullptr;

            Read(iltEntry, thunk);
            if (iltEntry == 0)
            {
                break;
            }
            thunk += sizeof(iltEntry);

            std::shared_ptr<Import> imprt = std::make_shared<Import>();
            imprt->dependency = dependency;
            dependency->imports.insert(imprt);

            if ((iltEntry << 1) >> 1 != iltEntry)
            {
                iltEntry = (iltEntry << 1) >> 1;
                exprt = dll->GetExport(iltEntry);
                imprt->ordinal = iltEntry;
                auto findName = [&]()->std::string
                    {
                        for (const auto& [key, value] : dll->namedExports)
                        {
                            if (value == exprt)
                            {
                                return key;
                            }
                        }
                        return "";
                    };
                imprt->exportAs = findName();
            }
            else
            {
                std::string exportAs = GetStringFromRva(iltEntry);
                exprt = dll->GetExport(exportAs);
                imprt->exportAs = exportAs;
                if (exprt != nullptr)
                {
                    imprt->ordinal = exprt->ordinal;
                }
            }

            imprt->exprt = exprt;
        } while (iltEntry != 0);
    }

    delete[] importDirectories;
}

void PeFile::HandleExports(PeUsage& peUsage)
{
    IMAGE_DATA_DIRECTORY& dataDirectory =
        ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];

    if (ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress == 0 ||
        IMAGE_DIRECTORY_ENTRY_EXPORT >= ntHeaders.OptionalHeader.NumberOfRvaAndSizes)
    {
        return;
    }

    IMAGE_EXPORT_DIRECTORY exportDirectory;

    Read(&exportDirectory, dataDirectory.VirtualAddress, sizeof(IMAGE_EXPORT_DIRECTORY));

    for (DWORD i = 0; i < exportDirectory.NumberOfFunctions; i++)
    {
        auto& exprt = exports[i + exportDirectory.Base] = std::make_shared<Export>();
        exprt->library = shared_from_this();
        exprt->ordinal = i + exportDirectory.Base;
        Read(&exprt->value, exportDirectory.AddressOfFunctions + i * sizeof(exprt->value), sizeof(exprt->value));

        if (exprt->value >= dataDirectory.VirtualAddress &&
            exprt->value < dataDirectory.VirtualAddress + dataDirectory.Size)
        {
            std::string forwarderString = GetStringFromRva(exprt->value);
            auto result = forwarderString.find_last_of('.', 0);
            if (result == std::string::npos)
            {
                throw (-1);
            }
            std::string forwardedDllName = forwarderString.substr(0, result);
            std::string forwardedFunctionStr = forwarderString.substr(result + 1, std::string::npos);

            auto forwardedDll = Load(forwardedDllName, peUsage);

            if (forwardedFunctionStr.starts_with("#"))
            {
                DWORD forwardedOrdinal = std::stoi(forwardedFunctionStr.substr(1));
                exprt->forwarder = forwardedDll->GetExport(forwardedOrdinal);
            }
            else
            {
                exprt->forwarder = forwardedDll->GetExport(forwardedFunctionStr);
            }

            exprt->value = 0;
        }
    }

    for (DWORD i = 0; i < exportDirectory.NumberOfNames; i++)
    {
        uint16_t ordinal = 0;
        DWORD nameRva;
        std::string exportAs;

        Read(&ordinal, exportDirectory.AddressOfNameOrdinals + i * sizeof(ordinal), sizeof(ordinal));
        Read(&nameRva, exportDirectory.AddressOfNames + i * sizeof(nameRva), sizeof(nameRva));

        exportAs = GetStringFromRva(nameRva);

        auto exprt = exports.at(ordinal + exportDirectory.Base);
        namedExports[exportAs] = exprt;
        exprt->names.insert(exportAs);
    }
}

const std::string& PeFile::GetName() const
{
    return exportAs;
}

std::shared_ptr<const Export> PeFile::GetExport(std::string exportAs) const
{
    if (namedExports.contains(exportAs))
    {
        return namedExports.at(exportAs);
    }
    return nullptr;
}

std::shared_ptr<const Export> PeFile::GetExport(DWORD ordinal) const
{
    if (exports.contains(ordinal))
    {
        return exports.at(ordinal);
    }
    return nullptr;
}

std::shared_ptr<PeFile> PeFile::Load(std::string path, PeUsage& peUsage)
{
    for (char& c : path)
    {
        c = std::toupper(c);
    }

    try
    {
        std::shared_ptr<PeFile> result;
        if (LoadedPeFiles.count(GetFilenameFromPath(path)) > 0)
        {
            result = LoadedPeFiles.at(GetFilenameFromPath(path)).lock();
        }
        else
        {
            struct EnableMakeShared : public PeFile
            {
                EnableMakeShared(const std::string& path) : PeFile(path)
                {

                }
            };
            result = std::static_pointer_cast<PeFile>(std::make_shared<EnableMakeShared>(path));
            LoadedPeFiles[GetFilenameFromPath(path)] = result;
        }

        result->LoadSections();
        result->HandleExports(peUsage);
        result->HandleImports(peUsage);
        peUsage.imageSet.insert(result);

        return result;
    }
    catch (int i)
    {
        return nullptr;
    }
}

uint8_t PeFile::Read(DWORD address) const
{
    for (const auto& section : sections)
    {
        if (section.IsInRange(address))
        {
            return section.Read(address);
        }
    }
    return 0;
}

template<typename T>
void PeFile::Read(T* buffer, DWORD startAddress, DWORD size) const
{
    for (DWORD i = 0; i < size; i++)
    {
        ((uint8_t*)buffer)[i] = Read(startAddress + i);
    }
}

template<typename T, typename std::enable_if_t<std::is_const<T>::value == false>*>
void PeFile::Read(T& t, DWORD startAddress) const
{
    for (DWORD i = 0; i < sizeof(t); i++)
    {
        ((uint8_t*)&t)[i] = Read(startAddress + i);
    }
}

PeFile::~PeFile()
{
    if (peFile != NULL)
    {
        std::fclose(peFile);
    }
    LoadedPeFiles.erase(this->GetName());
}

PeUsage PeFile::Load(const std::string& str)
{
    PeUsage usage = PeUsage();
    auto image = Load(str, usage);
    usage.mainImage = image;
    return usage;
}

PeUsage IteratorOperatorDerefImpl(const FileView::const_iterator& f)
{
    return PeUsage(*f.GetSetIterator());
}

FileView::const_iterator::const_iterator(const decltype(setIterator)& iterator) :
    setIterator(iterator)
{

}
const decltype(FileView::const_iterator::setIterator)& FileView::const_iterator::GetSetIterator() const
{
    return setIterator;
}

FileView::const_iterator& FileView::const_iterator::operator++()
{
    setIterator++;
    return *this;
}

FileView::const_iterator FileView::const_iterator::operator++(int)
{
    const_iterator tmp = *this;
    ++(*this);
    return tmp;
}

bool FileView::const_iterator::operator==(const const_iterator& b) const
{
    return setIterator == b.setIterator;
}

bool FileView::const_iterator::operator!=(const const_iterator& b) const
{
    return setIterator != b.setIterator;
}

FileView::const_iterator FileView::begin() const
{
    return const_iterator(imageSet.cbegin());
}

FileView::const_iterator FileView::end() const
{
    return const_iterator(imageSet.cend());
}

FileView::FileView(const std::set<std::shared_ptr<PeFile>>& imageSet) :
    imageSet(imageSet)
{
    if constexpr (std::ranges::range<FileView>)
    {
        std::cout << "r is a range\n";
    }
}

Section::Section(std::string exportAs, DWORD virtualAddress, DWORD sizeInFile, DWORD sizeVirtual) :
    exportAs(exportAs),
    virtualAddress(virtualAddress),
    sizeInFile(sizeInFile),
    sizeVirtual(sizeVirtual),
    data(new uint8_t[sizeInFile])
{

}

Section::Section(IMAGE_SECTION_HEADER sectionHeader, FILE* peFile) :
    Section(
        std::string(
            sectionHeader.Name,
            sectionHeader.Name + sizeof(sectionHeader.Name) - 1),
        sectionHeader.VirtualAddress,
        sectionHeader.SizeOfRawData,
        sectionHeader.Misc.VirtualSize)
{
    if (std::fseek(peFile, sectionHeader.PointerToRawData, SEEK_SET) != 0)
    {
        throw (-1);
    }

    if (std::fread(data, sizeInFile, 1, peFile) != 1)
    {
        throw (-1);
    }
}

bool Section::IsInRange(DWORD Address) const
{
    return !(Address < virtualAddress || Address >= virtualAddress + sizeVirtual);
}

uint8_t Section::Read(DWORD Address) const
{
    if (IsInRange(Address) == false)
    {
        throw (-1);
    }

    DWORD relativeAddress = Address - virtualAddress;
    if (relativeAddress >= sizeInFile)
    {
        return 0;
    }
    return data[relativeAddress];
}

auto Section::operator<=>(const Section& other) const
{
    return (this->virtualAddress <=> other.virtualAddress);
}

Section::Section(Section&& other) noexcept
{
    this->data = std::move(other.data);
    this->exportAs = std::move(other.exportAs);
    this->sizeInFile = std::move(other.sizeInFile);
    this->virtualAddress = std::move(other.virtualAddress);
    this->sizeVirtual = std::move(other.sizeVirtual);

    other.data = NULL;
}

Section::~Section()
{
    if (data != NULL)
    {
        delete[] data;
    }
}

Export::operator std::string() const
{
    if (names.size() > 0)
    {
        return *names.begin();
    }

    return std::string("@") + std::to_string(ordinal);
}