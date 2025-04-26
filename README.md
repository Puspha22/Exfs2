
# ExFS2: Extensible File System 2

ExFS2 is a simple user-space filesystem implemented in C.  
It simulates a filesystem using segment files (`inode_segment_0.seg` and `data_segment_0.seg`) and supports adding, listing, extracting, and removing files inside a virtual directory tree.

---

## 🛠 How to Build

First, clone or download this repository.

To build the project:
```bash
make
```

To clean previous builds and segment files:
```bash
make clean
```

---

## 🚀 How to Run

Basic usage:
```bash
./exfs2 -a <exfs_path> -f <host_path>    # Add a file
./exfs2 -l                                # List files and directories
./exfs2 -e <exfs_path> > <output_file>     # Extract a file
./exfs2 -r <exfs_path>                    # Remove a file
./exfs2 -D <exfs_path>                    # Debug a file or directory
```

Example:
```bash
# Create a test file
echo "Hello ExFS2!" > hello.txt

# Add the file into filesystem under /a/b/c/hello.txt
./exfs2 -a /a/b/c/hello.txt -f hello.txt

# List filesystem
./exfs2 -l

# Extract the file back
./exfs2 -e /a/b/c/hello.txt > recovered.txt

# Compare original and extracted
diff hello.txt recovered.txt

# Debug directory structure
./exfs2 -D /
./exfs2 -D /a/b/c

# Remove the file
./exfs2 -r /a/b/c/hello.txt
```

---

## 📚 Features

- ✅ **Segment-based storage** (`inode_segment_0.seg`, `data_segment_0.seg`)
- ✅ **Directory and file inodes**
- ✅ **Nested directory creation**
- ✅ **File addition and extraction**
- ✅ **Recursive directory listing**
- ✅ **File removal**
- ✅ **Debug command (-D)**
- ✅ **Binary-safe operations (fread/fwrite)**
- ✅ **Self-contained filesystem** (no dependency on host filesystem beyond segments)

---

## 📄 Files Included

- `exfs2.c` — Full filesystem source code
- `Makefile` — Build script
- `.gitignore` — Clean ignored files
- `README.md` — (This file)

---

## ⚙️ Commands Implemented

| Command     | Description                                  |
|-------------|----------------------------------------------|
| `-a`        | Add a file to the filesystem                 |
| `-l`        | List the contents of the filesystem          |
| `-e`        | Extract a file from the filesystem           |
| `-r`        | Remove a file from the filesystem            |
| `-D`        | Debug: Show detailed info about a path       |

---

## 🧹 Clean-up Instructions

After testing, you can clean everything with:
```bash
make clean
```

---

## 🙋 Notes

- Large file support (indirect blocks) is **NOT** implemented — only small single-block files are supported (bonus not attempted).
- Dynamic segment expansion is **NOT** implemented — fixed single segment per inode and data.

---

## 👨‍💻 Author

- **Puspha Raj Pandeya** — 2025  
- Southern Illinois University Edwardsville (SIUE)

---

✅ Now your project is **buildable**, **runnable**, and **shareable** instantly!
