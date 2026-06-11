cmake_minimum_required(VERSION 3.24) # Для CUDA предпочтительно > 3.18
message(STATUS "CMake version: ${CMAKE_VERSION}")

set(CMAKE_CUDA_COMPILER "/usr/local/cuda/bin/nvcc") 
# message(STATUS "NVCC version: ${CMAKE_CUDA_COMPILER_VERSION}")

project(cuda_findcontours LANGUAGES CXX CUDA)

# Архитектуры GPU для компиляции 
set(CMAKE_CUDA_ARCHITECTURES 61)

# Поиск CUDA Toolkit
find_package(CUDAToolkit REQUIRED)
message(STATUS "CUDAToolkit version: ${CUDAToolkit_VERSION}")
# Включение поддержки языка CUDA C
enable_language(CUDA)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} --extended-lambda")

# Поиск OpenCV
set(OpenCV_DIR "/opt/opencv_cuda/lib/cmake/opencv4" CACHE PATH "OpenCV config dir") # Установка нестандартного пути
find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui imgcodecs)
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV config dir   : ${OpenCV_DIR}")
message(STATUS "OpenCV include dirs : ${OpenCV_INCLUDE_DIRS}")
message(STATUS "OpenCV libs found: ${OpenCV_LIBS}")

add_executable(
    cuda_findcontours
    cuda_findcontours_bke.cu
)

# Линковка библиотек
target_link_libraries(cuda_findcontours PRIVATE opencv_core opencv_imgproc opencv_highgui opencv_imgcodecs)

