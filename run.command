#!/bin/zsh
cd "$(dirname "$0")"
# 确保已经编译过一次
if [ ! -f "build/campus_sim" ]; then
  mkdir -p build
  cd build
  cmake ..
  cmake --build .
  cd ..
fi
./build/campus_sim
