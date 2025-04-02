# buffer

定义了缓冲区类，用于在内存中暂存各种I/O数据，包括Buffer类



## Buffer类

### 功能

缓冲区类，空间大致可分为```[beginptr, readpos, writepos, buffer.end()]```,   ```readpos-writepos```是可读的空间，而其他空间都是空闲的，当然如果没有整理buffer情况下，可写的只有writepos-buffer.end()



### 参数

其接受一个指定缓冲区大小的参数，类对象初始化时，利用vector申请一片空间，重要成员由readpos， writepos



### 代码逻辑

- 从缓冲区只能从readpos-writepos
- 将数据从内存写入到缓冲区，重载了多个写入数据的函数，先判断能否写下那么多数据，如果整理后能写下，则整理缓冲区，如果写不下，则将缓冲区扩容
- 将数据从文件中读到缓冲区，分散读，缓冲区读不下会自动扩容
- 缓冲区内容写到文件

