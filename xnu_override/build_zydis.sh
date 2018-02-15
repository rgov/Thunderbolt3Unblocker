#!/bin/sh -e

if [ -z "$PROJECT_DIR" ]; then
    PROJECT_DIR="$(cd "$(dirname "$0")/.."; pwd)"
fi

if [ -z "$BUILD_DIR" ]; then
    BUILD_DIR="$(mktemp -d)"
fi


ZYDIS_DIR="${PROJECT_DIR}/xnu_override/zydis"
PRODUCT_DIR="${TARGET_BUILD_DIR}/zydis"


case "$1" in
    clean)
        rm -Rf "${PRODUCT_DIR}"
        ;;
    ""|build)
        if [ ! -d "${PRODUCT_DIR}" ]; then
            mkdir -p "${PRODUCT_DIR}"
            pushd "${PRODUCT_DIR}"
            cmake -G Ninja "${ZYDIS_DIR}"
            popd
        fi
        cmake --build "${PRODUCT_DIR}"
        ;;
    *)
        echo "error: unknown subcommand $1" 1>&2
        exit 1
        ;;
esac
