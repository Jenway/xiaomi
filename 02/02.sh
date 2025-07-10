#!/bin/bash
set -e

echo "设置 ulimit 为 unlimited"
ulimit -c unlimited

CORE_PATTERN_FILE="/proc/sys/kernel/core_pattern"
BACKUP_FILE="core_pattern.bak"
sudo cp "$CORE_PATTERN_FILE" "$BACKUP_FILE"

echo "设置 core_pattern 为当前目录"
sudo sh -c "echo './core.%e.%p' > $CORE_PATTERN_FILE"

echo "编译"
g++ -g -o crash crash.cc

echo "运行程序"
./crash || true 

core_file=$(ls -t core.crash.* | head -n1)

if [[ -f "$core_file" ]]; then
    echo "使用 GDB"
    gdb -batch -ex "bt" ./crash "$core_file" > bt.txt
    echo "调用栈保存到 bt.txt: "
    cat bt.txt
else
    echo "没有找到 core 文件"
fi

sudo cp "$BACKUP_FILE" "$CORE_PATTERN_FILE"
rm -rf $BACKUP_FILE

