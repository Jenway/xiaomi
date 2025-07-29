mkdir -p build && cmake -Bbuild . && cmake --build build && ./build/shared_hello
echo
echo "检查 RUNPATH 设置"
readelf -d build/shared_hello | grep -i 'PATH'

echo
echo "查看链接库"
ldd build/shared_hello | grep libhello