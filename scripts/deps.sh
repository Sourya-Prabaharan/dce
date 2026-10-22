#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p deps
verify() { if command -v sha256sum >/dev/null; then echo "$1  $2" | sha256sum -c -; else echo "$1  $2" | shasum -a 256 -c -; fi; }
if [ ! -f deps/boost_1_86_0/boost/asio.hpp ]; then
  curl -fL --retry 3 https://archives.boost.io/release/1.86.0/source/boost_1_86_0.tar.bz2 -o deps/boost.tar.bz2
  verify 1bed88e40401b2cb7a1f76d4bab499e352fa4d0c5f31c0dbae64e24d34d7513b deps/boost.tar.bz2
  tar -xjf deps/boost.tar.bz2 -C deps
  rm deps/boost.tar.bz2
fi
if [ ! -f deps/json.hpp ]; then
  curl -fL --retry 3 https://raw.githubusercontent.com/nlohmann/json/v3.11.3/single_include/nlohmann/json.hpp -o deps/json.hpp
fi
verify 9bea4c8066ef4a1c206b2be5a36302f8926f7fdc6087af5d20b417d0cf103ea6 deps/json.hpp
