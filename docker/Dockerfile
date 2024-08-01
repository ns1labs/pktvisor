FROM debian:bullseye-slim AS cppbuild

ENV BUILD_DEPS "g++ cmake make git pkgconf jq python3-pip python3-setuptools python ca-certificates"

RUN \
    apt-get update && \
    apt-get upgrade --yes --force-yes && \
    apt-get install --yes --force-yes --no-install-recommends ${BUILD_DEPS} && \
    pip3 install "conan==1.64.1"

# need git for current hash for VERSION
COPY ./.git/ /pktvisor-src/.git/
COPY ./src/ /pktvisor-src/src/
COPY ./cmd/ /pktvisor-src/cmd/
COPY ./3rd/ /pktvisor-src/3rd/
COPY ./libs/ /pktvisor-src/libs/
COPY ./docker/ /pktvisor-src/docker/
COPY ./golang/ /pktvisor-src/golang/
COPY ./integration_tests/ /pktvisor-src/integration_tests/
COPY ./cmake/ /pktvisor-src/cmake/
COPY ./CMakeLists.txt /pktvisor-src/
COPY ./conanfile.txt /pktvisor-src/

WORKDIR /tmp/build

RUN \
    conan profile new --detect default && \
    conan profile update settings.compiler.libcxx=libstdc++11 default && \
    conan config set general.revisions_enabled=1

RUN \
    PKG_CONFIG_PATH=/local/lib/pkgconfig cmake -DCMAKE_BUILD_TYPE=Release /pktvisor-src && \
    make all -j 4

FROM golang:latest AS gobuild
COPY golang/ /src/
WORKDIR /src/
COPY --from=cppbuild /pktvisor-src/golang/pkg/client/version.go /src/pkg/client/version.go
RUN go build -o pktvisor-cli cmd/pktvisor-cli/main.go

FROM debian:bullseye-slim AS runtime

ENV RUNTIME_DEPS "curl ca-certificates wget"

RUN \
    apt-get update && \
    apt-get upgrade --yes --force-yes && \
    apt-get install --yes --force-yes --no-install-recommends ${RUNTIME_DEPS} && \
    rm -rf /var/lib/apt
    
#create dir and download geo db
RUN mkdir /geo-db
RUN cd /geo-db && wget https://github.com/orb-community/geo-asn-database/raw/main/asn.mmdb.gz
RUN cd /geo-db && wget https://github.com/orb-community/geo-asn-database/raw/main/city.mmdb.gz

RUN mkdir /iana
COPY --from=cppbuild /pktvisor-src/src/tests/fixtures/pktvisor-port-service-names.csv /iana/custom-iana.csv

COPY --from=cppbuild /tmp/build/bin/pktvisord /usr/local/sbin/pktvisord
COPY --from=cppbuild /tmp/build/bin/crashpad_handler /usr/local/sbin/crashpad_handler
COPY --from=cppbuild /tmp/build/bin/pktvisor-reader /usr/local/sbin/pktvisor-reader
COPY --from=gobuild /src/pktvisor-cli /usr/local/bin/pktvisor-cli
COPY ./docker/entry-cp.sh /entry-cp.sh
COPY ./docker/run.sh /run.sh
RUN chmod a+x /entry-cp.sh
RUN chmod a+x /run.sh

ENTRYPOINT [ "/entry-cp.sh" ]

