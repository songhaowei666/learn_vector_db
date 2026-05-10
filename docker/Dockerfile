# 在 4.3.2 目录下执行: docker build -f docker/Dockerfile -t vdb-4.3.2:latest .
# 默认经 DaoCloud 拉取 ubuntu 22.04，避免直连 registry-1.docker.io（IPv6 等）超时；官方源:
#   docker build -f docker/Dockerfile --build-arg UBUNTU_IMAGE=ubuntu:22.04 -t vdb-4.3.2:latest .
ARG UBUNTU_IMAGE=m.daocloud.io/docker.io/library/ubuntu:22.04
FROM ${UBUNTU_IMAGE}

ENV DEBIAN_FRONTEND=noninteractive

# 与 makefile 注释一致：编译与链接所需开发包
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    g++ \
    make \
    ca-certificates \
    libfaiss-dev \
    libopenblas-openmp-dev \
    libspdlog-dev \
    libfmt-dev \
    librocksdb-dev \
    libroaring-dev \
    rapidjson-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN make clean && make -j"$(nproc)"

EXPOSE 9090

CMD ["./vdb_server"]
