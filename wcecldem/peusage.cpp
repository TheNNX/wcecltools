#include "pefiles.hpp"

PeUsage::PeUsage()
    : mainImage(nullptr)
{

}

PeUsage::PeUsage(std::shared_ptr<PeFile> mainImage) :
    mainImage(mainImage)
{

}

PeUsage::operator std::shared_ptr<PeFile>() const
{
    return mainImage;
}

PeUsage& PeUsage::operator=(nullptr_t n)
{
    this->imageSet.clear();
    this->mainImage = nullptr;
    return *this;
}

bool PeUsage::operator==(const std::shared_ptr<PeFile>& ptr) const
{
    return ptr == mainImage;
}

bool PeUsage::operator!=(const std::shared_ptr<PeFile>& ptr) const
{
    return ptr != mainImage;
}

PeFile* PeUsage::operator->()
{
    return mainImage.get();
}

const PeFile* PeUsage::operator->() const
{
    return mainImage.get();
}

const FileView PeUsage::GetFiles() const
{
    return FileView(imageSet);
}