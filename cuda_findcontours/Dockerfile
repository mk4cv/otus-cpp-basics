# docker build -t cv_cuda_c:ubuntu24-cuda12.8-devel-1.0 .

# Базовый образ - Nvidia CUDA devel (содержит заголовочные файлы cuda)
# Версия CUDA на хосте должна быть такой же или новее версии CUDA в контейнере
FROM nvcr.io/nvidia/cuda:12.8.1-cudnn-devel-ubuntu24.04

ENV DEBIAN_FRONTEND=noninteractive

# Обновление
RUN apt-get update && apt-get install -y

# Корректировка пути к библиотеке libnvrtc
RUN if [ ! -f /usr/local/cuda/lib64/libnvrtc.so ]; then \
        ln -s /usr/local/cuda/lib64/libnvrtc.so.12 /usr/local/cuda/lib64/libnvrtc.so; \
    else \
        echo "Символическая ссылка libnvrtc.so уже существует."; \
    fi

# Установка OpenCV (из стандартного репозиторя)
RUN apt-get install -y \
    libopencv-dev

# Очистка кеша
RUN apt-get clean && apt-get autoremove -y