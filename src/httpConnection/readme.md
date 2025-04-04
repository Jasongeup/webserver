# httpConnection

是服务器的逻辑处理单元，负责提供http服务，包括读写客户数据、解析http请求，写应答报文等，包括HttpConn, HttpRequest, HttpRespose三个类



## HttpConn类

### 功能

是逻辑处理单元的最上层接口，主要被WebServer类调用，运行在线程池中的某个线程中，负责从连接socket上读客户数据、往socket上写应答报文、调用HttpRequest类分析http请求，调用HttpResponse写应答报文

- 使用正则表达式解析http请求内容
- 使用有限状态机完成http请求解析
- 应答报文和请求文件的集中写
- 支持ET/LT模式
- 使用哈希表unordered_map存储头部字段



### 参数

其接受的参数包括：连接socket文件描述符、客户端的socket地址。

创建一个类对象时，初始化资源包括读缓冲区、写缓冲区、分散内存块iovec结构体数组、一个HttpRequest对象、一个HttpResponse对象



### 代码逻辑

- 从连接socket上将数据读入读缓冲区中（ET模式下循环读），初始化HttpRequest对象
- 调用HttpRequest对象解析读缓冲区中的http请求，初始化HttpResponse对象
- 调用HttpResponse对象写http应答报文到写缓冲区中，让第一个分散的内存块结构体指向写缓冲区，第二个内存块结构体指向客户请求的文件在内存中的映射（由HttpResponse完成映射）
- 将分散的内存块写入连接socket上（ET模式下循环写）



## HttpRequest类

### 功能

解析Http请求报文，主要被HttpConn类调用，利用有限状态机，可以解析GET/POST请求，获取http请求的路径。如果是Post请求（登录、注册），则调用WebServer中的数据库连接池单例对象验证



### 参数

类对象构造时不接受参数，创建一个类对象时，其初始化的重要成员函数包括http方法、请求文件路径、http版本、消息体、状态机起始状态、头部字段哈希表、消息体键值对哈希表，状态机枚举量、分析请求结果枚举量（FILE_REQUEST, BAD_REQUEST, GET_REQUEST...）



### 代码逻辑

利用状态机（读请求行、读头部字段、读消息体、结束状态）解析http请求，并获取请求参数

- 读缓冲区作为参数输入，逐行读取缓冲区中的内容（通过serach换行符来实现），判断当前状态

  - 如果是读请求行状态，则分析请求行，通过正则表达式获取请求的方法、请求文件路径、http版本，之后进行状态转移

  - 如果是读头部字段状态，则分析头部字段，利用正则表达式，将头部字段的键值存入到哈希表中，之后进行状态转移

  - 如果是读消息体状态，则解析消息体

    - 消息体的类型必须是html表单键值对形式，解析键值对存储在哈希表中，因为只能处理Post的注册、登录请求，只检查请求文件类型为login，register的键值对；

    - 通过SqlConnRAII类获取数据库连接池中的一个数据连接对象，如果是注册，则检查用户名是否已经使用过，如果是登录，则检查是否在数据库中，根据结果返回welcome或者error页面；

    - 进行状态转移

  

## HttpResponse类

### 功能

根据http请求分析结果，将http应答报文写入写缓冲区，将客户请求文件映射到内存，主要被HttpConn调用



### 参数

类对象初始化时接受资源文件目录路径、客户请求文件路径、http请求分析结果，重要成员有映射的内存地址、映射的文件状态、http应答的状态码



### 代码逻辑

- 首先判断请求的资源文件是否是目录、以及是否可读、http请求分析的结果，更新对应的http应答状态码
- 如果是错误状态码，则更新路径为对应的错误网页
- 添加应答状态行到写缓冲区，包括http版本、状态码、状态描述
- 添加头部字段到写缓冲区，包括是否keep-alive, 返回的消息体类型
- 编辑消息体，先打开用户请求的文件，将文件映射到内存的地址赋给映射内存地址成员（该成员在HttpConn中被赋给内存块结构体），将请求文件的大小写入写缓冲区，而实际的文件内容不放入写缓冲区
