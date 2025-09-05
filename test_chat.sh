#!/bin/bash

# 聊天功能测试脚本

echo "=== AI聊天功能测试脚本 ==="
echo

# 检查服务器是否运行
check_server() {
    echo "1. 检查Web服务器状态..."
    if pgrep -f "./server" > /dev/null; then
        echo "   ✓ Web服务器正在运行"
        return 0
    else
        echo "   ✗ Web服务器未运行"
        echo "   请先运行: make && ./server"
        return 1
    fi
}

# 检查LLM API服务
check_llm_api() {
    echo "2. 检查LLM API服务..."
    if pgrep -f "chat_api.py" > /dev/null; then
        echo "   ✓ LLM API服务正在运行"
        return 0
    else
        echo "   ✗ LLM API服务未运行"
        echo "   请先运行: cd llm_api && python3 chat_api.py --server"
        return 1
    fi
}

# 测试HTTP聊天接口
test_http_chat() {
    echo "3. 测试HTTP聊天接口..."
    
    response=$(curl -s -X POST http://localhost:1316/chat \
        -H "Content-Type: application/json" \
        -d '{"message": "你好，请介绍一下自己"}' 2>/dev/null)
    
    if echo "$response" | grep -q "response"; then
        echo "   ✓ HTTP聊天接口正常"
        echo "   响应: $response"
        return 0
    else
        echo "   ✗ HTTP聊天接口异常"
        echo "   响应: $response"
        return 1
    fi
}

# 测试WebSocket连接
test_websocket() {
    echo "4. 测试WebSocket连接..."
    
    # 使用websocat工具测试WebSocket连接
    if command -v websocat &> /dev/null; then
        echo "   使用websocat测试WebSocket..."
        timeout 5 websocat ws://localhost:1316/ws <<< '{"type":"chat_message","message":"测试消息"}' 2>/dev/null
        if [ $? -eq 0 ]; then
            echo "   ✓ WebSocket连接正常"
            return 0
        else
            echo "   ✗ WebSocket连接异常"
            return 1
        fi
    else
        echo "   跳过WebSocket测试 (需要安装websocat)"
        return 0
    fi
}

# 测试聊天页面
test_chat_pages() {
    echo "5. 测试聊天页面..."
    
    # 测试普通聊天页面
    http_status=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:1316/chat.html)
    if [ "$http_status" = "200" ]; then
        echo "   ✓ 普通聊天页面可访问"
    else
        echo "   ✗ 普通聊天页面不可访问 (HTTP $http_status)"
    fi
    
    # 测试WebSocket聊天页面
    ws_status=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:1316/chat_ws.html)
    if [ "$ws_status" = "200" ]; then
        echo "   ✓ WebSocket聊天页面可访问"
    else
        echo "   ✗ WebSocket聊天页面不可访问 (HTTP $ws_status)"
    fi
}

# 显示访问信息
show_access_info() {
    echo
    echo "=== 访问信息 ==="
    echo "普通聊天页面: http://localhost:1316/chat.html"
    echo "WebSocket聊天页面: http://localhost:1316/chat_ws.html"
    echo "API接口: http://localhost:1316/chat"
    echo "WebSocket接口: ws://localhost:1316/ws"
    echo
}

# 主测试流程
main() {
    echo "开始测试聊天功能..."
    echo
    
    all_passed=true
    
    check_server || all_passed=false
    echo
    
    check_llm_api || all_passed=false
    echo
    
    if [ "$all_passed" = true ]; then
        test_http_chat || all_passed=false
        echo
        
        test_websocket || all_passed=false
        echo
        
        test_chat_pages || all_passed=false
        echo
    fi
    
    show_access_info
    
    if [ "$all_passed" = true ]; then
        echo "🎉 所有测试通过！聊天功能已就绪。"
    else
        echo "❌ 部分测试失败，请检查上述错误信息。"
        echo
        echo "启动步骤："
        echo "1. 启动Web服务器: make && ./server"
        echo "2. 启动LLM API服务: cd llm_api && python3 chat_api.py --server"
        echo "3. 访问聊天页面进行测试"
    fi
}

# 运行测试
main


