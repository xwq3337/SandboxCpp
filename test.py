import requests
import json

BASE_URL = "http://localhost:8080"

def test_language(name, language, code, test_input, expected):
    """测试单个语言"""
    data = {
        'submission_id': 1,
        'language': language,
        'code': code,
        'test_cases': [{
            'case_id': 1,
            'stdin': test_input,
            'expected': expected
        }],
        'resources_limits': {
            'cpu_time': 2000,
            'memory_bytes': 268435456,
            'stack_bytes': 8388608,
            'output_bytes': 1048576
        },
        'message': '',
        'seccomp_profile': ''
    }
    
    try:
        response = requests.post(f'{BASE_URL}/submit', json=data, timeout=20)
        result = response.json()
        
        status = result['result'][0]['status']
        if status == 'Accepted':
            print(f"✓ {name:15} ({language:10}) - Passed")
            return True
        else:
            print(f"✗ {name:15} ({language:10}) - Failed ({status}) {result['result'][0]['stderr']}")
            return False
    except Exception as e:
        print(f"✗ {name:15} ({language:10}) - Error: {str(e)[:40]}")
        return False

# 测试用例
tests = [
    # 现有语言
    ("C++", "cpp", '''
#include <iostream>
using namespace std;
int main() {
    int a, b;
    cin >> a >> b;
    cout << a + b << endl;
    return 0;
}
''', "3 5", "8"),

    ("C", "c", '''
#include <stdio.h>
int main() {
    int a, b;
    scanf("%d %d", &a, &b);
    printf("%d\\n", a + b);
    return 0;
}
''', "7 2", "9"),

    ("Python", "python", '''
a, b = map(int, input().split())
print(a + b)
''', "4 6", "10"),

    ("Java", "java", '''
import java.util.Scanner;

public class Main {
    public static void main(String[] args) {
        Scanner sc = new Scanner(System.in);
        int a = sc.nextInt();
        int b = sc.nextInt();
        System.out.println(a + b);
    }
}
''', "5 3", "8"),

    ("Go", "go", '''
package main

import "fmt"

func main() {
    var a, b int
    fmt.Scan(&a, &b)
    fmt.Println(a + b)
}
''', "9 7", "16"),

    ("Rust", "rust", '''
use std::io;

fn main() {
    let mut input = String::new();
    io::stdin().read_line(&mut input).unwrap();
    let nums: Vec<i32> = input
        .split_whitespace()
        .map(|x| x.parse().unwrap())
        .collect();
    println!("{}", nums[0] + nums[1]);
}
''', "11 9", "20"),

    # 新语言
    ("JavaScript", "javascript", '''
const readline = require('readline');
const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout
});

rl.on('line', (line) => {
    const [a, b] = line.split(' ').map(Number);
    console.log(a + b);
    rl.close();
});
''', "6 4", "10"),

    # 新增语言
    ("C#", "csharp", '''
using System;

class Program {
    static void Main() {
        string[] input = Console.ReadLine().Split();
        int a = int.Parse(input[0]);
        int b = int.Parse(input[1]);
        Console.WriteLine(a + b);
    }
}
''', "8 2", "10"),

    ("Zig", "zig", '''
const std = @import("std");

pub fn main() !void {
    _ = std.c.write(1, "8\\n", 2);
}
''', "", "8"),

    ("PyPy", "pypy", '''
a, b = map(int, input().split())
print(a + b)
''', "3 8", "11"),
]

print("=" * 80)
print("Code Runner - 全部语言测试")
print("=" * 80)
print()

passed = 0
total = len(tests)

for name, lang, code, input_val, expected in tests:
    if test_language(name, lang, code, input_val, expected):
        passed += 1

print()
print("=" * 80)
print(f"测试结果: {passed}/{total} 通过")
if passed == total:
    print("🎉 所有测试通过!")
print("=" * 80)
