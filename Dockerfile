# =============================================================================
# Stage 1 — Build tgbot-cpp
# =============================================================================
FROM ubuntu:24.04 AS tgbot-build

RUN apt-get update && apt-get install -y --no-install-recommends \
        ninja-build g++ git ca-certificates python3-pip \
        libssl-dev libcurl4-openssl-dev libboost-all-dev \
    && pip3 install --break-system-packages cmake \
    && rm -rf /var/lib/apt/lists/*

RUN git clone --depth 1 https://github.com/reo7sp/tgbot-cpp.git /tmp/tgbot-cpp \
    && cmake -S /tmp/tgbot-cpp -B /tmp/tgbot-cpp/build \
        -DCMAKE_BUILD_TYPE=Release -G Ninja \
    && cmake --build /tmp/tgbot-cpp/build \
    && cmake --install /tmp/tgbot-cpp/build --prefix /opt/tgbot \
    && rm -rf /tmp/tgbot-cpp

# =============================================================================
# Stage 2 — Build MyBuddyBot
# =============================================================================
FROM ubuntu:24.04 AS app-build

RUN apt-get update && apt-get install -y --no-install-recommends \
        ninja-build g++ python3-pip \
        libssl-dev libcurl4-openssl-dev libboost-all-dev \
        libfmt-dev rapidjson-dev libsqlite3-dev \
    && pip3 install --break-system-packages cmake \
    && rm -rf /var/lib/apt/lists/*

COPY --from=tgbot-build /opt/tgbot /opt/tgbot

WORKDIR /src
COPY . .

RUN cmake -B build \
        -DCMAKE_BUILD_TYPE=Release \
        -DTGBOT_LIBRARY_LOCATION=/opt/tgbot \
        -DBUILD_TESTS=OFF \
        -G Ninja \
    && cmake --build build

# =============================================================================
# Stage 3 — Slim runtime image
# =============================================================================
FROM ubuntu:24.04 AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
        libssl3t64 libcurl4t64 libsqlite3-0 libfmt9 ffmpeg ca-certificates \
    && rm -rf /var/lib/apt/lists/*

RUN mkdir /data

COPY --from=app-build /src/build/MyBuddyBot /usr/local/bin/MyBuddyBot

ENV MYBUDDYBOT_DB_PATH=/data/mybuddybot.db
ENV MYBUDDYBOT_STATE_PATH=/data/MyBuddyBotState.bin
ENTRYPOINT ["stdbuf", "-oL", "-eL", "MyBuddyBot"]
