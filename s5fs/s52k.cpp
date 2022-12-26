#include "s52k.h"

#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <filesystem>

S52K::S52K(const char *filePath)
{
    m_file = ::open(filePath, O_RDONLY);
    if (m_file < 0)
        throw std::runtime_error{strerror(errno)};
    struct stat stat;
    if (fstat(m_file, &stat) != 0)
        throw std::runtime_error{strerror(errno)};
    m_fileSize = stat.st_size;
    m_map = mmap(nullptr, m_fileSize, PROT_READ, MAP_PRIVATE, m_file, 0);
    auto ptr = reinterpret_cast<const uint8_t *>(m_map);
    const uint8_t *end = ptr + m_fileSize - sizeof(filsys);

    while (ptr < end) {
        const auto *fs = (filsys *)ptr;
        if (fs->s_magic == FsMAGIC) {
            if (fs->s_type != 1 && fs->s_type != 2 && fs->s_type != 3) {
                ptr += 4;
                continue;
            }
            ptr -= 512;
            printf ("Found %s %s at 0x%lx\n", fs->s_fname, fs->s_fpack, (ptr - reinterpret_cast<const uint8_t *>(m_map)));
            m_start = ptr;
            break;
        } else {
            ++ptr;
        }
    }

    if (!m_start)
        throw std::runtime_error{"No fs found"};

    memset(&m_operations, 0, sizeof(m_operations));
    m_operations.getattr = getattr;
    m_operations.opendir= opendir;
    m_operations.readdir = readdir;
    m_operations.open = open;
    m_operations.read = read;
}

S52K::~S52K()
{
    if (m_map)
        munmap(m_map, m_fileSize);
    if (m_file > -1)
        ::close(m_file);
}

const fuse_operations *S52K::operations() const
{
    return &m_operations;
}

std::string S52K::trimmed(std::string str)
{
    static const char* ws = " \t\n\r\f\v";
    str.erase(0, str.find_first_not_of(ws));
    str.erase(str.find_last_not_of(ws) + 1);
    return str;
}

int S52K::getattr(const char *path, struct stat *st, fuse_file_info *fi)
{
    auto thiz = reinterpret_cast<const S52K *>(fuse_get_context()->private_data);
    const dinode *node = thiz->inode(fi);
    if (!node)
        node = thiz->findNode(path);
    if (!node)
        return -EACCES;
    fillStat(st, node);
    return 0;
}

int S52K::opendir(const char *path, fuse_file_info *fi)
{
    auto thiz = reinterpret_cast<const S52K *>(fuse_get_context()->private_data);
    const dinode *node = thiz->findNode(path);
    if (!node)
        return -EACCES;
    fi->fh = reinterpret_cast<uint64_t>(node);
    return 0;
}

int S52K::readdir(const char *path, void *buff, fuse_fill_dir_t filler, off_t offest, fuse_file_info *fi, fuse_readdir_flags flags)
{
    (void) flags;
    (void) offest;
    auto thiz = reinterpret_cast<const S52K *>(fuse_get_context()->private_data);
    const dinode *node = thiz->inode(fi);
    if (!node)
        node = thiz->findNode(path);
    if (!node)
        return -EACCES;
    long size = node->di_size;
    int nblk = (size + BSIZE -1 ) / BSIZE;
    int last = size % BSIZE;
    for(int n = 0; n < nblk; n++)
    {
        auto e = thiz->inodeBlock<direct>(node, n);
        int limit = BSIZE / sizeof(*e);
        if (n == nblk - 1)
            limit = last / sizeof(*e);
        for(int j = 0; j < limit; j++,e++) {
            const auto ino = e->d_ino;
            if(ino != 0 && e->d_name[0]) {
                struct stat stat;
                fillStat(&stat, thiz->inode(ino));
                filler(buff, trimmed(std::string{e->d_name}.substr(0, DIRSIZ)).c_str(),
                       &stat, 0, fuse_fill_dir_flags(0));
            }
        }
    }
    return 0;
}

int S52K::open(const char *path, fuse_file_info *fi)
{
    auto thiz = reinterpret_cast<const S52K *>(fuse_get_context()->private_data);
    if (fi->flags & (O_CREAT | O_WRONLY | O_RDWR  | O_TRUNC | O_APPEND))
        return -EACCES;
    const dinode *node = thiz->findNode(path);
    if (!node)
        return -EACCES;
    fi->fh = reinterpret_cast<uint64_t>(node);
    return 0;
}

int S52K::read(const char *path, char *buff, size_t size, off_t offset, fuse_file_info *fi)
{
    auto thiz = reinterpret_cast<const S52K *>(fuse_get_context()->private_data);
    const dinode *node = thiz->inode(fi);
    if (!node)
        node = thiz->findNode(path);
    if (!node)
        return -EACCES;

    auto sz = std::min<size_t>(size, node->di_size);
    int n = offset / BSIZE;
    offset %= BSIZE;
    while (sz) {
        auto data = thiz->inodeBlock(node, n++);
        const auto len = std::min<size_t>(BSIZE - offset, sz);
        memcpy(buff, data + offset, len);
        offset = 0;
        buff += len;
        sz -= len;
    }
    return size;
}

void S52K::fillStat(struct stat *st, const dinode *node)
{
    memset(st, 0, sizeof(struct stat));
    st->st_mode = node->di_mode;
    st->st_uid  = node->di_uid;
    st->st_gid  = node->di_gid;
    st->st_size = node->di_size;
    st->st_atim.tv_sec = node->di_atime;
    st->st_mtim.tv_sec = node->di_mtime;
    st->st_ctim.tv_sec = node->di_ctime;
}

const dinode *S52K::findNode(const char *path) const
{
    if (path[0] == '/')
        ++path;
    auto node = inode(S5ROOTINO);
    auto p = std::filesystem::path{path}.lexically_normal();
    for (const auto & e : p) {
        node = findNode(node, e.c_str());
        if (!node)
            return nullptr;
    }
    return node;
}

const dinode *S52K::findNode(const dinode *node, const char *path) const
{
    if (!S_ISDIR(node->di_mode))
        return nullptr;

    long size = node->di_size;
    int nblk = (size + BSIZE -1 ) / BSIZE;
    int last = size % BSIZE;
    for(int n = 0; n < nblk; n++)
    {
        auto e = inodeBlock<direct>(node, n);
        int limit = BSIZE / sizeof(*e);
        if (n == nblk - 1)
            limit = last / sizeof(*e);
        for(int j = 0; j < limit; j++,e++) {
            const auto ino = e->d_ino;
            if(ino != 0 && e->d_name[0]) {
                if (trimmed(std::string{e->d_name}.substr(0, DIRSIZ)) == path)
                    return inode(ino);
            }
        }
    }
    return nullptr;
}
