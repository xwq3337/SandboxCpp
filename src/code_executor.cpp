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
#include <sys/stat.h>
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
                                            workDir, testCase, input.resources_limits,
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
    std::string sourceFile = workDir + "/" + config.source_file;

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

    // 执行编译 - 需要设置完整的环境变量,特别是 PATH
    // 因为 sudo 会重置 PATH,导致 go 和 rustc 找不到
    int ret = 0;

    // 使用 fork + exec 来设置环境变量
    pid_t compilePid = fork();
    if (compilePid == 0) {
        // 子进程 - 设置环境变量后执行编译命令
        // 使用沙箱中的编译器路径
        const char* newPath = "/opt/sandbox/usr/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/opt/sandbox/usr/local/go/bin:/opt/sandbox/usr/local/node/bin:/opt/sandbox/usr/local/zig:/opt/sandbox/usr/local/cargo/bin:/snap/bin:/home/ubuntu/.cargo/bin:/home/ubuntu/.zig";
        setenv("PATH", newPath, 1);

        // 设置 LD_LIBRARY_PATH 以使用沙箱中的库
        const char* newLibPath = "/opt/sandbox/usr/lib:/opt/sandbox/lib:/opt/sandbox/lib64:/opt/sandbox/usr/local/lib";
        setenv("LD_LIBRARY_PATH", newLibPath, 1);

        // 设置 HOME 环境变量
        // go/zig/rustc/java/mono 等工具都依赖 HOME 来定位缓存目录
        const char* currentHome = getenv("HOME");
        if (!currentHome || strcmp(currentHome, "/root") == 0) {
            setenv("HOME", "/home/ubuntu", 1);
        }

        // 设置 Rust 环境变量，指向沙箱中的 rustup
        setenv("RUSTUP_HOME", "/opt/sandbox/usr/local/rustup", 1);
        setenv("CARGO_HOME", "/opt/sandbox/usr/local/cargo", 1);

        // Go 构建缓存目录
        setenv("GOCACHE", "/tmp/go-build-cache", 1);

        // Zig 全局缓存目录
        setenv("ZIG_GLOBAL_CACHE_DIR", "/tmp/zig-cache", 1);

        // XDG 标准缓存目录（通用 fallback）
        setenv("XDG_CACHE_HOME", "/tmp/xdg-cache", 1);

        // 确保缓存目录存在
        mkdir("/tmp/go-build-cache", 0755);
        mkdir("/tmp/zig-cache", 0755);
        mkdir("/tmp/xdg-cache", 0755);

        // 切换到工作目录，确保 CWD 有效
        // go/rustc/java 等工具启动时会调用 getcwd()，如果 CWD 不存在会直接报错
        chdir(workDir.c_str());

        // 执行编译命令
        ret = system(compileCmd.c_str());
        exit(ret != 0 ? 1 : 0);
    } else if (compilePid > 0) {
        // 父进程 - 等待编译完成
        int status;
        waitpid(compilePid, &status, 0);
        ret = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    } else {
        // fork 失败
        compileError = "Failed to fork for compilation";
        return Verdict::SystemError;
    }

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

    // 对于 Java，compiledPath 保存工作目录，因为 java 需要从 .class 文件所在目录运行
    if (language == "java") {
        compiledPath = workDir;  // 返回工作目录，后续执行时使用 java Main
    } else {
        compiledPath = outputFile;
    }
    return Verdict::Accepted;
}

TestCaseResult CodeExecutor::runTestCase(const std::string& language,
                                         const std::string& execPath,
                                         const std::string& workDir,
                                         const TestCase& testCase,
                                         const ResourcesLimits& limits,
                                         const std::vector<std::string>& allowedSyscalls) {

    // Java 需要特殊处理：运行 java Main，而不是执行编译好的二进制文件
    if (language == "java") {
        return executeJava(execPath, testCase, limits, allowedSyscalls);
    }

    // C# 需要特殊处理：用 mono 运行编译后的 .exe
    if (language == "csharp") {
        return executeCSharp(execPath, testCase, limits, allowedSyscalls);
    }

    // 检查是否需要使用解释器
    if (languageConfigs_.count(language) && languageConfigs_[language].compile_cmd.empty()) {
        // 解释型语言（如 Python）
        return executeInterpreted(language, execPath, testCase, limits, allowedSyscalls);
    }
    // 编译型语言
    return executeInSandbox(execPath, workDir, testCase, limits, allowedSyscalls);
}
TestCaseResult CodeExecutor::executeInSandbox(const std::string& execPath,
                                              const std::string& workDir,
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

        // 切换到可执行文件所在目录，确保 CWD 有效
        chdir(workDir.c_str());

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

        // 执行解释器 - 使用沙箱中的解释器
        if (language == "python") {
            execl("/opt/sandbox/usr/bin/python3", "python3", sourceFile.c_str(), nullptr);
        } else if (language == "javascript") {
            execl("/opt/sandbox/usr/bin/node", "node", sourceFile.c_str(), nullptr);
        } else if (language == "pypy") {
            execl("/opt/sandbox/usr/bin/pypy3", "pypy3", sourceFile.c_str(), nullptr);
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

TestCaseResult CodeExecutor::executeJava(const std::string& workDir,
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
    cgroup.setPidsLimit(100);  // Java 需要更多线程用于 GC、编译等

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

        // 切换到 .class 文件所在目录
        chdir(workDir.c_str());

        // 执行 java Main - 使用沙箱中的 java
        execl("/opt/sandbox/usr/bin/java", "java", "Main", nullptr);

        // 如果 execlp 返回，说明执行失败
        fprintf(stderr, "Failed to execute java\n");
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

TestCaseResult CodeExecutor::executeCSharp(const std::string& execPath,
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

    int stdin_pipe[2], stdout_pipe[2], stderr_pipe[2];
    if (pipe(stdin_pipe) < 0 || pipe(stdout_pipe) < 0 || pipe(stderr_pipe) < 0) {
        result.stderr = "Failed to create pipes";
        return result;
    }

    std::string cgroupName = "code_runner_" + std::to_string(getpid()) + "_" + result.case_id;
    CgroupManager cgroup(cgroupName);
    cgroup.setMemoryLimit(limits.memory_bytes);
    cgroup.setPidsLimit(50);

    auto startTime = std::chrono::high_resolution_clock::now();

    pid_t pid = fork();

    if (pid == 0) {
        cgroup.addProcess(getpid());

        struct rlimit rlim;
        rlim.rlim_cur = rlim.rlim_max = (limits.cpu_time + 999) / 1000;
        setrlimit(RLIMIT_CPU, &rlim);
        rlim.rlim_cur = rlim.rlim_max = limits.stack_bytes;
        setrlimit(RLIMIT_STACK, &rlim);
        rlim.rlim_cur = rlim.rlim_max = limits.output_bytes;
        setrlimit(RLIMIT_FSIZE, &rlim);

        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        const char* newPath = "/opt/sandbox/usr/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin";
        setenv("PATH", newPath, 1);
        setenv("HOME", "/home/ubuntu", 1);
        setenv("MONO_ENV_OPTIONS", "", 1);

        execl("/opt/sandbox/usr/bin/mono", "mono", execPath.c_str(), nullptr);

        fprintf(stderr, "Failed to execute mono\n");
        exit(1);
    } else if (pid > 0) {
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
        wait4(pid, &status, 0, nullptr);

        auto endTime = std::chrono::high_resolution_clock::now();
        result.time = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
        result.memory = cgroup.getMemoryUsage();

        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) == 0) {
                result.status = compareOutput(result.stdout, testCase.expected)
                    ? Verdict::Accepted : Verdict::WrongAnswer;
            } else {
                result.status = Verdict::RuntimeError;
            }
        } else if (WIFSIGNALED(status)) {
            int sig = WTERMSIG(status);
            if (sig == SIGXCPU || result.time > limits.cpu_time) {
                result.status = Verdict::TimeLimitExceeded;
            } else if (sig == SIGKILL && result.memory > (size_t)limits.memory_bytes) {
                result.status = Verdict::MemoryLimitExceeded;
            } else {
                result.status = Verdict::RuntimeError;
            }
        }
    } else {
        result.stderr = "Failed to fork process";
        close(stdin_pipe[0]); close(stdin_pipe[1]);
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        close(stderr_pipe[0]); close(stderr_pipe[1]);
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
