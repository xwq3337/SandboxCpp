FROM ubuntu:24.04

# 设置环境变量
ENV DEBIAN_FRONTEND=noninteractive

# 安装依赖
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libseccomp-dev \
    g++ \
    gcc \
    python3 \
    default-jdk \
    golang \
    git \
    curl \
    jq \
    && rm -rf /var/lib/apt/lists/*

# 创建工作目录
WORKDIR /app

# 复制项目文件
COPY . .

# 创建 build 目录并编译
RUN mkdir -p build && cd build && cmake .. && make -j$(nproc)

# 暴露端口
EXPOSE 8080

# 运行服务器
CMD ["./build/code_runner"]
