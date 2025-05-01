
# ExFS2: Extensible File System 2

ExFS2 is a user-space file system implemented in C that simulates a basic block-based file system using flat segment files.

## ✅ Features Implemented

- [x] Add file (`-a`)
- [x] Extract file (`-e`)
- [x] Remove file (`-r`)
- [x] List all files (`-l`)
- [x] Debug file/directory (`-D`)
- [x] Nested directories and path resolution
- [x] Direct, single indirect, and double indirect block handling
- [ ] Triple indirect blocks (**not implemented** - not required per project spec)

## 🗂 Segment Design

- Inode Segment: `inode_segment_*.seg` (256 inodes per segment)
- Data Segment: `data_segment_*.seg` (256 blocks per segment)
- Block size: 4KB, Segment size: 1MB

## ⚙️ Build Instructions

```bash
make clean    # Clean up all build files and segments
make          # Compile all sources
```

## 🚀 Usage Instructions

### Initialize filesystem
```bash
./exfs2 -i
```

### Add a file
```bash
./exfs2 -a /vault/file.txt -f file.txt
```

### Extract a file
```bash
./exfs2 -e /vault/file.txt > recovered.txt
```

### Remove a file
```bash
./exfs2 -r /vault/file.txt
```

### List all files and directories
```bash
./exfs2 -l
```

### Debug a file or directory
```bash
./exfs2 -D /vault/file.txt
```

## 🔍 Verifying Output
To confirm the file was extracted correctly:
```bash
cmp file.txt recovered.txt
```

## 🧪 File Generation Tips

To create test files (optional):

```bash
# Create a small text file
echo "hello world" > hello.txt

# Create a 3MB binary file (direct + single indirect)
dd if=/dev/urandom of=bigfile.bin bs=1K count=3000

# Create a ~6MB binary file (should trigger double indirect)
dd if=/dev/urandom of=hugefile.bin bs=1K count=6000
```

## 📁 Project Structure

```
add.c         - Add file logic
extract.c     - Extract file logic
remove.c      - Remove file logic
debug.c       - Debug information printer
helpers.c     - Common utilities (block mapping, directory entry)
init.c        - Filesystem initialization
main.c        - CLI parser/dispatcher
path.c        - Path resolution, traversal, mkdir-like support
exfs2.h       - Shared structs and constants
Makefile      - Build rules
```

## 🚫 Not Implemented

- Triple indirect block support (not required)
- File overwrite/update functionality

---

**Author:** Puspha Raj Pandeya  
**Note:** This project is for CS514 assignment. Triple indirect support was discussed but not required per final specification.
