#pragma once

#include <Windows.h>
#include <map>
#include <string>
#include <memory>
#include <set>

class PeFile;
struct Dependency;
struct Import;
struct Export;

struct Import
{
    std::weak_ptr<Dependency> dependency;
    DWORD ordinal;
    std::string name;
    std::weak_ptr<const Export> exprt;
};

struct Dependency
{
    std::weak_ptr<PeFile> library;
    std::string libraryName;
    std::set<std::shared_ptr<Import>> imports;
};

struct Export
{
    std::set<std::string> names;
    std::weak_ptr<PeFile> library;
    DWORD ordinal = 0;
    DWORD value = 0;
    std::weak_ptr<const Export> forwarder;

    operator std::string() const;
};

struct Section
{
    Section(std::string name, DWORD virtualAddress, DWORD sizeInFile, DWORD sizeVirtual);
    Section(IMAGE_SECTION_HEADER sectionHeader, FILE* peFile);
    bool IsInRange(DWORD Address) const;
    uint8_t Read(DWORD Address) const;
    auto operator<=>(const Section& other) const;
    Section(Section&& other) noexcept;
    ~Section();

private:
    DWORD virtualAddress;
    DWORD sizeInFile;
    DWORD sizeVirtual;
    uint8_t* data;
    std::string name;
};

class FileView
{
private:
    friend class PeUsage;
    std::set<std::shared_ptr<PeFile>> imageSet;

    FileView(const std::set<std::shared_ptr<PeFile>>& imageSet);
public:
    class const_iterator
    {
    private:
        friend class FileView;
        std::set<std::shared_ptr<PeFile>>::const_iterator setIterator;

        const_iterator(const decltype(setIterator)& iterator);
    public:
        const decltype(setIterator)& GetSetIterator() const;

        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = const PeUsage;
        using pointer = value_type*;
        using reference = value_type&;

        auto operator*() const;

        const_iterator& operator++();
        const_iterator operator++(int);

        bool operator==(const const_iterator& b) const;
        bool operator!=(const const_iterator& b) const;
    };

    const_iterator begin() const;
    const_iterator end() const;
};

class PeUsage
{
    std::set<std::shared_ptr<PeFile>> imageSet;
    std::shared_ptr<PeFile> mainImage;

    PeUsage();
public:
    PeUsage(std::shared_ptr<PeFile> mainImage);

    operator std::shared_ptr<PeFile>() const;

    PeUsage& operator=(nullptr_t n);

    bool operator==(const std::shared_ptr<PeFile>& ptr) const;
    bool operator!=(const std::shared_ptr<PeFile>& ptr) const;

    PeFile* operator->();
    const PeFile* operator->() const;

    const FileView GetFiles() const;

    friend class PeFile;
};

class PeFile : public std::enable_shared_from_this<PeFile>
{
private:
    static std::map<std::string, std::weak_ptr<PeFile>> LoadedPeFiles;

    FILE* peFile = NULL;
    std::string name;
    IMAGE_DOS_HEADER dosHeader;
    IMAGE_NT_HEADERS32 ntHeaders;
    std::set<Section> sections;

    std::map<std::string, std::shared_ptr<Dependency>> dependencies;
    std::map<DWORD, std::shared_ptr<Export>> exports;
    std::map<std::string, std::shared_ptr<Export>> namedExports;

    PeFile(const std::string& path);
    void LoadSections();
    std::string GetStringFromRva(DWORD rva);

    void HandleImports(PeUsage& peUsage);
    void HandleExports(PeUsage& peUsage);
public:
    const std::string& GetName() const;
    std::shared_ptr<const Export> GetExport(std::string name) const;
    std::shared_ptr<const Export> GetExport(DWORD ordinal) const;
    static std::shared_ptr<PeFile> Load(std::string path, PeUsage& peUsage);
    uint8_t Read(DWORD address) const;
    template<typename T>
    void Read(T* buffer, DWORD startAddress, DWORD size) const;
    template<typename T, typename std::enable_if_t<std::is_const<T>::value == false>* = nullptr>
    void Read(T& t, DWORD startAddress) const;
    ~PeFile();
    static PeUsage Load(const std::string& str);
};

inline auto FileView::const_iterator::operator*() const
{
    PeUsage IteratorOperatorDerefImpl(const FileView::const_iterator & f);
    return IteratorOperatorDerefImpl(*this);
}