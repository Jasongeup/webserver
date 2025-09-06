#!/usr/bin/env python3
"""
Ultra Fast LLM API Server
使用最轻量级模型，提供快速推理的LLM服务

功能特性：
- 使用Qwen2.5-0.5B轻量级模型，确保快速响应
- 支持HTTP API和命令行两种使用模式
- 优化的生成参数，平衡速度和质量
- 自动清理响应内容，提供更好的用户体验
- 支持自定义模型、生成长度、温度等参数

作者: JasonGe
创建时间: 2025/03/27
"""

import sys
import json
import argparse
import logging
from typing import Optional
from transformers import AutoModelForCausalLM, AutoTokenizer
import torch
from fastapi import FastAPI
from pydantic import BaseModel
import uvicorn

# 配置日志系统
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class UltraFastLLMAPI:
    """
    超快速LLM API类
    
    该类封装了轻量级大语言模型的加载、推理和管理功能，
    专门为快速响应而优化，适合实时对话场景。
    """
    
    def __init__(self, model_name: str = "Qwen/Qwen2.5-0.5B"):
        """
        初始化LLM API实例
        
        Args:
            model_name: 模型名称，默认为Qwen2.5-0.5B轻量级模型
        """
        self.model_name = model_name
        self.tokenizer = None  # 分词器，用于文本编码和解码
        self.model = None      # 语言模型，用于文本生成
        self.device = "cpu"    # 强制使用CPU，避免GPU内存问题和依赖
        logger.info(f"Using device: {self.device}")
        
    def load_model(self):
        """
        加载超轻量级模型
        
        该方法负责：
        1. 下载并加载预训练的分词器和模型
        2. 配置模型参数以优化内存使用
        3. 设置模型为评估模式，禁用梯度计算
        
        Returns:
            bool: 模型加载成功返回True，失败返回False
        """
        try:
            logger.info(f"Loading ultra-fast model: {self.model_name}")
            
            # 加载分词器 - 负责将文本转换为模型可理解的token序列
            self.tokenizer = AutoTokenizer.from_pretrained(self.model_name)
            
            # 设置填充token - 用于批处理时统一序列长度
            if self.tokenizer.pad_token is None:
                self.tokenizer.pad_token = self.tokenizer.eos_token
            
            # 加载语言模型，使用最小配置以节省内存
            self.model = AutoModelForCausalLM.from_pretrained(
                self.model_name, 
                torch_dtype=torch.float32,  # 使用float32确保数值稳定性
                low_cpu_mem_usage=True,     # 启用低内存使用模式
                device_map=None             # 手动管理设备分配
            )
            
            # 将模型移动到CPU设备
            self.model = self.model.to(self.device)
            
            # 设置为评估模式，禁用dropout和batch normalization的训练行为
            self.model.eval()
            
            logger.info("Ultra-fast model loaded successfully!")
            return True
        except Exception as e:
            logger.error(f"Failed to load model: {e}")
            return False
    
    def chat(self, message: str, max_tokens: int = 50, temperature: float = 0.3) -> dict:
        """
        使用真实LLM进行超快速对话
        
        这是核心对话方法，负责处理用户输入并生成AI回复。
        使用优化的生成参数确保快速响应和合理质量。
        
        Args:
            message: 用户输入的对话消息
            max_tokens: 最大生成token数量，默认50个token
            temperature: 采样温度，控制生成的随机性，默认0.3（较低温度=更确定性的输出）
            
        Returns:
            dict: 包含AI回复的字典，格式为{"response": "回复内容"}或{"error": "错误信息"}
        """
        try:
            # 检查模型和分词器是否已加载
            if not self.model or not self.tokenizer:
                return {"error": "模型未加载，请稍后重试"}
            
            # 验证输入消息不为空
            if not message.strip():
                return {"error": "请输入您的问题"}
            
            logger.info(f"Processing message with ultra-fast LLM: {message}")
            
            # 构建对话格式的输入，使用简洁的中文格式
            formatted_input = f"用户：{message}\n助手："
            
            # 将文本编码为模型可理解的token序列
            inputs = self.tokenizer(
                formatted_input, 
                return_tensors="pt",      # 返回PyTorch张量
                truncation=True,          # 启用截断，防止输入过长
                max_length=128           # 限制输入最大长度为128个token
            )
            
            # 使用优化的生成参数进行文本生成
            with torch.no_grad():  # 禁用梯度计算，节省内存和计算时间
                outputs = self.model.generate(
                    **inputs,
                    # 生成长度控制
                    max_new_tokens=max_tokens,        # 限制新生成的token数量
                    
                    # 采样策略参数
                    do_sample=True,                   # 启用随机采样而非贪心解码
                    temperature=temperature,          # 采样温度，控制随机性
                    top_p=0.8,                       # 核采样参数，只考虑累积概率前80%的token
                    top_k=10,                        # 只考虑概率最高的10个token
                    
                    # 重复控制参数
                    repetition_penalty=1.2,          # 重复惩罚系数，减少重复内容
                    no_repeat_ngram_size=2,          # 禁止重复的2-gram
                    
                    # 特殊token设置
                    pad_token_id=self.tokenizer.eos_token_id,  # 填充token
                    eos_token_id=self.tokenizer.eos_token_id,  # 结束token
                    
                    # 搜索策略参数
                    num_beams=1,                     # 不使用beam search，使用贪心搜索提高速度
                    length_penalty=1.0,              # 长度惩罚系数
                    
                    # 性能优化参数
                    use_cache=True,                  # 使用键值缓存，加速生成
                    output_scores=False,             # 不输出每个token的分数
                    return_dict_in_generate=False    # 简化输出格式
                )
            
            # 将生成的token序列解码为文本
            full_response = self.tokenizer.decode(outputs[0], skip_special_tokens=True)
            
            # 提取助手的回复部分，去除用户输入
            if "助手：" in full_response:
                response = full_response.split("助手：")[-1].strip()
            else:
                # 如果格式不匹配，截取输入长度之后的部分
                response = full_response[len(formatted_input):].strip()
            
            # 清理响应内容，移除可能的格式标记和多余内容
            response = self._clean_response(response)
            
            # 如果响应为空或太短，提供默认回复
            if not response or len(response) < 2:
                response = "我理解您的问题，让我思考一下。"
            
            logger.info(f"Generated ultra-fast response: {response}")
            return {"response": response}
            
        except Exception as e:
            logger.error(f"Ultra-fast chat error: {e}")
            return {"error": f"AI服务暂时不可用: {str(e)}"}
    
    def _clean_response(self, response: str) -> str:
        """
        清理响应内容，移除额外的对话格式标记
        
        该方法负责清理模型生成的原始响应，移除可能包含的：
        - 对话格式标记（用户：、助手：等）
        - 多余的换行和空白
        - 不相关的对话内容
        
        Args:
            response: 原始响应文本
            
        Returns:
            str: 清理后的响应文本
        """
        # 移除可能的用户输入标记
        response = response.replace("用户：", "").replace("User:", "").replace("Human:", "")
        
        # 移除可能的助手标记
        response = response.replace("助手：", "").replace("Assistant:", "").replace("AI:", "")
        
        # 移除可能的对话格式标记
        response = response.replace("A:", "").replace("B:", "").replace("A1", "").replace("D", "")
        
        # 按行分割，只保留第一行有效内容
        lines = response.split('\n')
        cleaned_lines = []
        
        for line in lines:
            line = line.strip()
            # 跳过空行和包含对话标记的行
            if (line and 
                not line.startswith("用户：") and 
                not line.startswith("助手：") and
                not line.startswith("User:") and
                not line.startswith("Assistant:") and
                not line.startswith("AI:") and
                not line.startswith("A:") and
                not line.startswith("B:") and
                not line.startswith("A1") and
                not line.startswith("D") and
                len(line) > 1):
                cleaned_lines.append(line)
                break  # 只取第一行有效内容
        
        return " ".join(cleaned_lines).strip()

def main():
    """
    主函数 - 程序入口点
    
    支持两种运行模式：
    1. HTTP服务器模式：启动FastAPI服务器，提供REST API接口
    2. 命令行模式：直接处理单个消息并输出结果
    """
    # 配置命令行参数解析器
    parser = argparse.ArgumentParser(description="Ultra Fast LLM Chat API")
    parser.add_argument("message", nargs="?", help="Message to send to the model")
    parser.add_argument("--model", default="Qwen/Qwen2.5-0.5B", help="Model name")
    parser.add_argument("--max-tokens", type=int, default=30, help="Maximum tokens to generate")
    parser.add_argument("--temperature", type=float, default=0.8, help="Sampling temperature")
    parser.add_argument("--server", action="store_true", help="Start as HTTP server")
    parser.add_argument("--port", type=int, default=8000, help="Server port")
    
    args = parser.parse_args()
    
    # 创建超快速LLM API实例
    chat_api = UltraFastLLMAPI(args.model)
    
    if args.server:
        # HTTP服务器模式
        app = FastAPI(title="Ultra Fast LLM Chat API", version="1.0.0")
        
        # 定义聊天请求的数据模型
        class ChatRequest(BaseModel):
            message: str
            max_tokens: Optional[int] = args.max_tokens
            temperature: Optional[float] = args.temperature
        
        # 服务器启动事件 - 加载模型
        @app.on_event("startup")
        async def startup_event():
            logger.info("Starting up Ultra Fast LLM API server...")
            if not chat_api.load_model():
                logger.error("Failed to load ultra-fast model, server will not work properly")
                sys.exit(1)
            logger.info("Ultra Fast LLM API server started successfully!")
        
        # 聊天API端点 - 处理POST请求
        @app.post("/chat")
        async def chat_endpoint(req: ChatRequest):
            """
            HTTP聊天接口
            
            接收JSON格式的聊天请求，返回AI回复
            请求格式: {"message": "用户消息", "max_tokens": 50, "temperature": 0.3}
            响应格式: {"response": "AI回复"} 或 {"error": "错误信息"}
            """
            return chat_api.chat(req.message, req.max_tokens, req.temperature)
        
        # 健康检查端点 - 检查服务状态
        @app.get("/health")
        async def health_check():
            """
            健康检查接口
            
            返回服务器状态信息，包括模型加载状态、设备信息等
            """
            return {
                "status": "healthy", 
                "model_loaded": chat_api.model is not None,
                "model_name": chat_api.model_name,
                "device": chat_api.device,
                "response_type": "ultra_fast_llm"
            }
        
        logger.info(f"Starting Ultra Fast LLM server on port {args.port}")
        uvicorn.run(app, host="0.0.0.0", port=args.port)
        
    else:
        # 命令行模式 - 直接处理单个消息
        if not args.message:
            print("Error: Message is required for command line mode")
            sys.exit(1)
        
        # 加载模型
        if not chat_api.load_model():
            print(json.dumps({"error": "Failed to load ultra-fast model"}))
            sys.exit(1)
        
        # 处理消息并输出结果
        result = chat_api.chat(args.message, args.max_tokens, args.temperature)
        print(json.dumps(result, ensure_ascii=False))

if __name__ == "__main__":
    main()

