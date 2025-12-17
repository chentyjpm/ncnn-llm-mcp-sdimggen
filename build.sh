

set -e

cmake -S . -B build
cmake --build build -j

echo "Build completed: build/stable-diffusion-ncnn and build/stable-diffusion-ncnn-mcp"
