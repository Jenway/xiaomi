## Day1

初始化 project: cpp-hello-service , 并提交项目到 gitLab ,项目内容包括：

- Git 项目创建和提交 提交 hello.cpp
- 提交编译 hello.cpp 的 cmake 文件
    - run `01/01.sh`
- 提交一个触发 coredump 的函数，并提交 core 文件。
- 提交 gdb 调试 coredump 的调用栈信息 bt.txt 。
    - run `02/02.sh`
- 写一个具有基本功能的 shell，并使用 hello 测试程序在 shell 上执行。
    - run `03/03.sh`,然后 cd 到 01 目录执行 `./build/hello_world`
- 利用cmake配置一个静态库，可执行程序去调用这个静态库方法。
    - run `04/04.sh`
- 利用cmake配置一个动态库，可执行程序去调用这个动态库方法。
    - run `04/05.sh`
