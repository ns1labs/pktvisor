#!/bin/bash
#
function validateParams() {
  echo "========================= Checking parameters ========================="
  [[ -z $INPUT_BUGSPLAT_SYMBOL_URL ]] && echo "Bugsplat symbol url is required" && exit 1 || echo "Bugsplat symbol url pŕesent"
}

function build() {
  echo "========================= Building pktvisor ========================="
  cp -rf /github/workspace/.git/ /pktvisor-src/.git/
  cp -rf /github/workspace/src/ /pktvisor-src/src/
  cp -rf /github/workspace/cmd/ /pktvisor-src/cmd/
  cp -rf /github/workspace/3rd/ /pktvisor-src/3rd/
  cp -rf /github/workspace/libs/ /pktvisor-src/libs/
  cp -rf /github/workspace/docker/ /pktvisor-src/docker/
  cp -rf /github/workspace/golang/ /pktvisor-src/golang/
  cp -rf /github/workspace/build/ /pktvisor-src/build/
  cp -rf /github/workspace/integration_tests/ /pktvisor-src/integration_tests/
  cp -rf /github/workspace/cmake/ /pktvisor-src/cmake/
  cp -rf /github/workspace/CMakeLists.txt /pktvisor-src/
  cp -rf /github/workspace/conanfile.py /pktvisor-src/
  mkdir /tmp/build
  cd /tmp/build
  cp -rf /pktvisor-src/build/conan_home/ .
  chmod -R 777 /tmp/build/conan_home/
  conan profile new --detect default
  conan profile update settings.compiler.libcxx=libstdc++11 default
  conan config set general.revisions_enabled=1
  PKG_CONFIG_PATH=/local/lib/pkgconfig cmake -DCMAKE_BUILD_TYPE=$INPUT_BUILD_TYPE -DCMAKE_PROJECT_TOP_LEVEL_INCLUDES=./cmake/conan_provider.cmake -DASAN=$INPUT_ASAN /pktvisor-src
  make all -j 4
}

function move() {
  echo "========================= Compacting binary and copying ========================="
  cd /tmp/build
  cp -rf /tmp/build/bin/pktvisord /github/workspace/
  strip -s /tmp/build/bin/crashpad_handler
  cp -rf /tmp/build/bin/crashpad_handler /github/workspace/
  cp -rf /tmp/build/bin/pktvisor-reader /github/workspace/
  cp -rf /tmp/build/VERSION /github/workspace/
  chmod -R 777 /tmp/build/conan_home/
  cp -rf /tmp/build/conan_home/ /github/workspace/build/
  cp -rf /pktvisor-src/golang/pkg/client/version.go /github/workspace/version.go
  cp -rf /pktvisor-src/src/tests/fixtures/pktvisor-port-service-names.csv /github/workspace/custom-iana.csv
}

function publishToBugsplat() {
  echo "========================= Publishing symbol to bugsplat ========================="
  cd /tmp/build
  if [ "$INPUT_BUGSPLAT" == "true" ]; then
  wget https://github.com/orb-community/CrashpadTools/raw/main/linux/dump_syms
  chmod a+x ./dump_syms
  wget https://github.com/orb-community/CrashpadTools/raw/main/linux/symupload
  chmod a+x ./symupload
  ./dump_syms /github/workspace/pktvisord > pktvisor.sym
  PKTVISOR_VERSION=$(cat VERSION)
  ls -lha
  ./symupload -k $INPUT_BUGSPLAT_KEY pktvisor.sym $INPUT_BUGSPLAT_SYMBOL_URL$PKTVISOR_VERSION 2>/dev/null
  fi
}

validateParams
build
move
publishToBugsplat
