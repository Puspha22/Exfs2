#!/bin/bash

set -e  # Exit on any error

echo "[init] Cleaning old segment and temp files..."
rm -f inode_segment_*.seg data_segment_*.seg exfs2 *.o \
      hello.txt recovered.txt bigfile.bin recovered_big.bin \
      huge.bin recovered_huge.bin

echo "[build] Compiling filesystem..."
make clean && make

# Check if exfs2 was built successfully
if [[ ! -x ./exfs2 ]]; then
  echo "[error] exfs2 binary not found after build. Aborting test."
  exit 1
fi

echo "[init] Initializing filesystem..."
./exfs2 -l || true

# === Small file test ===
echo "[test] Creating hello.txt..."
echo "Hello World!" > hello.txt
./exfs2 -a /greeting/hello.txt -f hello.txt

echo "[test] Extracting hello.txt..."
./exfs2 -e /greeting/hello.txt > recovered.txt
diff hello.txt recovered.txt && echo "✅ Small file test passed"

echo "[test] Removing hello.txt..."
./exfs2 -r /greeting/hello.txt
./exfs2 -l

# === Medium file test (~1MB) ===
echo "[test] Creating 1MB bigfile.bin..."
dd if=/dev/urandom of=bigfile.bin bs=1M count=1 status=none

echo "[test] Adding bigfile.bin..."
./exfs2 -a /deep/big.bin -f bigfile.bin
./exfs2 -D /deep/big.bin

echo "[test] Extracting bigfile.bin..."
./exfs2 -e /deep/big.bin > recovered_big.bin
cmp bigfile.bin recovered_big.bin && echo "✅ Medium file test passed"

# === Large file test (~5MB, triggers double indirect) ===
echo "[test] Creating 5MB huge.bin..."
dd if=/dev/urandom of=huge.bin bs=1M count=5 status=none

echo "[test] Adding huge.bin..."
./exfs2 -a /vault/huge.bin -f huge.bin
./exfs2 -D /vault/huge.bin

echo "[test] Extracting huge.bin..."
./exfs2 -e /vault/huge.bin > recovered_huge.bin
cmp huge.bin recovered_huge.bin && echo "✅ Large file test (double indirect) passed"

# === Cleanup ===
echo "[cleanup] Removing test artifacts..."
rm -f hello.txt recovered.txt bigfile.bin recovered_big.bin huge.bin recovered_huge.bin

echo "[final] Listing filesystem contents..."
./exfs2 -l

echo "✅ All tests passed successfully."
