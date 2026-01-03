#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import requests
import json
import time

SERVER_URL = "http://localhost:8080"

def print_section(title):
    """打印章节标题"""
    print("=" * 50)
    print(title)
    print("=" * 50)
    print()

def make_request(endpoint, method="GET", data=None, headers=None):
    """发送HTTP请求并处理响应"""
    try:
        url = f"{SERVER_URL}{endpoint}"

        if method.upper() == "GET":
            response = requests.get(url, headers=headers)
        elif method.upper() == "POST":
            if data and headers and "Content-Type" in headers and "application/json" in headers["Content-Type"]:
                response = requests.post(url, json=data, headers=headers)
            else:
                response = requests.post(url, data=data, headers=headers)
        else:
            print(f"不支持的HTTP方法: {method}")
            return None

        response.raise_for_status()  # 检查HTTP错误

        # 尝试解析为JSON
        try:
            return response.json()
        except json.JSONDecodeError:
            return response.text

    except requests.exceptions.RequestException as e:
        print(f"请求失败: {e}")
        return None
    except Exception as e:
        print(f"发生错误: {e}")
        return None

def test_health_check():
    """测试健康检查"""
    print_section("1. 测试健康检查...")
    result = make_request("/health", "GET")
    if result:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    print()

def test_cpp_code():
    """测试C++代码执行"""
    print_section("2. 测试C++代码执行...")

    data = {
        "submission_id": 1,
        "language": "cpp",
        "code": """#include <iostream>
                    int main() {
                        int a, b;
                        std::cin >> a >> b;
                        std::cout << a + b << std::endl;
                        return 0;
                    }""",
        "test_cases": [
            {
                "case_id": 1,
                "stdin": "1 2",
                "expected": "3"
            },
            {
                "case_id": 2,
                "stdin": "5 7",
                "expected": "12"
            }
        ],
        "resources_limits": {
            "cpu_time": 1000,
            "memory_bytes": 268435456,
            "stack_bytes": 8388608,
            "output_bytes": 1048576
        },
        "message": "",
        "seccomp_profile": ""
    }

    result = make_request("/submit", "POST", data, {"Content-Type": "application/json"})
    if result:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    print()

def test_python_code():
    """测试Python代码执行"""
    print_section("3. 测试Python代码执行...")

    data = {
        "submission_id": 2,
        "language": "python",
        "code": """a, b = map(int, input().split())
print(a + b)""",
        "test_cases": [
            {
                "case_id": 1,
                "stdin": "1 2",
                "expected": "3"
            }
        ],
        "resources_limits": {
            "cpu_time": 2000,
            "memory_bytes": 268435456,
            "stack_bytes": 8388608,
            "output_bytes": 1048576
        },
        "message": "",
        "seccomp_profile": ""
    }

    result = make_request("/submit", "POST", data, {"Content-Type": "application/json"})
    if result:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    print()

def test_c_code():
    """测试C代码执行"""
    print_section("4. 测试C代码执行...")

    data = {
        "submission_id": 3,
        "language": "c",
        "code": """#include <stdio.h>
int main() {
    int a, b;
    scanf("%d %d", &a, &b);
    printf("%d\\n", a + b);
    return 0;
}""",
        "test_cases": [
            {
                "case_id": 1,
                "stdin": "3 4",
                "expected": "7"
            },
            {
                "case_id": 2,
                "stdin": "100 4",
                "expected": "104"
            },
            {
                "case_id": 3,
                "stdin": "1000 4000",
                "expected": "5000"
            }
        ],
        "resources_limits": {
            "cpu_time": 1000,
            "memory_bytes": 268435456,
            "stack_bytes": 8388608,
            "output_bytes": 1048576
        },
        "message": "",
        "seccomp_profile": ""
    }

    result = make_request("/submit", "POST", data, {"Content-Type": "application/json"})
    if result:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    print()

def test_compile_error():
    """测试编译错误处理"""
    print_section("5. 测试编译错误处理...")

    data = {
        "submission_id": 4,
        "language": "cpp",
        "code": """#include <iostream>
int main() {
    std::cout << "Missing semicolon"
    return 0;
}""",
        "test_cases": [
            {
                "case_id": 1,
                "stdin": "",
                "expected": "test"
            }
        ],
        "resources_limits": {
            "cpu_time": 1000,
            "memory_bytes": 268435456,
            "stack_bytes": 8388608,
            "output_bytes": 1048576
        },
        "message": "",
        "seccomp_profile": ""
    }

    result = make_request("/submit", "POST", data, {"Content-Type": "application/json"})
    if result:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    print()

def test_wrong_answer():
    """测试Wrong Answer"""
    print_section("6. 测试Wrong Answer...")

    data = {
        "submission_id": 5,
        "language": "cpp",
        "code": """#include <iostream>
int main() {
    std::cout << "wrong" << std::endl;
    return 0;
}""",
        "test_cases": [
            {
                "case_id": 1,
                "stdin": "",
                "expected": "correct"
            }
        ],
        "resources_limits": {
            "cpu_time": 1000,
            "memory_bytes": 268435456,
            "stack_bytes": 8388608,
            "output_bytes": 1048576
        },
        "message": "",
        "seccomp_profile": ""
    }

    result = make_request("/submit", "POST", data, {"Content-Type": "application/json"})
    if result:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    print()

def main():
    """主函数"""
    print_section("Code Runner 测试脚本")

    # 测试服务器是否可用
    print("正在检查服务器连接...")
    try:
        response = requests.get(f"{SERVER_URL}/health", timeout=5)
        if response.status_code == 200:
            print("✓ 服务器连接正常")
        else:
            print(f"⚠ 服务器返回状态码: {response.status_code}")
    except requests.exceptions.RequestException as e:
        print(f"✗ 无法连接到服务器: {e}")
        print("请确保 Code Runner 服务正在运行在 localhost:8080")
        return

    print()

    # 执行所有测试
    test_health_check()
    test_cpp_code()
    test_python_code()
    test_c_code()
    test_compile_error()
    test_wrong_answer()

    print("=" * 50)
    print("测试完成！")
    print("=" * 50)

if __name__ == "__main__":
    main()
