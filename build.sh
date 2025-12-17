

set -e

cmake -S . -B build -Dncnn_DIR="ncnn/ncnn-20230517-ubuntu-2204/lib/cmake/ncnn"
cmake --build build -j

echo "Build completed: build/stable-diffusion-ncnn and build/stable-diffusion-ncnn-mcp"
