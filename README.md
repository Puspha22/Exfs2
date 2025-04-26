
# ExFS2: Extensible File System 2

ExFS2 is a user-space filesystem implemented in C.  
It manages a virtual file system inside two segment files (`inode_segment_0.seg` and `data_segment_0.seg`) and supports nested directories, file addition, extraction, removal, and listing — entirely self-contained.

---

## 🛠 How to Build

First clone or download this repository.

To build the project:
```bash
make
```

To clean generated files:
```bash
make clean
```

---

## 🚀 How to Run

The executable is `exfs2`. Supported commands:

| Command | Purpose |
|:-------|:--------|
| `-a /path/to/add/inside/fs -f hostfile.txt` | Add a file into ExFS2 filesystem |
| `-l` | List all files and directories inside ExFS2 |
| `-e /path/to/file.txt` | Extract a file from ExFS2 to stdout |
| `-r /path/to/file.txt` | Remove a file from ExFS2 filesystem |
| `-D /path` | Debug a path: print inode info and directory entries |

Example usage:

```bash
# Add a file
./exfs2 -a /a/b/c/hello.txt -f hello.txt

# List contents
./exfs2 -l

# Extract file
./exfs2 -e /a/b/c/hello.txt > recovered.txt

# Compare
diff hello.txt recovered.txt

# Remove file
./exfs2 -r /a/b/c/hello.txt

# Debug a path
./exfs2 -D /a/b
```

---

## 📂 Project Files

| File | Description |
|:----|:------------|
| `exfs2.c` | Main implementation |
| `Makefile` | Build instructions |
| `.gitignore` | Ignore binaries, segment files, recovered files |

---

## ⚡ How Each Function Works

- `run_add()`: Adds a file to a nested directory. Creates directories if they don't exist.
- `run_list()`: Recursively lists the directory tree from root.
- `run_extract()`: Extracts and outputs the file's raw content to stdout.
- `run_remove()`: Removes a file's inode, data block, and its directory entry.
- `run_debug()`: Prints inode and directory details for a given path (used for verifying FS structure).

---

## ⚠️ Known Limitations

- Only one inode segment (`inode_segment_0.seg`) and one data segment (`data_segment_0.seg`) are used.
- No indirect, double, or triple block pointers implemented.
- Files must fit within one data block (4KB).
- No dynamic segment expansion yet.

---

## 👤 Author

- Puspha Raj Pandeya (Puspha22)

---

## 🏁 Final Notes

ExFS2 can handle nested paths, file operations, and directory management inside a simple simulated filesystem.  
Designed for educational purposes (CS514, Spring '25).

Enjoy exploring it! 🚀
