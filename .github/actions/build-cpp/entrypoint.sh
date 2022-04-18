#!/bin/bash
#
function validateParams() {
  echo "========================= Checking parameters ========================="
  [[ -z $INPUT_SYMBOL_URL ]] && echo "Backtrace symbol url is required" && exit 1 || echo "Backtrace symbol url present"
}

function build() {
  echo "========================= Building pktvisor ========================="
  cp -rf /github/workspace/.git/ /pktvisor-src/.git/
  cp -rf /github/workspace/src/ /pktvisor-src/src/
  cp -rf /github/workspace/cmd/ /pktvisor-src/cmd/
  cp -rf /github/workspace/3rd/ /pktvisor-src/3rd/
  cp -rf /github/workspace/docker/ /pktvisor-src/docker/
  cp -rf /github/workspace/golang/ /pktvisor-src/golang/
  cp -rf /github/workspace/integration_tests/ /pktvisor-src/integration_tests/
  cp -rf /github/workspace/cmake/ /pktvisor-src/cmake/
  cp -rf /github/workspace/CMakeLists.txt /pktvisor-src/
  cp -rf /github/workspace/conanfile.txt /pktvisor-src/
  mkdir /tmp/build
  cd /tmp/build
  conan profile new --detect default && \
  conan profile update settings.compiler.libcxx=libstdc++11 default && \
  conan config set general.revisions_enabled=1
  PKG_CONFIG_PATH=/local/lib/pkgconfig cmake -DCMAKE_BUILD_TYPE=Debug -DASAN=ON /pktvisor-src && \
  make all -j 4
}

function compact() {
  echo "========================= Compacting binary and copying ========================="
  cd /tmp/build
  zip pktvisord.zip /tmp/build/bin/pktvisord
  cp -rf /tmp/build/bin/pktvisord /github/workspace/
  cp -rf /tmp/build/bin/crashpad_handler /github/workspace/
  cp -rf /tmp/build/bin/pktvisor-reader /github/workspace/
  #version for pktvisor-cli
  cp -rf /pktvisor-src/golang/pkg/client/version.go /github/workspace/version.go
}

function publish() {
  echo "========================= Publishing symbol to backtrace ========================="
  cd /tmp/build
  curl --data-binary @pktvisord.zip -H "Expect: gzip" "${INPUT_SYMBOL_URL}"
}

validateParams
build
compact
publish