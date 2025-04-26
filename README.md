# ExFS2: Extensible File System 2

ExFS2 is a simple user-space filesystem implemented in C.  
It simulates a filesystem using segment files (`inode_segment_0.seg` and `data_segment_0.seg`) and supports adding, listing, extracting, and removing files inside a virtual directory tree.

---

## 🛠 How to Build

First clone or download this repository.

To build the project:
```bash
make
```

To clean the project:
```bash
make clean
```

---

## 🚀 How to Run

### 1. Create a sample file
```bash
echo "Hello Nested FS!" > hello.txt
```

### 2. Add a file to the virtual filesystem
```bash
./exfs2 -a /a/b/c/hello.txt -f hello.txt
```

### 3. List all files
```bash
./exfs2 -l
```
Example output:
```
|- a
   |- b
      |- c
         |- hello.txt
```

### 4. Extract a file from the filesystem
```bash
./exfs2 -e /a/b/c/hello.txt > recovered.txt
```

### 5. Compare original and recovered file
```bash
diff hello.txt recovered.txt
```
- If there's no output, the files are identical ✅.

### 6. Remove a file
```bash
./exfs2 -r /a/b/c/hello.txt
```

---

## 📂 Project Files

| File                | Purpose                                 |
|---------------------|-----------------------------------------|
| `exfs2.c`            | Main C code implementing ExFS2         |
| `Makefile`           | Makefile to build and clean the project |
| `.gitignore`         | To ignore segment and binary files     |
| `inode_segment_0.seg`| Segment file storing inodes (auto-created) |
| `data_segment_0.seg` | Segment file storing file data (auto-created) |

---

## 📚 Explanation of Major Functions

- `run_add(path, file)`:
  - Adds a file into ExFS2, creating directories as needed.

- `run_list()`:
  - Recursively lists the filesystem tree starting from root.

- `run_extract(path)`:
  - Extracts a file's contents to stdout (can redirect to a file).

- `run_remove(path)`:
  - Removes a file and reclaims its inode and block.

- `init_filesystem()`:
  - Initializes or loads the segment files and root inode.

---

## 📜 How to Test Everything

Here’s a complete test flow:
```bash
make
echo "Hello Nested FS!" > hello.txt
./exfs2 -a /a/b/c/hello.txt -f hello.txt
./exfs2 -l
./exfs2 -e /a/b/c/hello.txt > recovered.txt
diff hello.txt recovered.txt
./exfs2 -r /a/b/c/hello.txt
./exfs2 -l
make clean
```

---

## 📎 Requirements

- Linux, WSL, or compatible UNIX environment
- `gcc` (C compiler)
- `make` (build tool)

---

## 📖 Notes

- No need to manually create directories — ExFS2 will auto-create nested directories when adding a file.
- The recovered files should match the original exactly after extraction.
- Segment files are created automatically during the first add.

---

## 📜 License

Educational use only (CS514 Spring 2025 Assignment).  
