#!/usr/bin/env bash
# 启动容器：映射 9090，持久化 ScalarStorage、WAL、快照目录与快照元数据文件
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
IMAGE_NAME="${IMAGE_NAME:-vdb-4.3.2:latest}"
# 默认数据放在 4.3.2/docker-data，可通过环境变量 DATA_DIR 覆盖
DATA_DIR="${DATA_DIR:-${ROOT}/docker-data}"

mkdir -p "${DATA_DIR}/scalar" "${DATA_DIR}/snapshots_"
# WAL 与普通文件类元数据需预先存在再 bind mount
if [[ ! -f "${DATA_DIR}/WALStorage" ]]; then
    touch "${DATA_DIR}/WALStorage"
fi
if [[ ! -f "${DATA_DIR}/snapshots_MaxLogID" ]]; then
    touch "${DATA_DIR}/snapshots_MaxLogID"
fi

docker run --rm -it \
    -p "${PORT:-9090}:9090" \
    -v "${DATA_DIR}/scalar:/app/ScalarStorage" \
    -v "${DATA_DIR}/WALStorage:/app/WALStorage" \
    -v "${DATA_DIR}/snapshots_:/app/snapshots_" \
    -v "${DATA_DIR}/snapshots_MaxLogID:/app/snapshots_MaxLogID" \
    -w /app \
    "${IMAGE_NAME}"
