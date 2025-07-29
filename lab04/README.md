## Day 04

run `make all`

- 实现一个泛型函数，接受两个参数交换它们的值
    - run `./build/01_swap`
    - 这里可以用 `std::move` 尝试转换参数为右值触发移动构造来提高效率
    - ![01_swap](static/01_swap.png)
- 利用映射，实现一个简单的英汉词典，要求输入中文或者英文，给出对应的翻译词汇，如果没有输出默认提示
    - 英译汉和汉译英都是一样的逻辑，所以可以用一个参数为字典引用和 `key` 的工具函数来实现代码复用，对外暴露的接口实际上都调用的工具函数
    - run `./build/02_dict`
    - ![02_dict](static/02_dict.png)

### Misc.

> 从隔壁的组员那里听来的先进经验：把目录结构附上 ~~（绷不住）~~

```bash
> exa -T
.
├── config.mk
├── include
│  ├── myDictionary.h
│  └── mySwap.h
├── Makefile
├── README.md
├── src
│  ├── 01_swap.cc
│  ├── 02_dict.cc
│  └── myDictionary.cc
└── static
   ├── 01_swap.png
   └── 02_dict.png
```

- 我尝试用了 `Concept` 来约束参数类型以及（更重要的）让编译时报错更友好些
- 既然都升到 `C++20` 标准了自然也可以用 `optional` 了，更友好的接口
