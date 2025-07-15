## Day 06

使用epoll开发一个文件传输的服务器和客户端，要求:

1. 使用TCP协议创建连接
2. 客户端申请指定的文件名，服务器返回对应的文件数据，客户端将收到的数据写人本地文件以及其它异常情况处理(Json)
3. 无指定文件则返回“not file”字符串
4. 运行过程中，使用tcpdump抓包，过滤无关端口，截图上传


### 协议设计

- 使用 JSON 格式请求与响应。
- 这里使用了 `nlohmann/json.hpp` 库来进行 JSON 字符串解析和处理

  * 请求文件：`{"filename":"test.txt"}`
  * 成功响应：`{"filesize":文件大小}` + 文件数据
  * 错误响应：`{"error":"错误原因"}`

### 组件

* **Socket**
    - socket 的 RAII wrapper，支持非阻塞监听与
    - 提供 accept 接口非阻塞返回 Client 的文件描述符
* **Poller**
    - epoll 的简单封装，提供添加/删除 fd 接口与事件监听接口
    - 事件接口返回一个触发时间的迭代器的范围以便遍历
* **Connection**
    - 负责处理单个客户端的协议逻辑，进行文件传输和错误处理
    -  这里调用了 `sendfile` 零拷贝接口，提高了效率
* **ClientConnection**
    - 客户端连接封装，实现文件请求和数据接收。
    - 比较简单，所以没有进行分层，ClientConnection 类会创建一个 socket fd 并尝试连接到 server，然后发出请求再接收

## 使用说明

1. 编译项目：

   ```bash
    cmake -Bbuild . && cmake --build build
   ```

2. 启动服务器：

   ```bash
   ./fileServer <端口号>
   ```

3. 启动客户端并请求文件：

   ```bash
   ./fileClient 127.0.0.1 <端口号> <test.txt>
   ```

4. 文件传输完成后，客户端会在本地生成对应文件。

### 结果

![result](assets/wireshark.png)

如图所示，首先客户端和服务端进行三次握手，这一部分由内核以及 socket 库负责

- SYN
- SYN ACK
- ACK + PSH

> 注意到这个地方在第三次握手的同时消息同时发出，优化了效率

Server 先发送 ACK 确认请求

然后发出 PUSH ACK，这里是协议头 json

由于文件是走的 sendfile，所以又有一个 FIN PUSH ACK，可以看出文件比较小，所以直接发完顺便发了 FIN，也就是四次挥手的第一次

- client 先发 ACK 确认接收到了 这个 FIN
- 然后又发 FIN ACK，表明已经不需要继续 tcp 连接的
- server 回复 ACK 确认

四次挥手结束，连接正常关闭
