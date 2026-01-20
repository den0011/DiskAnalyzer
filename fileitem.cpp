#include "fileitem.h"

FileItem::FileItem(const QString &name, const QString &path, qint64 size,
                   const QDateTime &modified, bool isDir)
    : m_name(name)
    , m_path(path)
    , m_size(size)
    , m_modified(modified)
    , m_isDirectory(isDir)
    , m_cachedTotalSize(0)
    , m_totalSizeCached(false)
{
}

FileItem::~FileItem()
{
    m_children.clear();
}

void FileItem::addChild(std::shared_ptr<FileItem> child)
{
    m_children.append(child);
    m_totalSizeCached = false;
}

qint64 FileItem::totalSize() const
{
    if (m_totalSizeCached)
        return m_cachedTotalSize;

    qint64 total = m_size;
    for (const auto &child : m_children) {
        total += child->totalSize();
    }

    const_cast<FileItem*>(this)->m_cachedTotalSize = total;
    const_cast<FileItem*>(this)->m_totalSizeCached = true;

    return total;
}
