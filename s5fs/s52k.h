#pragma once

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <sys/stat.h>
#include <string>

#define FUSE_USE_VERSION 30
#include <fuse.h>

constexpr int32_t NICINOD = 100;             /* number of superblock inodes */
constexpr int32_t NICFREE = 50;              /* number of superblock free blocks */

#pragma pack(push, 2)
struct filsys
{
    uint16_t    s_isize;        /* size in blocks of i-list */
    int32_t     s_fsize;        /* size in blocks of entire volume */
    int16_t     s_nfree;        /* number of addresses in s_free */
    int32_t     s_free[NICFREE];/* free block list */
    int16_t     s_ninode;       /* number of i-nodes in s_inode */
    uint16_t    s_inode[NICINOD];/* free i-node list */
    char        s_flock;        /* lock during free list manipulation */
    char        s_ilock;        /* lock during i-list manipulation */
    char        s_fmod;         /* super block modified flag */
    char        s_ronly;        /* mounted read-only flag */
    int32_t     s_time;         /* last super block update */
    int16_t     s_dinfo[4];     /* device information */
    int32_t     s_tfree;        /* total free blocks*/
    uint16_t    s_tinode;       /* total free inodes */
    char        s_fname[6];     /* file system name */
    char        s_fpack[6];     /* file system pack name */
    int32_t     s_fill[14];     /* adjust to make sizeof struct filsys == 512 */
    int32_t     s_state;        /* file system state */
    int32_t     s_magic;        /* magic number to indicate new file system */
    int32_t     s_type;         /* type of new file system */
};

constexpr int32_t DIRSIZ = 14;		/* max characters per directory */
constexpr int32_t S5ROOTINO = 2;	/* i number of all roots */

constexpr int32_t BSIZE = 2048;		/* size of secondary block (bytes) */
constexpr int32_t NADDR = 13;

struct dinode {
    uint16_t	di_mode;	    /* mode and type of file */
    int16_t     di_nlink;    	/* number of links to file */
    uint16_t	di_uid;      	/* owner's user id */
    uint16_t	di_gid;      	/* owner's group id */
    int32_t     di_size;     	/* number of bytes in file */
    uint8_t  	di_addr[3 * NADDR];	/* disk block addresses */
    uint8_t     di_gen;		    /* file generation number */
    uint32_t    di_atime;   	/* time last accessed */
    uint32_t    di_mtime;   	/* time last modified */
    uint32_t    di_ctime;   	/* time created */
};

struct direct
{
    uint16_t d_ino;
    char     d_name[DIRSIZ];
};

#pragma pack(pop)

constexpr int32_t FsMAGIC = 0xfd187e20;      /* s_magic */

#define FsOKAY          0x7c269d38      /* s_state: clean */
#define FsACTIVE        0x5e72d81a      /* s_state: active */
#define FsBAD           0xcb096f43      /* s_state: bad root */
#define FsBADBLK        0xbadbc14b      /* s_state: bad block corrupted it */


class S52K {
public:
    S52K(const char *filePath);
    ~S52K();
    const fuse_operations *operations() const;
private:
    static std::string trimmed(std::string str);
    static int getattr(const char *path, struct stat *st, fuse_file_info *fi);
    static int opendir(const char *path, struct fuse_file_info *fi);
    static int readdir(const char *path, void *buff, fuse_fill_dir_t filler, off_t offest,
                       struct fuse_file_info *fi, enum fuse_readdir_flags flags);
    static int open(const char *path, struct fuse_file_info * fi);
    static int read(const char *path, char *buff, size_t size, off_t offset, struct fuse_file_info *fi);

    static void fillStat(struct stat *st, const dinode * node);
    const dinode *findNode(const char *path) const;
    const dinode *findNode(const dinode *node, const char *path) const;

    static inline const dinode *inode(const struct fuse_file_info *fi)
    {
        return fi ? reinterpret_cast<const dinode *>(fi->fh) : nullptr;
    }

    inline const dinode *inode(uint16_t n) const
    {
        return reinterpret_cast<const dinode *>(m_start + BSIZE * 2 + sizeof(dinode) * (n - 1));
    }
    static inline uint32_t read3bytes(const uint8_t *a, uint8_t n)
    {
        return ((((a[n * 3 + 2] << 8) + a[n * 3 + 1]) << 8) + a[n * 3]);
    }
    static inline uint32_t read4bytes(const uint8_t *a,int n)
    {
        return ((((((a[n * 4 + 3] << 8) + a[n * 4 + 2]) << 8) + a[n * 4 + 1]) << 8) + a[n * 4]);
    }

    inline const uint8_t *absBlock(uint32_t n) const
    {
        return m_start + n * BSIZE;
    }

    template <typename T = uint8_t>
    const T *inodeBlock(const dinode *node, int n) const
    {
        if(n < 10)
            return reinterpret_cast<const T*>(absBlock(read3bytes(node->di_addr, n)));
        if(n < 10 + BSIZE / 4)
            return reinterpret_cast<const T*>(absBlock(read4bytes(
                absBlock(read3bytes(node->di_addr, 10)), n - 10)));

        if(n < 10 + BSIZE / 4 + BSIZE / 4 * BSIZE / 4)
            return reinterpret_cast<const T*>(absBlock(read4bytes(
                absBlock(read4bytes(
                    absBlock(read3bytes(node->di_addr, 11)),
                    (n - 10 - BSIZE / 4) / (BSIZE / 4))),
                (n - 10 - BSIZE / 4) % (BSIZE / 4))));

        throw std::runtime_error{"invalid node block"};
    }

private:
    fuse_operations m_operations;
    int m_file = -1;
    size_t m_fileSize;
    const uint8_t *m_start = nullptr;
    void *m_map = nullptr;
};


