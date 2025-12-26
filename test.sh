#!/bin/bash

# 测试脚本 - 用于测试 Code Runner 服务

SERVER_URL="http://localhost:8080"

echo "======================================"
echo "Code Runner 测试脚本"
echo "======================================"
echo ""

# 测试健康检查
echo "1. 测试健康检查..."
curl -s "${SERVER_URL}/health" | jq .
echo ""
echo ""

# 测试 C++ 代码
echo "2. 测试 C++ 代码执行..."
curl -s -X POST "${SERVER_URL}/submit" \
  -H "Content-Type: application/json" \
  -d '{
    "submission_id": 1,
    "language": "cpp",
    "code": "#include <iostream>\nint main() {\n    int a, b;\n    std::cin >> a >> b;\n    std::cout << a + b << std::endl;\n    return 0;\n}",
    "test_cases": [
      {
        "case_id": 1,
        "stdin_data": "1 2",
        "expected": "3"
      },
      {
        "case_id": 2,
        "stdin_data": "5 7",
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
  }' | jq .
echo ""
echo ""

# 测试 Python 代码
echo "3. 测试 Python 代码执行..."
curl -s -X POST "${SERVER_URL}/submit" \
  -H "Content-Type: application/json" \
  -d '{
    "submission_id": 2,
    "language": "python",
    "code": "a, b = map(int, input().split())\nprint(a + b)",
    "test_cases": [
      {
        "case_id": 1,
        "stdin_data": "1 2",
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
  }' | jq .
echo ""
echo ""

# 测试 C 代码
echo "4. 测试 C 代码执行..."
curl -s -X POST "${SERVER_URL}/submit" \
  -H "Content-Type: application/json" \
  -d '{
    "submission_id": 3,
    "language": "c",
    "code": "#include <stdio.h>\nint main() {\n    int a, b;\n    scanf(\"%d %d\", &a, &b);\n    printf(\"%d\\n\", a + b);\n    return 0;\n}",
    "test_cases": [
      {
        "case_id": 1,
        "stdin_data": "3 4",
        "expected": "7"
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
  }' | jq .
echo ""
echo ""

# 测试编译错误
echo "5. 测试编译错误处理..."
curl -s -X POST "${SERVER_URL}/submit" \
  -H "Content-Type: application/json" \
  -d '{
    "submission_id": 4,
    "language": "cpp",
    "code": "#include <iostream>\nint main() {\n    std::cout << \"Missing semicolon\"\n    return 0;\n}",
    "test_cases": [
      {
        "case_id": 1,
        "stdin_data": "",
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
  }' | jq .
echo ""
echo ""

# 测试 Wrong Answer
echo "6. 测试 Wrong Answer..."
curl -s -X POST "${SERVER_URL}/submit" \
  -H "Content-Type: application/json" \
  -d '{
    "submission_id": 5,
    "language": "cpp",
    "code": "#include <iostream>\nint main() {\n    std::cout << \"wrong\" << std::endl;\n    return 0;\n}",
    "test_cases": [
      {
        "case_id": 1,
        "stdin_data": "",
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
  }' | jq .
echo ""
echo ""

echo "======================================"
echo "测试完成！"
echo "======================================"
