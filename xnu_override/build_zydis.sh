#!/bin/sh -e

if [ -z "$PROJECT_DIR" ]; then
    PROJECT_DIR="$(cd "$(dirname "$0")/.."; pwd)"
fi

if [ -z "${TARGET_BUILD_DIR}" ]; then
    TARGET_BUILD_DIR="$(mktemp -d)"
fi


ZYDIS_DIR="${PROJECT_DIR}/xnu_override/zydis"
PRODUCT_DIR="${TARGET_BUILD_DIR}/zydis"

GENERATOR="Unix Makefiles"
if which -s ninja; then
    GENERATOR="Ninja"
fi


case "$1" in
    clean)
        rm -Rf "${PRODUCT_DIR}"
        ;;
    ""|build)
        if [ ! -d "${ZYDIS_DIR}" ]; then
            git submodule update --init --recursive
        fi
        if [ ! -f "${PRODUCT_DIR}/CMakeCache.txt" ]; then
            mkdir -p "${PRODUCT_DIR}"
            pushd "${PRODUCT_DIR}"
            cmake -G "${GENERATOR}" "${ZYDIS_DIR}"
            popd
        fi
        cmake --build "${PRODUCT_DIR}"
        ;;
    *)
        echo "error: unknown subcommand $1" 1>&2
        exit 1
        ;;
esac
