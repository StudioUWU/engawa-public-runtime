# syntax=docker/dockerfile:1.7

FROM --platform=linux/amd64 ubuntu:26.04@sha256:2260313b31c8c011cd2eebe728008efac1b3982be73eb71348ea2648d2c0e09b

ARG TARGETARCH
ARG ER_APT_PACKAGES
ARG ER_DEPENDENCY_JOBS=4
ARG ER_DOCKERFILE_SHA256
ARG ER_LOCK_SHA256
ARG ER_ICU_VERSION
ARG ER_ICU_URL
ARG ER_ICU_SHA256
ARG ER_HARFBUZZ_VERSION
ARG ER_HARFBUZZ_URL
ARG ER_HARFBUZZ_SHA256
ARG ER_LIBWPE_VERSION
ARG ER_LIBWPE_URL
ARG ER_LIBWPE_SHA256
ARG ER_LIBWPE_LICENSE_SHA256

LABEL org.opencontainers.image.title="EngawaRuntime Ubuntu 26 SDK builder" \
      org.opencontainers.image.base.name="ubuntu:26.04" \
      org.opencontainers.image.base.digest="sha256:2260313b31c8c011cd2eebe728008efac1b3982be73eb71348ea2648d2c0e09b" \
      com.studiouwu.engawaruntime.platform="linux/amd64" \
      com.studiouwu.engawaruntime.lock-sha256="${ER_LOCK_SHA256}" \
      com.studiouwu.engawaruntime.dockerfile-sha256="${ER_DOCKERFILE_SHA256}" \
      com.studiouwu.engawaruntime.icu-version="${ER_ICU_VERSION}" \
      com.studiouwu.engawaruntime.icu-sha256="${ER_ICU_SHA256}" \
      com.studiouwu.engawaruntime.harfbuzz-version="${ER_HARFBUZZ_VERSION}" \
      com.studiouwu.engawaruntime.harfbuzz-sha256="${ER_HARFBUZZ_SHA256}" \
      com.studiouwu.engawaruntime.libwpe-version="${ER_LIBWPE_VERSION}" \
      com.studiouwu.engawaruntime.libwpe-sha256="${ER_LIBWPE_SHA256}" \
      com.studiouwu.engawaruntime.libwpe-license-sha256="${ER_LIBWPE_LICENSE_SHA256}"

ENV DEBIAN_FRONTEND=noninteractive \
    TZ=UTC \
    LANG=C.UTF-8 \
    LC_ALL=C.UTF-8 \
    HOME=/home/ubuntu \
    PYTHONDONTWRITEBYTECODE=1 \
    ER_LINUX_DOCKER=1 \
    ER_ICU_ROOT=/opt/er/icu-77.1 \
    ER_HARFBUZZ_ROOT=/opt/er/harfbuzz-11.2.1 \
    ER_LIBWPE_ROOT=/opt/er/libwpe-1.16.3 \
    CC=clang \
    CXX=clang++ \
    AR=llvm-ar \
    NM=llvm-nm \
    RANLIB=llvm-ranlib

SHELL ["/bin/bash", "-euo", "pipefail", "-c"]

RUN test "${TARGETARCH}" = "amd64" \
    && test -n "${ER_APT_PACKAGES}" \
    && case "${ER_DEPENDENCY_JOBS}" in (*[!0-9]*|'') exit 2;; esac \
    && test "${ER_DEPENDENCY_JOBS}" -ge 1 \
    && grep -Fx 'VERSION_ID="26.04"' /etc/os-release \
    && test "$(dpkg --print-architecture)" = "amd64" \
    && apt-get update \
    && apt-get install --yes --no-install-recommends ${ER_APT_PACKAGES} \
    && dpkg-query -W -f='${binary:Package}\t${Version}\n' ${ER_APT_PACKAGES} \
        | LC_ALL=C sort > /tmp/er-apt-packages.tsv \
    && rm -rf /var/lib/apt/lists/*

COPY Ubuntu26.lock.json /opt/er/Ubuntu26.lock.json

RUN echo "${ER_LOCK_SHA256}  /opt/er/Ubuntu26.lock.json" | sha256sum --check --strict - \
    && install -d -m 0755 /opt/er/licenses \
    && curl --fail --location --proto '=https' --tlsv1.2 \
        --output /tmp/icu.tar.gz "${ER_ICU_URL}" \
    && echo "${ER_ICU_SHA256}  /tmp/icu.tar.gz" | sha256sum --check --strict - \
    && install -d -m 0755 /tmp/icu-src \
    && tar --extract --gzip --file /tmp/icu.tar.gz \
        --directory /tmp/icu-src --strip-components=1 \
    && cd /tmp/icu-src/source \
    && CFLAGS='-O2 -fPIC' CXXFLAGS='-O2 -fPIC' \
        ./runConfigureICU Linux \
            --prefix="${ER_ICU_ROOT}" \
            --libdir="${ER_ICU_ROOT}/lib" \
            --disable-debug \
            --disable-samples \
            --disable-shared \
            --disable-tests \
            --enable-release \
            --enable-static \
            --with-data-packaging=static \
    && make --jobs "${ER_DEPENDENCY_JOBS}" \
    && make install \
    && install -m 0644 /tmp/icu-src/LICENSE /opt/er/licenses/ICU-77.1.txt \
    && test -f "${ER_ICU_ROOT}/lib/libicudata.a" \
    && test -f "${ER_ICU_ROOT}/lib/libicui18n.a" \
    && test -f "${ER_ICU_ROOT}/lib/libicuuc.a" \
    && test ! -e "${ER_ICU_ROOT}/lib/libicuuc.so" \
    && PKG_CONFIG_PATH="${ER_ICU_ROOT}/lib/pkgconfig" \
        pkg-config --exact-version="${ER_ICU_VERSION}" icu-uc \
    && test "$(PKG_CONFIG_PATH="${ER_ICU_ROOT}/lib/pkgconfig" \
        pkg-config --variable=prefix icu-uc)" = "${ER_ICU_ROOT}" \
    && curl --fail --location --proto '=https' --tlsv1.2 \
        --output /tmp/harfbuzz.tar.xz "${ER_HARFBUZZ_URL}" \
    && echo "${ER_HARFBUZZ_SHA256}  /tmp/harfbuzz.tar.xz" \
        | sha256sum --check --strict - \
    && install -d -m 0755 /tmp/harfbuzz-src \
    && tar --extract --xz --file /tmp/harfbuzz.tar.xz \
        --directory /tmp/harfbuzz-src --strip-components=1 \
    && PKG_CONFIG_PATH="${ER_ICU_ROOT}/lib/pkgconfig" \
        CFLAGS='-O2 -fPIC' CXXFLAGS='-O2 -fPIC' \
        meson setup /tmp/harfbuzz-build /tmp/harfbuzz-src \
            --buildtype=release \
            --default-library=static \
            --libdir=lib \
            --prefix="${ER_HARFBUZZ_ROOT}" \
            -Db_staticpic=true \
            -Dbenchmark=disabled \
            -Dcairo=disabled \
            -Dchafa=disabled \
            -Dcoretext=disabled \
            -Ddirectwrite=disabled \
            -Ddocs=disabled \
            -Dfontations=disabled \
            -Dfreetype=enabled \
            -Dgdi=disabled \
            -Dglib=disabled \
            -Dgobject=disabled \
            -Dgraphite2=disabled \
            -Dharfruzz=disabled \
            -Dicu=enabled \
            -Dicu_builtin=false \
            -Dintrospection=disabled \
            -Dtests=disabled \
            -Dutilities=disabled \
            -Dwasm=disabled \
    && meson compile --jobs "${ER_DEPENDENCY_JOBS}" -C /tmp/harfbuzz-build \
    && meson install -C /tmp/harfbuzz-build \
    && install -m 0644 /tmp/harfbuzz-src/COPYING \
        /opt/er/licenses/HarfBuzz-11.2.1.txt \
    && test -f "${ER_HARFBUZZ_ROOT}/lib/libharfbuzz.a" \
    && test -f "${ER_HARFBUZZ_ROOT}/lib/libharfbuzz-icu.a" \
    && test ! -e "${ER_HARFBUZZ_ROOT}/lib/libharfbuzz.so" \
    && test ! -e "${ER_HARFBUZZ_ROOT}/lib/libharfbuzz-icu.so" \
    && PKG_CONFIG_PATH="${ER_HARFBUZZ_ROOT}/lib/pkgconfig:${ER_ICU_ROOT}/lib/pkgconfig" \
        pkg-config --exact-version="${ER_HARFBUZZ_VERSION}" harfbuzz \
    && test "$(PKG_CONFIG_PATH="${ER_HARFBUZZ_ROOT}/lib/pkgconfig:${ER_ICU_ROOT}/lib/pkgconfig" \
        pkg-config --variable=prefix harfbuzz)" = "${ER_HARFBUZZ_ROOT}" \
    && curl --fail --location --proto '=https' --tlsv1.2 \
        --output /tmp/libwpe.tar.xz "${ER_LIBWPE_URL}" \
    && echo "${ER_LIBWPE_SHA256}  /tmp/libwpe.tar.xz" \
        | sha256sum --check --strict - \
    && install -d -m 0755 /tmp/libwpe-src \
    && tar --extract --xz --file /tmp/libwpe.tar.xz \
        --directory /tmp/libwpe-src --strip-components=1 \
    && CFLAGS='-O2 -fPIC' CXXFLAGS='-O2 -fPIC' \
        meson setup /tmp/libwpe-build /tmp/libwpe-src \
            --buildtype=release \
            --default-library=static \
            --libdir=lib \
            --prefix="${ER_LIBWPE_ROOT}" \
            -Db_staticpic=true \
            -Dbuild-docs=false \
            -Denable-xkb=true \
    && meson compile --jobs "${ER_DEPENDENCY_JOBS}" -C /tmp/libwpe-build \
    && meson install -C /tmp/libwpe-build \
    && echo "${ER_LIBWPE_LICENSE_SHA256}  /tmp/libwpe-src/COPYING" \
        | sha256sum --check --strict - \
    && install -m 0644 /tmp/libwpe-src/COPYING \
        /opt/er/licenses/libwpe-1.16.3.txt \
    && test -f "${ER_LIBWPE_ROOT}/lib/libwpe-1.0.a" \
    && test ! -e "${ER_LIBWPE_ROOT}/lib/libwpe-1.0.so" \
    && PKG_CONFIG_PATH="${ER_LIBWPE_ROOT}/lib/pkgconfig" \
        pkg-config --exact-version="${ER_LIBWPE_VERSION}" wpe-1.0 \
    && test "$(PKG_CONFIG_PATH="${ER_LIBWPE_ROOT}/lib/pkgconfig" \
        pkg-config --variable=prefix wpe-1.0)" = "${ER_LIBWPE_ROOT}" \
    && install -m 0644 /tmp/er-apt-packages.tsv /opt/er/apt-packages.tsv \
    && rm -rf /tmp/icu.tar.gz /tmp/icu-src \
        /tmp/harfbuzz.tar.xz /tmp/harfbuzz-src /tmp/harfbuzz-build \
        /tmp/libwpe.tar.xz /tmp/libwpe-src /tmp/libwpe-build \
        /tmp/er-apt-packages.tsv

ENV CMAKE_PREFIX_PATH=/opt/er/libwpe-1.16.3:/opt/er/harfbuzz-11.2.1:/opt/er/icu-77.1 \
    PKG_CONFIG_PATH=/opt/er/libwpe-1.16.3/lib/pkgconfig:/opt/er/harfbuzz-11.2.1/lib/pkgconfig:/opt/er/icu-77.1/lib/pkgconfig

RUN test "$(id --user ubuntu)" = "1000" \
    && test "$(id --group ubuntu)" = "1000" \
    && test "$(getent passwd 1000 | cut --delimiter=: --fields=6)" = "/home/ubuntu" \
    && install -d -m 0755 -o 1000 -g 1000 /workspace \
    && install -m 0644 -o 1000 -g 1000 /dev/null /workspace/.er-image-seed

USER 1000:1000
WORKDIR /workspace

CMD ["python3", "--version"]
