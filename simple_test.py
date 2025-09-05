#!/usr/bin/env python3
"""
简单的聊天功能测试脚本
测试LLM API服务是否正常工作
"""

import requests
import json
import time
import sys

def test_llm_api():
    """测试LLM API服务"""
    print("测试LLM API服务...")
    
    try:
        # 测试健康检查
        response = requests.get("http://localhost:8000/health", timeout=5)
        if response.status_code == 200:
            print("✓ LLM API服务健康检查通过")
            data = response.json()
            print(f"  状态: {data.get('status')}")
            print(f"  模型已加载: {data.get('model_loaded')}")
        else:
            print(f"✗ LLM API服务健康检查失败: {response.status_code}")
            return False
    except Exception as e:
        print(f"✗ 无法连接到LLM API服务: {e}")
        return False
    
    try:
        # 测试聊天接口
        test_message = "你好，请简单介绍一下自己"
        payload = {"message": test_message}
        
        print(f"发送测试消息: {test_message}")
        response = requests.post("http://localhost:8000/chat", 
                               json=payload, 
                               timeout=30)
        
        if response.status_code == 200:
            data = response.json()
            if "response" in data:
                print("✓ 聊天接口测试通过")
                print(f"  AI回复: {data['response'][:100]}...")
                return True
            else:
                print(f"✗ 聊天接口响应格式错误: {data}")
                return False
        else:
            print(f"✗ 聊天接口测试失败: {response.status_code}")
            print(f"  响应内容: {response.text}")
            return False
            
    except Exception as e:
        print(f"✗ 聊天接口测试异常: {e}")
        return False

def test_direct_llm():
    """直接测试LLM命令行接口"""
    print("\n测试LLM命令行接口...")
    
    try:
        import subprocess
        result = subprocess.run([
            "python3", "llm_api/chat_api.py", 
            "你好，请简单介绍一下自己"
        ], capture_output=True, text=True, timeout=30)
        
        if result.returncode == 0:
            print("✓ LLM命令行接口测试通过")
            response = json.loads(result.stdout)
            if "response" in response:
                print(f"  AI回复: {response['response'][:100]}...")
                return True
            else:
                print(f"✗ LLM响应格式错误: {response}")
                return False
        else:
            print(f"✗ LLM命令行接口测试失败")
            print(f"  错误输出: {result.stderr}")
            return False
            
    except Exception as e:
        print(f"✗ LLM命令行接口测试异常: {e}")
        return False

def main():
    print("=== AI聊天功能简单测试 ===")
    print()
    
    # 测试LLM API服务
    api_success = test_llm_api()
    
    # 如果API服务失败，尝试直接测试LLM
    if not api_success:
        print("\nAPI服务测试失败，尝试直接测试LLM...")
        direct_success = test_direct_llm()
        
        if direct_success:
            print("\n🎉 LLM功能正常，但API服务可能有问题")
            print("建议检查API服务配置和依赖")
        else:
            print("\n❌ LLM功能测试失败")
            print("请检查模型安装和Python环境")
    else:
        print("\n🎉 所有测试通过！聊天功能正常工作")
        print("\n访问信息:")
        print("- LLM API服务: http://localhost:8000")
        print("- 健康检查: http://localhost:8000/health")
        print("- 聊天接口: http://localhost:8000/chat")

if __name__ == "__main__":
    main()

