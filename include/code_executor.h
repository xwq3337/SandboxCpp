#pragma once

#include "data_structures.h"
#include "compile_cache.h"
#include <string>
#include <map>

class CodeExecutor {
public:
    CodeExecutor();

    // 加载语言配置
    void loadLanguageConfigs(const std::string& configPath);

    // 执行代码
    OutputResult execute(const InputStruct& input);

private:
    std::map<std::string, LanguageConfig> languageConfigs_;
    CompileCache compileCache_;
    
    // 编译代码
    Verdict compile(const std::string& language, const std::string& code, 
                    std::string& compiledPath, std::string& compileError, std::string& workDir);
    
    // 运行单个测试用例
    TestCaseResult runTestCase(const std::string& language, const std::string& execPath,
                               const std::string& workDir,
                               const TestCase& testCase, const ResourcesLimits& limits,
                               const std::vector<std::string>& allowedSyscalls);

    // 在沙箱中执行
    TestCaseResult executeInSandbox(const std::string& language, const std::string& execPath,
                                    const std::string& workDir,
                                    const TestCase& testCase,
                                    const ResourcesLimits& limits,
                                    const std::vector<std::string>& allowedSyscalls);
    
    // 执行解释型语言
    TestCaseResult executeInterpreted(const std::string& language, const std::string& sourceFile,
                                      const TestCase& testCase, const ResourcesLimits& limits,
                                      const std::vector<std::string>& allowedSyscalls);

    // 执行 Java 程序
    TestCaseResult executeJava(const std::string& workDir, const TestCase& testCase,
                              const ResourcesLimits& limits,
                              const std::vector<std::string>& allowedSyscalls);

    // 执行 C# (Mono) 程序
    TestCaseResult executeCSharp(const std::string& execPath, const TestCase& testCase,
                                 const ResourcesLimits& limits,
                                 const std::vector<std::string>& allowedSyscalls);

    // 比较输出
    bool compareOutput(const std::string& output, const std::string& expected);
    
    // 创建临时工作目录
    std::string createWorkDir();
    
    // 清理工作目录
    void cleanupWorkDir(const std::string& workDir);
};
