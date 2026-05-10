#!/usr/bin/env bash
# 构建 vdb_server 镜像；可在任意目录执行（脚本会定位到 4.3.2 根目录作为构建上下文）
# 默认基础镜像见 Dockerfile（DaoCloud）；若需 Docker Hub 官方 ubuntu:22.04:
#   UBUNTU_IMAGE=ubuntu:22.04 ./docker/build.sh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
IMAGE_NAME="${IMAGE_NAME:-vdb-4.3.2:latest}"

BUILD_ARGS=()
if [[ -n "${UBUNTU_IMAGE:-}" ]]; then
    BUILD_ARGS+=(--build-arg "UBUNTU_IMAGE=${UBUNTU_IMAGE}")
fi

cd "${ROOT}"
docker build -f docker/Dockerfile -t "${IMAGE_NAME}" "${BUILD_ARGS[@]}" .
echo "构建完成: ${IMAGE_NAME}"
