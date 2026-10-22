FROM debian:bookworm-slim AS build
RUN apt-get update && apt-get install -y --no-install-recommends g++ cmake make curl ca-certificates bzip2 && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY scripts/deps.sh scripts/deps.sh
RUN sh scripts/deps.sh
COPY CMakeLists.txt ./
COPY include include
COPY src src
COPY tests tests
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j 2 && ctest --test-dir build -R '^unit$' --output-on-failure
FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends libstdc++6 && rm -rf /var/lib/apt/lists/* && useradd -u 10001 -m dce
WORKDIR /app
COPY --from=build /src/build/dce-coordinator /src/build/dce-worker /src/build/dce-cli /usr/local/bin/
COPY web web
USER dce
EXPOSE 8080 9000
CMD ["dce-coordinator", "--bind", "0.0.0.0", "--data", "/data", "--dashboard", "/app/web/index.html"]
