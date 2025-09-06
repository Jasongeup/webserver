# AI 智能聊天功能

本项目已集成AI智能聊天功能，支持与本地大模型进行对话。

## 功能特性

- 🤖 **智能对话**: 与本地大模型进行自然语言对话
- 🌐 **Web界面**: 现代化的聊天界面，支持实时交互
- ⚡ **实时通信**: 支持HTTP实时通信
- 📱 **响应式设计**: 适配桌面和移动设备
- 🔄 **自动重连**: 连接断开时自动重连
- 🎨 **美观界面**: 现代化的UI设计，支持深色模式

## 快速开始

### 1. 安装依赖

```bash
# 安装Python依赖
pip install torch transformers fastapi uvicorn

# 或者使用conda
conda install pytorch transformers fastapi uvicorn -c pytorch -c conda-forge
```

### 2. 启动服务

#### 启动Web服务器
```bash
# 编译并启动Web服务器
make
./server
```

#### 启动LLM API服务
```bash
# 方法1: 使用启动脚本
cd llm_api
./start_llm_server.sh

# 方法2: 直接运行
cd llm_api
python3 chat_api.py --server --port 8000
```

### 3. 访问聊天页面

- **聊天页面**: http://localhost:1316/chat.html

## 使用说明

### 聊天模式
1. 打开 http://localhost:1316/chat.html
2. 在输入框中输入消息
3. 按Enter发送或点击发送按钮
4. AI会通过HTTP API返回回复

## API接口

### HTTP聊天接口
```
POST /chat
Content-Type: application/json

{
    "message": "你好，请介绍一下自己"
}
```

响应：
```json
{
    "response": "你好！我是AI智能助手..."
}
```


## 配置选项

### LLM API配置
可以通过命令行参数配置LLM API服务：

```bash
python3 chat_api.py --server --port 8000 --model "deepseek-ai/deepseek-llm-7b-chat" --max-tokens 128 --temperature 0.7
```

参数说明：
- `--port`: API服务端口（默认8000）
- `--model`: 模型名称（默认deepseek-ai/deepseek-llm-7b-chat）
- `--max-tokens`: 最大生成token数（默认128）
- `--temperature`: 采样温度（默认0.7）

### 模型选择
支持使用不同的预训练模型：

```bash
# 使用不同的模型
python3 chat_api.py --server --model "microsoft/DialoGPT-medium"
python3 chat_api.py --server --model "facebook/blenderbot-400M-distill"
```

## 测试功能

运行测试脚本验证聊天功能：

```bash
./test_chat.sh
```

测试内容包括：
- Web服务器状态检查
- LLM API服务状态检查
- HTTP聊天接口测试
- HTTP连接测试
- 聊天页面访问测试

## 故障排除

### 常见问题

1. **模型加载失败**
   - 检查网络连接，确保能下载模型
   - 检查磁盘空间，模型文件较大
   - 检查Python环境和依赖包

2. **HTTP连接失败**
   - 检查防火墙设置
   - 确保端口1316未被占用
   - 检查网络连接

3. **聊天无响应**
   - 检查LLM API服务是否运行
   - 查看服务器日志
   - 检查模型是否正确加载

### 日志查看

```bash
# 查看Web服务器日志
tail -f log/$(date +%Y_%m_%d).log

# 查看LLM API服务日志
cd llm_api
python3 chat_api.py --server 2>&1 | tee chat_api.log
```

## 性能优化

### 模型优化
- 使用GPU加速（如果可用）
- 调整max_tokens参数控制生成长度
- 使用量化模型减少内存占用

### 服务器优化
- 调整线程池大小
- 启用连接复用
- 使用负载均衡（多实例部署）

## 开发说明

### 添加新功能
1. 修改 `src/httpConnection/httpConn.cpp` 添加新的HTTP路由
2. 修改 `src/httpConnection/httpConn.cpp` 添加HTTP消息处理
3. 更新前端页面添加新功能

### 自定义UI
- 修改 `resources/css/chat.css` 调整样式
- 修改 `resources/chat.html` 调整布局

## 许可证

本项目遵循MIT许可证。

## 贡献

欢迎提交Issue和Pull Request来改进聊天功能！


