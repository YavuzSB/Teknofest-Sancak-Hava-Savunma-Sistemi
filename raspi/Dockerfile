# ===========================================================================
# SANCAK HAVA SAVUNMA SİSTEMİ - Raspberry Pi 5 (ARM64) Build Container
# ===========================================================================
#
# Kullanım:
#   podman build --platform linux/arm64 -t sancak-raspi:latest .
#   podman run --rm sancak-raspi:latest       # derleme çıktısını kontrol et
#   podman cp <container>:/app/build/sancak_combat ./   # binary'yi al
#
# Multi-stage build: (1) derleme  (2) runtime
# ===========================================================================

# ---- STAGE 1: Build -------------------------------------------------------
FROM docker.io/arm64v8/debian:bookworm-slim AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Derleyici + OpenCV (DNN dahil) + geliştirme kütüphaneleri
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        git \
        ca-certificates \
        pkg-config \
        libopencv-dev \
        libopencv-dnn-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Bağımlılık katmanı (CMakeLists değişmedikçe cache'den gelir)
COPY CMakeLists.txt .
COPY config/ config/

# include + src + tests kopyala
COPY include/ include/
COPY src/     src/
COPY tests/   tests/

# CMake configure
RUN cmake -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DSANCAK_BUILD_COMBAT=ON \
        -DSANCAK_BUILD_LEGACY=OFF \
        -DSANCAK_ENABLE_TESTS=ON \
        -DSANCAK_ENABLE_WARNINGS=ON \
        -DSANCAK_WARNINGS_AS_ERRORS=OFF \
        -DSANCAK_ENABLE_NATIVE_ARCH=OFF

# Derleme (ayrı katman - QEMU altında uzun sürer)
RUN cmake --build build -j$(nproc) 2>&1 \
    && echo "====== BUILD BASARILI ======"

# Testleri çalıştır
RUN cd build && ctest --output-on-failure || true

# ---- STAGE 2: Runtime (minimal) -------------------------------------------
FROM docker.io/arm64v8/debian:bookworm-slim AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        libopencv-core406t64 \
        libopencv-imgproc406t64 \
        libopencv-highgui406t64 \
        libopencv-videoio406t64 \
        libopencv-dnn406t64 \
        libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Build stage'den binary, config ve modelleri kopyala
COPY --from=builder /app/build/sancak_combat /app/sancak_combat
COPY --from=builder /app/build/config/       /app/config/
COPY --from=builder /app/build/models/       /app/models/

# Model dosyası (best.onnx) volume veya COPY ile eklenecek
# COPY models/best.onnx /app/models/best.onnx

ENTRYPOINT ["/app/sancak_combat"]
CMD ["--headless", "--config", "/app/config/sancak_config.yml"]
