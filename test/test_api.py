#!/usr/bin/env python3
import requests
import json
import base64
import os
import time
import wave
import pytest
from dotenv import load_dotenv

# 加载环境变量
load_dotenv()

# API 服务器配置
API_HOST = "http://localhost:8090"  # 更新为新的端口
API_KEY = os.getenv("API_KEY", "test_key")  # 从环境变量获取API密钥
print(f"Using API key: {API_KEY}")  # 调试信息

# 通用请求头
HEADERS = {
    "Content-Type": "application/json",
    "Authorization": f"Bearer {API_KEY}"
}

# 打印请求头用于调试
print(f"Request headers: {json.dumps(HEADERS, indent=2)}")

def test_health_check():
    """测试健康检查接口"""
    # 测试正确的API密钥
    response = requests.get(f"{API_HOST}/health", headers=HEADERS)
    assert response.status_code == 200
    data = response.json()
    assert data["status"] == "OK"
    assert data["message"] == "Service is healthy"

    # 测试无认证头
    response = requests.get(f"{API_HOST}/health")
    assert response.status_code == 401
    data = response.json()
    assert data["error"]["code"] == 401
    assert data["error"]["status"] == "UNAUTHENTICATED"

    # 测试错误的API密钥
    wrong_headers = {
        "Content-Type": "application/json",
        "Authorization": "Bearer wrong_key"
    }
    response = requests.get(f"{API_HOST}/health", headers=wrong_headers)
    assert response.status_code == 401
    data = response.json()
    assert data["error"]["code"] == 401
    assert data["error"]["status"] == "UNAUTHENTICATED"

def test_request_size_limit():
    """测试请求大小限制"""
    # 生成一个超过限制的大文件
    large_content = b"0" * (11 * 1024 * 1024)  # 11MB，超过10MB限制
    audio_content = base64.b64encode(large_content).decode("utf-8")

    request_data = {
        "config": {
            "encoding": "LINEAR16",
            "sampleRateHertz": 16000,
            "languageCode": "zh-CN"
        },
        "audio": {
            "content": audio_content
        }
    }

    response = requests.post(
        f"{API_HOST}/recognize",
        json=request_data,
        headers=HEADERS
    )

    assert response.status_code == 413
    data = response.json()
    assert data["error"]["code"] == 413
    assert data["error"]["status"] == "FAILED_PRECONDITION"

# def compress_audio(audio_content, max_size=1024*1024):  # 1MB限制
#     """压缩音频数据到指定大小"""
#     # TODO: 实现实际的音频压缩逻辑
#     # 当前仅截取部分数据用于测试
#     return audio_content[:max_size]

def test_recognize_with_wav():
    """测试WAV文件同步识别"""
    # 读取测试音频文件
    audio_path = "test_data/zh.wav"
    with open(audio_path, "rb") as audio_file:
        audio_content = audio_file.read()
        # 压缩音频数据
        # audio_content = compress_audio(audio_content)
        audio_content = base64.b64encode(audio_content).decode("utf-8")

    # 构造请求数据
    request_data = {
        "config": {
            "encoding": "LINEAR16",
            "sampleRateHertz": 16000,
            "languageCode": "zh-CN",
            "enableAutomaticPunctuation": True,
            "maxAlternatives": 1,
            "enableWordTimeOffsets": True
        },
        "audio": {
            "content": audio_content
        }
    }

    # 测试无认证
    no_auth_response = requests.post(
        f"{API_HOST}/recognize",
        json=request_data
    )
    assert no_auth_response.status_code == 401

    # 测试错误的认证
    wrong_headers = HEADERS.copy()
    wrong_headers["Authorization"] = "Bearer wrong_key"
    wrong_auth_response = requests.post(
        f"{API_HOST}/recognize",
        json=request_data,
        headers=wrong_headers
    )
    assert wrong_auth_response.status_code == 401

    # 测试正确的认证
    response = requests.post(
        f"{API_HOST}/recognize",
        json=request_data,
        headers=HEADERS
    )

    # 验证响应
    assert response.status_code == 200
    data = response.json()
    assert "results" in data
    assert len(data["results"]) > 0
    assert "alternatives" in data["results"][0]
    assert "transcript" in data["results"][0]["alternatives"][0]
    assert "confidence" in data["results"][0]["alternatives"][0]

def test_recognize_with_english():
    """测试英文音频识别"""
    audio_path = "test_data/en.wav"
    with open(audio_path, "rb") as audio_file:
        audio_content = audio_file.read()
        audio_content = base64.b64encode(audio_content).decode("utf-8")

    request_data = {
        "config": {
            "encoding": "LINEAR16",
            "sampleRateHertz": 16000,
            "languageCode": "en-US",
            "enableAutomaticPunctuation": True
        },
        "audio": {
            "content": audio_content
        }
    }

    response = requests.post(
        f"{API_HOST}/recognize",
        json=request_data,
        headers=HEADERS
    )

    assert response.status_code == 200
    data = response.json()
    assert "results" in data

def test_multilingual_recognition():
    """测试多语言识别"""
    test_cases = [
        ("test_data/zh.wav", "zh-CN"),
        ("test_data/en.wav", "en-US"),
        ("test_data/ja.wav", "ja-JP"),
        ("test_data/ko.wav", "ko-KR"),
        ("test_data/yue.wav", "yue-HK")
    ]

    for audio_path, language_code in test_cases:
        with open(audio_path, "rb") as audio_file:
            audio_content = audio_file.read()
            audio_content = base64.b64encode(audio_content).decode("utf-8")

        request_data = {
            "config": {
                "encoding": "LINEAR16",
                "sampleRateHertz": 16000,
                "languageCode": language_code,
                "enableAutomaticPunctuation": True
            },
            "audio": {
                "content": audio_content
            }
        }

        response = requests.post(
            f"{API_HOST}/recognize",
            json=request_data,
            headers=HEADERS
        )

        assert response.status_code == 200
        data = response.json()
        assert "results" in data

def test_error_handling():
    """测试错误处理"""
    # 测试无效的音频数据
    request_data = {
        "config": {
            "encoding": "LINEAR16",
            "sampleRateHertz": 16000,
            "languageCode": "zh-CN"
        },
        "audio": {
            "content": "invalid_base64_content"
        }
    }

    response = requests.post(
        f"{API_HOST}/recognize",
        json=request_data,
        headers=HEADERS
    )

    assert response.status_code == 400

    # 测试不支持的音频格式
    request_data["config"]["encoding"] = "UNSUPPORTED_FORMAT"
    response = requests.post(
        f"{API_HOST}/recognize",
        json=request_data,
        headers=HEADERS
    )

    assert response.status_code == 400

    # 测试缺少必要字段
    request_data = {
        "config": {
            "languageCode": "zh-CN"
        }
    }

    response = requests.post(
        f"{API_HOST}/recognize",
        json=request_data,
        headers=HEADERS
    )

    assert response.status_code == 400

if __name__ == "__main__":
    pytest.main(["-v", __file__]) 