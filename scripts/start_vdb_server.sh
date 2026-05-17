#!/usr/bin/env bash
# vdb_server 启动/停止脚本
# 用法:
#   ./scripts/start_vdb_server.sh              # 后台启动（默认 conf.ini）
#   ./scripts/start_vdb_server.sh -f           # 前台启动
#   ./scripts/start_vdb_server.sh /path/conf.ini
#   ./scripts/start_vdb_server.sh stop|status|restart

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BINARY="${PROJECT_ROOT}/vdb_server"
CONFIG="${PROJECT_ROOT}/conf.ini"
RUN_DIR="${PROJECT_ROOT}/run"
LOG_DIR="${PROJECT_ROOT}/logs"
PID_FILE="${RUN_DIR}/vdb_server.pid"
LOG_FILE="${LOG_DIR}/vdb_server.log"

FOREGROUND=0
CMD="start"

usage() {
    cat <<EOF
用法: $(basename "$0") [选项|命令] [配置文件]

命令:
  start     后台启动（默认）
  stop      停止进程
  status    查看运行状态
  restart   先 stop 再 start

选项:
  -f        前台运行（不写入 PID 文件）

示例:
  $(basename "$0")
  $(basename "$0") -f
  $(basename "$0") ${PROJECT_ROOT}/conf.ini
  $(basename "$0") stop
EOF
}

parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            start|stop|status|restart)
                CMD="$1"
                shift
                ;;
            -f|--foreground)
                FOREGROUND=1
                shift
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            -*)
                echo "未知选项: $1" >&2
                usage >&2
                exit 1
                ;;
            *)
                CONFIG="$1"
                shift
                ;;
        esac
    done
}

ensure_paths() {
    if [[ ! -x "${BINARY}" ]]; then
        echo "未找到可执行文件: ${BINARY}" >&2
        echo "请先在项目根目录执行: make clean && make" >&2
        exit 1
    fi
    if [[ ! -f "${CONFIG}" ]]; then
        echo "未找到配置文件: ${CONFIG}" >&2
        exit 1
    fi
    mkdir -p "${RUN_DIR}" "${LOG_DIR}"

    local db_path wal_path
    db_path="$(grep -E '^db_path=' "${CONFIG}" | head -1 | cut -d= -f2- | tr -d '\r')"
    wal_path="$(grep -E '^wal_path=' "${CONFIG}" | head -1 | cut -d= -f2- | tr -d '\r')"
    if [[ -n "${db_path}" && "${db_path}" != /* ]]; then
        mkdir -p "${PROJECT_ROOT}/${db_path}"
    fi
    if [[ -n "${wal_path}" && "${wal_path}" != /* ]]; then
        mkdir -p "${PROJECT_ROOT}/${wal_path}"
    fi
}

setup_lib_path() {
    if [[ -d /usr/local/lib ]]; then
        export LD_LIBRARY_PATH="/usr/local/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
    fi
}

is_running() {
    [[ -f "${PID_FILE}" ]] || return 1
    local pid
    pid="$(cat "${PID_FILE}")"
    [[ -n "${pid}" ]] && kill -0 "${pid}" 2>/dev/null
}

do_stop() {
    if ! is_running; then
        echo "vdb_server 未在运行"
        rm -f "${PID_FILE}"
        return 0
    fi
    local pid
    pid="$(cat "${PID_FILE}")"
    echo "停止 vdb_server (pid=${pid}) ..."
    kill "${pid}" 2>/dev/null || true
    for _ in $(seq 1 30); do
        if ! kill -0 "${pid}" 2>/dev/null; then
            rm -f "${PID_FILE}"
            echo "已停止"
            return 0
        fi
        sleep 0.2
    done
    echo "进程未退出，发送 SIGKILL" >&2
    kill -9 "${pid}" 2>/dev/null || true
    rm -f "${PID_FILE}"
    echo "已强制停止"
}

do_status() {
    if is_running; then
        echo "vdb_server 运行中, pid=$(cat "${PID_FILE}")"
        echo "  配置: ${CONFIG}"
        echo "  日志: ${LOG_FILE}"
        return 0
    fi
    echo "vdb_server 未运行"
    return 1
}

do_start() {
    if is_running; then
        echo "vdb_server 已在运行, pid=$(cat "${PID_FILE}")" >&2
        exit 1
    fi

    setup_lib_path
    cd "${PROJECT_ROOT}"

    local http_port
    http_port="$(grep -E '^http_server_port=' "${CONFIG}" | head -1 | cut -d= -f2- | tr -d '\r')"
    echo "启动 vdb_server"
    echo "  工作目录: ${PROJECT_ROOT}"
    echo "  配置:     ${CONFIG}"
    echo "  HTTP 端口: ${http_port:-（见配置）}"

    if [[ "${FOREGROUND}" -eq 1 ]]; then
        exec "${BINARY}" "${CONFIG}"
    fi

    nohup "${BINARY}" "${CONFIG}" >>"${LOG_FILE}" 2>&1 &
    echo $! >"${PID_FILE}"
    sleep 0.3
    if ! is_running; then
        echo "启动失败，请查看日志: ${LOG_FILE}" >&2
        tail -n 20 "${LOG_FILE}" >&2 || true
        rm -f "${PID_FILE}"
        exit 1
    fi
    echo "已后台启动, pid=$(cat "${PID_FILE}")"
    echo "日志: ${LOG_FILE}"
}

parse_args "$@"

case "${CMD}" in
    start)
        ensure_paths
        do_start
        ;;
    stop)
        do_stop
        ;;
    status)
        do_status
        ;;
    restart)
        do_stop || true
        ensure_paths
        do_start
        ;;
    *)
        usage >&2
        exit 1
        ;;
esac
