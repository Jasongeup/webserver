#!/usr/bin/env python3
"""
Ultra Fast LLM API Server
使用最轻量级模型，提供快速推理的LLM服务
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

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class UltraFastLLMAPI:
    def __init__(self, model_name: str = "Qwen/Qwen2.5-0.5B"):
        self.model_name = model_name
        self.tokenizer = None
        self.model = None
        self.device = "cpu"  # 强制使用CPU，避免GPU内存问题
        logger.info(f"Using device: {self.device}")
        
    def load_model(self):
        """加载超轻量级模型"""
        try:
            logger.info(f"Loading ultra-fast model: {self.model_name}")
            
            # 加载tokenizer
            self.tokenizer = AutoTokenizer.from_pretrained(self.model_name)
            
            # 设置pad_token
            if self.tokenizer.pad_token is None:
                self.tokenizer.pad_token = self.tokenizer.eos_token
            
            # 加载模型，使用最小配置
            self.model = AutoModelForCausalLM.from_pretrained(
                self.model_name, 
                torch_dtype=torch.float32,  # 使用float32确保稳定性
                low_cpu_mem_usage=True,     # 最小内存使用
                device_map=None             # 手动管理设备
            )
            
            # 移动到CPU
            self.model = self.model.to(self.device)
            
            # 设置为评估模式
            self.model.eval()
            
            logger.info("Ultra-fast model loaded successfully!")
            return True
        except Exception as e:
            logger.error(f"Failed to load model: {e}")
            return False
    
    def chat(self, message: str, max_tokens: int = 50, temperature: float = 0.3) -> dict:
        """使用真实LLM进行超快速对话"""
        try:
            if not self.model or not self.tokenizer:
                return {"error": "模型未加载，请稍后重试"}
            
            if not message.strip():
                return {"error": "请输入您的问题"}
            
            logger.info(f"Processing message with ultra-fast LLM: {message}")
            
            # 构建输入，使用更简洁的格式
            formatted_input = f"用户：{message}\n助手："
            
            # 编码输入，限制长度
            inputs = self.tokenizer(
                formatted_input, 
                return_tensors="pt", 
                truncation=True, 
                max_length=128  # 限制输入长度
            )
            
            # 使用更稳定的生成参数
            with torch.no_grad():
                outputs = self.model.generate(
                    **inputs,
                    max_new_tokens=max_tokens,        # 限制生成长度
                    do_sample=True,                   # 启用采样
                    temperature=temperature,          # 降低温度，提高稳定性
                    top_p=0.8,                       # 降低核采样
                    top_k=10,                        # 减少候选词数量
                    repetition_penalty=1.2,          # 增加重复惩罚
                    pad_token_id=self.tokenizer.eos_token_id,
                    eos_token_id=self.tokenizer.eos_token_id,
                    # 生成参数
                    num_beams=1,                     # 不使用beam search
                    no_repeat_ngram_size=2,          # 增加重复检查
                    length_penalty=1.0,              # 长度惩罚
                    # 优化参数
                    use_cache=True,                  # 使用缓存
                    output_scores=False,             # 不输出分数
                    return_dict_in_generate=False    # 简化输出
                )
            
            # 解码响应
            full_response = self.tokenizer.decode(outputs[0], skip_special_tokens=True)
            
            # 提取助手回复部分
            if "助手：" in full_response:
                response = full_response.split("助手：")[-1].strip()
            else:
                response = full_response[len(formatted_input):].strip()
            
            # 清理响应 - 移除可能的额外对话内容
            response = self._clean_response(response)
            
            # 如果响应为空或太短，提供简短回复
            if not response or len(response) < 2:
                response = "我理解您的问题，让我思考一下。"
            
            logger.info(f"Generated ultra-fast response: {response}")
            return {"response": response}
            
        except Exception as e:
            logger.error(f"Ultra-fast chat error: {e}")
            return {"error": f"AI服务暂时不可用: {str(e)}"}
    
    def _clean_response(self, response: str) -> str:
        """清理响应，移除额外的对话内容"""
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
        app = FastAPI(title="Ultra Fast LLM Chat API", version="1.0.0")
        
        class ChatRequest(BaseModel):
            message: str
            max_tokens: Optional[int] = args.max_tokens
            temperature: Optional[float] = args.temperature
        
        @app.on_event("startup")
        async def startup_event():
            logger.info("Starting up Ultra Fast LLM API server...")
            if not chat_api.load_model():
                logger.error("Failed to load ultra-fast model, server will not work properly")
                sys.exit(1)
            logger.info("Ultra Fast LLM API server started successfully!")
        
        @app.post("/chat")
        async def chat_endpoint(req: ChatRequest):
            return chat_api.chat(req.message, req.max_tokens, req.temperature)
        
        @app.get("/health")
        async def health_check():
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
        # 命令行模式
        if not args.message:
            print("Error: Message is required for command line mode")
            sys.exit(1)
        
        if not chat_api.load_model():
            print(json.dumps({"error": "Failed to load ultra-fast model"}))
            sys.exit(1)
        
        result = chat_api.chat(args.message, args.max_tokens, args.temperature)
        print(json.dumps(result, ensure_ascii=False))

if __name__ == "__main__":
    main()

