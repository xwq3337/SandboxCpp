#include "code_executor.h"
#include "seccomp_filter.h"
#include "namespace_isolation.h"
#include "cgroup_manager.h"
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sched.h>
#include <chrono>
#include <algorithm>

CodeExecutor::CodeExecutor() {
    // 构造函数
}

void CodeExecutor::loadLanguageConfigs(const std::string& configPath) {
    std::ifstream file(configPath);
    if (!file.is_open()) {
        fprintf(stderr, "Failed to open language config file: %s\n", configPath.c_str());
        return;
    }

    try {
        json j;
        file >> j;

        for (auto& [lang, config] : j.items()) {
            LanguageConfig lc = config.get<LanguageConfig>();
            languageConfigs_[lang] = lc;
        }
    } catch (const std::exception& e) {
        fprintf(stderr, "Failed to parse language config: %s\n", e.what());
    }
}

OutputResult CodeExecutor::execute(const InputStruct& input) {
    OutputResult output;
    output.submission_id = input.submission_id;
    output.language = input.language;
    output.code = input.code;
    output.verdict = Verdict::SystemError;
    output.max_time = 0;
    output.max_memory = 0;

    // 编译代码
    std::string compiledPath;
    std::string compileError;
    std::string workDir;
    Verdict compileResult = compile(input.language, input.code, compiledPath, compileError, workDir);

    if (compileResult != Verdict::Accepted) {
        output.verdict = compileResult;
        // 添加编译错误信息
        TestCaseResult errorResult;
        errorResult.case_id = "compile";
        errorResult.stderr = compileError;
        errorResult.status = compileResult;
        output.result.push_back(errorResult);
        if (!workDir.empty()) {
            cleanupWorkDir(workDir);
        }
        return output;
    }

    // 运行测试用例
    std::vector<std::string> allowedSyscalls;
    if (!input.seccomp_profile.empty() && languageConfigs_.count(input.language)) {
        allowedSyscalls = languageConfigs_[input.language].allow_sys_calls;
    } else {
        allowedSyscalls = SeccompFilter::getDefaultAllowedSyscalls();
    }

    output.verdict = Verdict::Accepted;

    for (const auto& testCase : input.test_cases) {
        TestCaseResult result = runTestCase(input.language, compiledPath,
                                            testCase, input.resources_limits,
                                            allowedSyscalls);

        output.result.push_back(result);
        output.max_time = std::max(output.max_time, result.time);
        output.max_memory = std::max(output.max_memory, result.memory);

        // 如果有任何测试用例失败，更新总体判定
        if (result.status != Verdict::Accepted) {
            output.verdict = result.status;
        }
    }

    // 清理工作目录
    if (!workDir.empty()) {
        cleanupWorkDir(workDir);
    }
    if (!compiledPath.empty()) {
        unlink(compiledPath.c_str());
    }

    return output;
}

Verdict CodeExecutor::compile(const std::string& language, const std::string& code,
                              std::string& compiledPath, std::string& compileError, std::string& workDir) {
    if (languageConfigs_.find(language) == languageConfigs_.end()) {
        compileError = "Unsupported language: " + language;
        return Verdict::SystemError;
    }

    const auto& config = languageConfigs_[language];

    // 创建临时文件
    workDir = createWorkDir();
    std::string sourceFile = workDir + "/main." + language;

    // 写入源代码
    std::ofstream srcFile(sourceFile);
    if (!srcFile.is_open()) {
        compileError = "Failed to create source file";
        return Verdict::SystemError;
    }
    srcFile << code;
    srcFile.close();

    // 如果不需要编译（如 Python）
    if (config.compile_cmd.empty()) {
        compiledPath = sourceFile;  // 对于解释型语言，返回源文件路径
        return Verdict::Accepted;
    }

    std::string outputFile = workDir + "/main";

    // 构建编译命令
    std::string compileCmd = config.compile_cmd;

    // 替换占位符
    size_t pos;
    while ((pos = compileCmd.find("{source}")) != std::string::npos) {
        compileCmd.replace(pos, 8, sourceFile);
    }
    while ((pos = compileCmd.find("{output}")) != std::string::npos) {
        compileCmd.replace(pos, 8, outputFile);
    }

    // 重定向编译错误输出
    std::string errorFile = workDir + "/compile_error.txt";
    compileCmd += " 2> " + errorFile;

    // 执行编译
    int ret = system(compileCmd.c_str());

    if (ret != 0) {
        // 读取编译错误
        std::ifstream errFile(errorFile);
        if (errFile.is_open()) {
            std::stringstream buffer;
            buffer << errFile.rdbuf();
            compileError = buffer.str();
        }
        return Verdict::CompilationError;
    }

    compiledPath = outputFile;
    return Verdict::Accepted;
}

TestCaseResult CodeExecutor::runTestCase(const std::string& language,
                                         const std::string& execPath,
                                         const TestCase& testCase,
                                         const ResourcesLimits& limits,
                                         const std::vector<std::string>& allowedSyscalls) {
    // 检查是否需要使用解释器
    if (languageConfigs_.count(language) && languageConfigs_[language].compile_cmd.empty()) {
        // 解释型语言（如 Python）
        return executeInterpreted(language, execPath, testCase, limits, allowedSyscalls);
    }
    // 编译型语言
    return executeInSandbox(execPath, testCase, limits, allowedSyscalls);
}

TestCaseResult CodeExecutor::executeInSandbox(const std::string& execPath,
                                              const TestCase& testCase,
                                              const ResourcesLimits& limits,
                                              const std::vector<std::string>& allowedSyscalls) {
    TestCaseResult result;
    result.case_id = std::to_string(testCase.case_id);
    result.stdin = testCase.stdin;
    result.expected = testCase.expected;
    result.status = Verdict::SystemError;
    result.time = 0;
    result.memory = 0;

    // 创建管道用于输入输出
    int stdin_pipe[2], stdout_pipe[2], stderr_pipe[2];
    if (pipe(stdin_pipe) < 0 || pipe(stdout_pipe) < 0 || pipe(stderr_pipe) < 0) {
        result.stderr = "Failed to create pipes";
        return result;
    }

    // 创建 cgroup
    std::string cgroupName = "code_runner_" + std::to_string(getpid()) + "_" + result.case_id;
    CgroupManager cgroup(cgroupName);
    cgroup.setMemoryLimit(limits.memory_bytes);
    cgroup.setPidsLimit(10);

    auto startTime = std::chrono::high_resolution_clock::now();

    pid_t pid = fork();

    if (pid == 0) {
        // 子进程

        // 将子进程加入 cgroup
        cgroup.addProcess(getpid());

        // 设置资源限制
        struct rlimit rlim;

        // CPU 时间限制
        rlim.rlim_cur = rlim.rlim_max = (limits.cpu_time + 999) / 1000; // 转换为秒
        setrlimit(RLIMIT_CPU, &rlim);

        // 栈大小限制
        rlim.rlim_cur = rlim.rlim_max = limits.stack_bytes;
        setrlimit(RLIMIT_STACK, &rlim);

        // 文件大小限制
        rlim.rlim_cur = rlim.rlim_max = limits.output_bytes;
        setrlimit(RLIMIT_FSIZE, &rlim);

        // 重定向标准输入输出
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);

        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);

        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        // 安装 seccomp 过滤器（暂时禁用以调试）
        // if (SeccompFilter::installFilter(allowedSyscalls) != 0) {
        //     fprintf(stderr, "Failed to install seccomp filter\n");
        //     exit(1);
        // }

        // 执行程序
        execl(execPath.c_str(), execPath.c_str(), nullptr);

        // 如果 execl 返回，说明执行失败
        fprintf(stderr, "Failed to execute program\n");
        exit(1);
    } else if (pid > 0) {
        // 父进程
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        // 写入输入数据
        write(stdin_pipe[1], testCase.stdin.c_str(), testCase.stdin.length());
        close(stdin_pipe[1]);

        // 读取输出
        char buffer[4096];
        ssize_t n;

        while ((n = read(stdout_pipe[0], buffer, sizeof(buffer))) > 0) {
            result.stdout.append(buffer, n);
            if (result.stdout.length() > (size_t)limits.output_bytes) {
                kill(pid, SIGKILL);
                result.status = Verdict::RuntimeError;
                result.stderr = "Output limit exceeded";
                break;
            }
        }
        close(stdout_pipe[0]);

        // 读取错误输出
        while ((n = read(stderr_pipe[0], buffer, sizeof(buffer))) > 0) {
            result.stderr.append(buffer, n);
        }
        close(stderr_pipe[0]);

        // 等待子进程结束
        int status;
        struct rusage usage;
        wait4(pid, &status, 0, &usage);

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        result.time = duration.count();
        result.memory = cgroup.getMemoryUsage();

        // 判断执行结果
        if (WIFEXITED(status)) {
            int exitCode = WEXITSTATUS(status);
            if (exitCode == 0) {
                // 比较输出
                if (compareOutput(result.stdout, testCase.expected)) {
                    result.status = Verdict::Accepted;
                } else {
                    result.status = Verdict::WrongAnswer;
                }
            } else {
                result.status = Verdict::RuntimeError;
            }
        } else if (WIFSIGNALED(status)) {
            int sig = WTERMSIG(status);
            if (sig == SIGXCPU || result.time > limits.cpu_time) {
                result.status = Verdict::TimeLimitExceeded;
            } else if (sig == SIGSYS) {
                result.status = Verdict::RestrictedSystemCall;
            } else if (sig == SIGKILL && result.memory > (size_t)limits.memory_bytes) {
                result.status = Verdict::MemoryLimitExceeded;
            } else {
                result.status = Verdict::RuntimeError;
            }
        }

    } else {
        // fork 失败
        result.stderr = "Failed to fork process";
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
    }

    return result;
}

TestCaseResult CodeExecutor::executeInterpreted(const std::string& language,
                                                const std::string& sourceFile,
                                                const TestCase& testCase,
                                                const ResourcesLimits& limits,
                                                const std::vector<std::string>& allowedSyscalls) {
    TestCaseResult result;
    result.case_id = std::to_string(testCase.case_id);
    result.stdin = testCase.stdin;
    result.expected = testCase.expected;
    result.status = Verdict::SystemError;
    result.time = 0;
    result.memory = 0;

    // 创建管道用于输入输出
    int stdin_pipe[2], stdout_pipe[2], stderr_pipe[2];
    if (pipe(stdin_pipe) < 0 || pipe(stdout_pipe) < 0 || pipe(stderr_pipe) < 0) {
        result.stderr = "Failed to create pipes";
        return result;
    }

    // 创建 cgroup
    std::string cgroupName = "code_runner_" + std::to_string(getpid()) + "_" + result.case_id;
    CgroupManager cgroup(cgroupName);
    cgroup.setMemoryLimit(limits.memory_bytes);
    cgroup.setPidsLimit(10);

    auto startTime = std::chrono::high_resolution_clock::now();

    pid_t pid = fork();

    if (pid == 0) {
        // 子进程

        // 将子进程加入 cgroup
        cgroup.addProcess(getpid());

        // 设置资源限制
        struct rlimit rlim;

        // CPU 时间限制
        rlim.rlim_cur = rlim.rlim_max = (limits.cpu_time + 999) / 1000;
        setrlimit(RLIMIT_CPU, &rlim);

        // 栈大小限制
        rlim.rlim_cur = rlim.rlim_max = limits.stack_bytes;
        setrlimit(RLIMIT_STACK, &rlim);

        // 文件大小限制
        rlim.rlim_cur = rlim.rlim_max = limits.output_bytes;
        setrlimit(RLIMIT_FSIZE, &rlim);

        // 重定向标准输入输出
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);

        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);

        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        // 安装 seccomp 过滤器（暂时禁用以调试）
        // if (SeccompFilter::installFilter(allowedSyscalls) != 0) {
        //     fprintf(stderr, "Failed to install seccomp filter\n");
        //     exit(1);
        // }

        // 执行解释器（如 Python）
        if (language == "python") {
            execl("/usr/bin/python3", "python3", sourceFile.c_str(), nullptr);
        } else if (language == "java") {
            // Java 需要特殊处理
            execl("/usr/bin/java", "java", "-cp", sourceFile.c_str(), "Main", nullptr);
        }

        // 如果 execl 返回，说明执行失败
        fprintf(stderr, "Failed to execute interpreter\n");
        exit(1);
    } else if (pid > 0) {
        // 父进程（后续的处理逻辑与 executeInSandbox 相同）
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        write(stdin_pipe[1], testCase.stdin.c_str(), testCase.stdin.length());
        close(stdin_pipe[1]);

        char buffer[4096];
        ssize_t n;

        while ((n = read(stdout_pipe[0], buffer, sizeof(buffer))) > 0) {
            result.stdout.append(buffer, n);
            if (result.stdout.length() > (size_t)limits.output_bytes) {
                kill(pid, SIGKILL);
                result.status = Verdict::RuntimeError;
                result.stderr = "Output limit exceeded";
                break;
            }
        }
        close(stdout_pipe[0]);

        while ((n = read(stderr_pipe[0], buffer, sizeof(buffer))) > 0) {
            result.stderr.append(buffer, n);
        }
        close(stderr_pipe[0]);

        int status;
        struct rusage usage;
        wait4(pid, &status, 0, &usage);

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        result.time = duration.count();
        result.memory = cgroup.getMemoryUsage();

        if (WIFEXITED(status)) {
            int exitCode = WEXITSTATUS(status);
            if (exitCode == 0) {
                if (compareOutput(result.stdout, testCase.expected)) {
                    result.status = Verdict::Accepted;
                } else {
                    result.status = Verdict::WrongAnswer;
                }
            } else {
                result.status = Verdict::RuntimeError;
            }
        } else if (WIFSIGNALED(status)) {
            int sig = WTERMSIG(status);
            if (sig == SIGXCPU || result.time > limits.cpu_time) {
                result.status = Verdict::TimeLimitExceeded;
            } else if (sig == SIGSYS) {
                result.status = Verdict::RestrictedSystemCall;
            } else if (sig == SIGKILL && result.memory > (size_t)limits.memory_bytes) {
                result.status = Verdict::MemoryLimitExceeded;
            } else {
                result.status = Verdict::RuntimeError;
            }
        }

    } else {
        result.stderr = "Failed to fork process";
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
    }

    return result;
}

bool CodeExecutor::compareOutput(const std::string& output, const std::string& expected) {
    // 去除尾部空白字符
    auto trim = [](std::string s) {
        s.erase(s.find_last_not_of(" \n\r\t") + 1);
        return s;
    };

    return trim(output) == trim(expected);
}

std::string CodeExecutor::createWorkDir() {
    char tmpTemplate[] = "/tmp/code_runner_XXXXXX";
    char* tmpDir = mkdtemp(tmpTemplate);
    if (tmpDir == nullptr) {
        return "";
    }
    return std::string(tmpDir);
}

void CodeExecutor::cleanupWorkDir(const std::string& workDir) {
    std::string cmd = "rm -rf " + workDir;
    system(cmd.c_str());
}
