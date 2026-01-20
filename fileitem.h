#ifndef FILEITEM_H
#define FILEITEM_H

#include <QString>
#include <QDateTime>
#include <QList>
#include <memory>

class FileItem
{
public:
    FileItem(const QString &name, const QString &path, qint64 size,
             const QDateTime &modified, bool isDir);
    ~FileItem();

    void addChild(std::shared_ptr<FileItem> child);
    qint64 totalSize() const;

    QString name() const { return m_name; }
    QString path() const { return m_path; }
    qint64 size() const { return m_size; }
    QDateTime modified() const { return m_modified; }
    bool isDirectory() const { return m_isDirectory; }
    const QList<std::shared_ptr<FileItem>>& children() const { return m_children; }

private:
    QString m_name;
    QString m_path;
    qint64 m_size;
    QDateTime m_modified;
    bool m_isDirectory;
    QList<std::shared_ptr<FileItem>> m_children;
    mutable qint64 m_cachedTotalSize;
    mutable bool m_totalSizeCached;
};

#endif // FILEITEM_H
