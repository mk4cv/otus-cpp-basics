#include <opencv2/opencv.hpp>
#include <cuda_runtime.h>
#include <cub/cub.cuh>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <future>
#include <functional>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <iostream>
#include <iomanip>

// =====================================================================================================
/* 
    Поиск связных компонент, центроидов и контуров объектов на бинарном изображении с помощью CUDA.

    Вход:  бинарное изображение (uchar*, 0=фон, >0=объект) на GPU

    Архитектура:
        Этап 1: Детекция границ 
            CUDA kernel: k_detectBorderAndMask
        Этап 2: Префиксное сканирование (CUB) - определение позиции для записи координат каждого граничного пикселя
            cub::DeviceScan
        Этап 3: Подсчет количества граничных пикселей.   
        Этап 4: Сбор координат граничных пикселей.
            CUDA kernel: k_scatterCoords
        Этап 5: Локальная инициализация BKE - Union-Find внутри тайлов 16x16.
            CUDA kernel: k_bke_localInit         
        Этап 6: Глобальное слияние.
            CUDA kernel: k_bke_global 
        Этап 7: Сжатие путей (Path Compression).
            CUDA kernel: k_bke_flatten 
        Этап 8: Сбор меток.
            CUDA kernel: k_scatterLabels
        Этап 9: Сортировка (Radix Sort).
            cub::DeviceRadixSort
        Этап 10: Передача данных на CPU.
        Этап 11: Группировка пикселей в объекты и трассировка контуров методом Мура.
            cpuGroupAndTrace 

    Вывод: вектор ObjectStats (label, area, bbox, centroid, contour)
*/

// =====================================================================================================
/*
    Макрос проверки ошибок CUDA.
    Оборачивает любой CUDA вызов, при ошибке печатает файл/строку и вызывает abort().
    Применяется для cudaMalloc, cudaMemcpy, cudaStreamSynchronize.
    printf для совместимости с CUDA C.
*/
#define CHECK_CUDA(call) do {                                        \
    cudaError_t e = (call);                                          \
    if (e != cudaSuccess) {                                          \
        fprintf(stderr, "CUDA error %s:%d  %s\n",                   \
                __FILE__, __LINE__, cudaGetErrorString(e));          \
        std::abort();                                                \
    }                                                                \
} while(0)

// =====================================================================================================
/*
    Константы тайлов

    TILE_W / TILE_H = 16
    Размер блока для k_detectBorderAndMask (kernel с halo-загрузкой).
    16x16 = 256 потоков/блок - оптимальная occupancy для kernel с shared memory.
    Каждый блок загружает (16+2)x(16+2) = 324 байта в shared memory.

    BKE_TILE = 16
    Размер блока для k_bke_localInit (Union-Find в shared memory).
    256 потоков/блок -- компилятор размещает больше блоков на SM,
    снижает давление на регистры и улучшает скрытие latency.
*/
static constexpr int TILE_W = 16;
static constexpr int TILE_H = 16;
static constexpr int BKE_TILE = 16;

// =====================================================================================================
/*
    Вспомогательные функции для Union-Find

    Union-Find (Disjoint Set Union) - структура для объединения граничных пикселей в связные компоненты.
    Инвариант: root(x) = минимальный индекс в компоненте.
    Каждый пиксель хранит ссылку на родителя, корень ссылается на себя.
*/

// Поиск корня в глобальной памяти (для k_bke_global, k_bke_flatten).
__device__ __forceinline__
int bke_find(int* L, int x) {
    while (L[x] != x) x = L[x];
    return x;
}

// Поиск корня в shared memory (для k_bke_localInit).
__device__ __forceinline__
int bke_find_shared(int* sL, int x) {
    while (sL[x] != x) x = sL[x];
    return x;
}



// =====================================================================================================
/*
    Kernel 1: k_detectBorderAndMask

    Определение граничных пикселей - объектные пиксели (v>0), у которых хотя бы один из 8 соседей является фоновым (v=0).

    Особенности:
    1. Shared memory с halo 1px: блок загружает (W+2)x(H+2) пикселей, устраняя повторные обращения к DRAM для соседних пикселей.
    2. __ldg(): чтение через read-only texture cache (L1, 32KB) вместо L2.
    3. Граничные пиксели кадра (gx=0/W-1, gy=0/H-1) = 0, чтобы избежать детекции периметра кадра как контура объекта.

    Вход:  бинарное изображение (uchar*, 0=фон, >0=объект) на GPU

    Вывод:
    brd[i]  = 1 (граничный пиксель) или 0  -- тип uchar
    mask[i] = 1 (граничный пиксель) или 0  -- тип int (для CUB scan)
*/
__global__
void k_detectBorderAndMask(const uchar* __restrict__ src,
                            uchar* __restrict__ brd,
                            int*  __restrict__ mask,
                            int W, int H)
{
    // Shared memory: тайл (TILE_W+2)x(TILE_H+2) (+1px halo со всех сторон)
    __shared__ uchar smem[(TILE_H + 2) * (TILE_W + 2)];
    const int SW = TILE_W + 2;  // ширина строки в shared memory (с учетом halo)

    int tx = threadIdx.x;  // локальная X-координата потока в блоке [0, TILE_W)
    int ty = threadIdx.y;  // локальная Y-координата потока в блоке [0, TILE_H)
    int gx = blockIdx.x * TILE_W + tx;  // глобальная X-координата пикселя
    int gy = blockIdx.y * TILE_H + ty;  // глобальная Y-координата пикселя

    // Загрузка тайла в shared memory с halo
    // Каждый поток загружает свой центральный пиксель.
    // Граничные потоки блока дополнительно загружают halo-пиксели соседних блоков.
    {
        int sx = tx + 1, sy = ty + 1;  // позиция в smem (смещение на 1 = halo)

        // Центральный пиксель текущего потока
        smem[sy * SW + sx] = (gx < W && gy < H) ? __ldg(&src[gy * W + gx]) : 0;

        // Левый halo: загружает только первый столбец блока (tx==0)
        if (tx == 0) {
            int hx = gx - 1;
            smem[sy * SW + 0] = (hx >= 0 && gy < H) ? __ldg(&src[gy * W + hx]) : 0;
        }
        // Правый halo: загружает только последний столбец блока
        if (tx == TILE_W - 1) {
            int hx = gx + 1;
            smem[sy * SW + (SW-1)] = (hx < W && gy < H) ? __ldg(&src[gy * W + hx]) : 0;
        }
        // Верхний halo: загружает только первая строка блока (ty==0)
        if (ty == 0) {
            int hy = gy - 1;
            smem[0 * SW + sx] = (gx < W && hy >= 0) ? __ldg(&src[hy * W + gx]) : 0;
        }
        // Нижний halo: загружает только последняя строка блока
        if (ty == TILE_H - 1) {
            int hy = gy + 1;
            smem[(TILE_H+1)*SW + sx] = (gx < W && hy < H) ? __ldg(&src[hy * W + gx]) : 0;
        }
        // Угловые пиксели halo (4 угла блока загружаются угловыми потоками)
        if (tx == 0 && ty == 0) {
            int hx = gx-1, hy = gy-1;
            smem[0] = (hx >= 0 && hy >= 0) ? __ldg(&src[hy * W + hx]) : 0;
        }
        if (tx == TILE_W-1 && ty == 0) {
            int hx = gx+1, hy = gy-1;
            smem[SW-1] = (hx < W && hy >= 0) ? __ldg(&src[hy * W + hx]) : 0;
        }
        if (tx == 0 && ty == TILE_H-1) {
            int hx = gx-1, hy = gy+1;
            smem[(TILE_H+1)*SW] = (hx >= 0 && hy < H) ? __ldg(&src[hy * W + hx]) : 0;
        }
        if (tx == TILE_W-1 && ty == TILE_H-1) {
            int hx = gx+1, hy = gy+1;
            smem[(TILE_H+1)*SW+(SW-1)] = (hx < W && hy < H) ? __ldg(&src[hy * W + hx]) : 0;
        }
    }
    __syncthreads();  // Все потоки блока завершили загрузку shared memory

    if (gx >= W || gy >= H) return;  // Потоки за пределами изображения -> выход

    uchar result = 0;

    // Граничные пиксели кадра принудительно = 0 (периметр кадра не детектируется)
    if (gx > 0 && gx < W-1 && gy > 0 && gy < H-1) {
        int sx = tx + 1, sy = ty + 1;  // Позиция текущего пикселя в shared memory
        uchar v = smem[sy * SW + sx];  // Значение текущего пикселя

        if (v) {
            // Пиксель объектный (v=1) -> проверка всех 8-и соседей через побитовый OR
            uchar n00 = smem[(sy-1)*SW+(sx-1)], n01 = smem[(sy-1)*SW+sx],
                  n02 = smem[(sy-1)*SW+(sx+1)], n10 = smem[ sy   *SW+(sx-1)],
                  n12 = smem[ sy   *SW+(sx+1)], n20 = smem[(sy+1)*SW+(sx-1)],
                  n21 = smem[(sy+1)*SW+sx],     n22 = smem[(sy+1)*SW+(sx+1)];
            // Граничный, если хотя бы один сосед фоновый (=0)
            uchar allFilled = (n00 && n01 && n02 && n10 && n12 && n20 && n21 && n22) ? 1 : 0;
            result = allFilled ? 0 : 1;
        }
        // Если 1 -> есть объектный сосед -> граничный пиксель
        // Если v != 0 (объектный пиксель) - result остается 0.
    }

    brd [gy * W + gx] = result;  // Карта граничных пикселей (uchar, для Union-Find)
    mask[gy * W + gx] = result;  // арта граничных пикселей (int, для CUB ExclusiveSum)
}

// =====================================================================================================
/*
    Kernel 2: k_scatterCoords

    Компактизация: сбор координат граничных пикселей в плотный массив.
    Каждый граничный пиксель записывает свои (x, y) по индексу из prefix sum.

    Вход:
    brd[]  - карта граничных пикселей (0 или 1)
    scan[] - эксклюзивный prefix sum brd[] (от CUB ExclusiveSum):
                scan[i] = количество граничных пикселей с индексами [0, i)

    Алгоритм:
    Если brd[i]=1, то scan[i] = позиция пикселя в компактном массиве.
    coords[scan[i]] = make_int2(i % W, i / W) - координаты (x, y).

    Особеннности:
    1. __ballot_sync() - битовая маска потоков варпа с b != 0.
    2. __popc() - popcount для вычисления warp-level rank.
*/
// Исправление по замечанию №5
__global__
void k_scatterCoords(const uchar* __restrict__ brd,
                     const int*   __restrict__ scan,
                     int2* coords, int W, int N)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;  // глобальный индекс пикселя
    if (i >= N) return;

    uchar b = brd[i];  // 1 = граничный пиксель, 0 = нет

    if (b) { // Граница внутри объекта
        coords[scan[i]] = make_int2(i % W, i / W);
    }

}


// =====================================================================================================
/*
 Kernel 3: k_bke_localInit

    Первый этап BKE: инициализация Union-Find и локальное слияние компонент
    внутри каждого тайла BKE_TILE x BKE_TILE целиком в shared memory.

    Алгоритм:
    1. Инициализация: граничный пиксель -> sL[li] = li (корень себя), фоновый пиксель -> sL[li] = -1 (не участвует).
    2. Итеративное слияние (до BKE_TILE итераций или до конвергенции).
    Каждый граничный пиксель проверяет 4 соседа: left, top, top-left, top-right  и объединяет через Union-Find .
    3. Экспорт: локальный индекс корня -> глобальный индекс -> L[gi].

    При обходе слева->право, сверху->вниз достаточно проверять уже обработанных соседей. 
    Правый и нижний будут обработаны позже или в k_bke_global при слиянии на границах тайлов.

    Особенности:
    1. Вся работа в shared memory (нет обращений к DRAM внутри цикла).
    2.  __syncthreads_or() для раннего выхода при конвергенции.
    3. BKE_TILE=16 -> 256 потоков/блок
*/
__global__
void k_bke_localInit(const uchar* __restrict__ brd, int* L, int W, int H)
{
    // Shared memory для Union-Find тайла BKE_TILE x BKE_TILE
    __shared__ int sL[BKE_TILE * BKE_TILE];

    int tx = threadIdx.x, ty = threadIdx.y;
    int gx = blockIdx.x * BKE_TILE + tx;  // глобальная X-координата
    int gy = blockIdx.y * BKE_TILE + ty;  // глобальная Y-координата
    int li = ty * BKE_TILE + tx;          // локальный индекс в тайле [0, 256)
    int gi = gy * W + gx;                 // глобальный линейный индекс пикселя

    bool valid = (gx < W && gy < H);
    // Чтение через __ldg (texture cache) - brd[] не изменяется в этом kernel
    bool isBrd = valid && (__ldg(&brd[gi]) != 0);

    // Инициализация UF: граничный -> корень себя, фоновый -> -1
    sL[li] = isBrd ? li : -1;
    __syncthreads();  // Cинхронизация - все потоки завершили инициализацию sL

    // Итеративное локальное слияние в shared memory
    for (int iter = 0; iter < BKE_TILE; ++iter) {
        bool changed = false;  // флаг изменений в этой итерации

        if (isBrd) {
            int ra = bke_find_shared(sL, li);  // корень текущего пикселя

            // Сосед слева: (tx-1, ty) - только если не на левом крае тайла
            if (tx > 0) {
                int nb = ty * BKE_TILE + (tx-1);
                if (sL[nb] >= 0) {  // сосед граничный (не -1)
                    int rb = bke_find_shared(sL, nb);
                    if (ra != rb) {
                        int lo = min(ra,rb), hi = max(ra,rb);
                        // Объединяем: hi -> lo (меньший индекс = корень)
                        if (sL[hi] != lo) { sL[hi] = lo; changed = true; }
                        ra = lo;  // Обновление локального корня для следующих проверок
                    }
                }
            }
            // Сосед сверху: (tx, ty-1)
            if (ty > 0) {
                int nb = (ty-1) * BKE_TILE + tx;
                if (sL[nb] >= 0) {
                    int rb = bke_find_shared(sL, nb);
                    if (ra != rb) {
                        int lo = min(ra,rb), hi = max(ra,rb);
                        if (sL[hi] != lo) { sL[hi] = lo; changed = true; }
                        ra = lo;
                    }
                }
            }
            // Диагональ сверху-слева: (tx-1, ty-1)
            if (tx > 0 && ty > 0) {
                int nb = (ty-1) * BKE_TILE + (tx-1);
                if (sL[nb] >= 0) {
                    int rb = bke_find_shared(sL, nb);
                    if (ra != rb) {
                        int lo = min(ra,rb), hi = max(ra,rb);
                        if (sL[hi] != lo) { sL[hi] = lo; changed = true; }
                        ra = lo;
                    }
                }
            }
            // Диагональ сверху-справа: (tx+1, ty-1)
            if (tx < BKE_TILE-1 && ty > 0) {
                int nb = (ty-1) * BKE_TILE + (tx+1);
                if (sL[nb] >= 0) {
                    int rb = bke_find_shared(sL, nb);
                    if (ra != rb) {
                        int lo = min(ra,rb), hi = max(ra,rb);
                        if (sL[hi] != lo) { sL[hi] = lo; changed = true; }
                    }
                }
            }
        }
        __syncthreads();  // Cинхронизация перед проверкой changed

        // Ранний Вывод: если ни один поток блока не сделал изменений - конвергенция
        if (!__syncthreads_or((int)changed)) break;
    }

    // Экспорт: локальный корень -> глобальный индекс -> L[]
    if (valid && isBrd) {
        int root_local = bke_find_shared(sL, li);  // корень в локальном Union-Find
        // Конвертиртация локального индекса корня в глобальные координаты
        int root_lx = root_local % BKE_TILE;
        int root_ly = root_local / BKE_TILE;
        int root_gx = blockIdx.x * BKE_TILE + root_lx;
        int root_gy = blockIdx.y * BKE_TILE + root_ly;
        L[gi] = root_gy * W + root_gx;  // глобальный линейный индекс корня
    } else if (valid) {
        L[gi] = -1;  // фоновый пиксель (не принадлежит ни одной компоненте)
    }
}

// =====================================================================================================
/*
    Kernel 4: k_bke_global

    Второй этап BKE: слияние компонент на границах тайлов.
    После k_bke_localInit объекты, пересекающие границы тайлов, ещё не объединены. 
    Алгоритм обрабатывает "швы" (seams) между тайлами.

    Алгоритм:
    Каждый поток обрабатывает один пиксель на горизонтальном или вертикальном шве. 
    Если оба пикселя по обе стороны шва граничные и принадлежат разным компонентам - объединение через atomicMin.

    Запускается в цикле с флагом changed (pinned memory).
    Если за итерацию не было ни одного слияния -- конвергенция, выход.
    Теоретический максимум - (max(nTilesX, nTilesY) + 1) / 2.
*/
__global__
void k_bke_global(const uchar* __restrict__ brd,
                  int* L, int* changed, int W, int H)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;

    int nTilesX = (W + BKE_TILE - 1) / BKE_TILE;  // число тайлов по X
    int nTilesY = (H + BKE_TILE - 1) / BKE_TILE;  // число тайлов по Y

    // Горизонтальные швы: (nTilesY-1) строк x W пикселей
    int hSeams  = (nTilesY - 1) * W;
    // Вертикальные швы: (nTilesX-1) столбцов x H пикселей
    int vSeams  = (nTilesX - 1) * H;

    if (idx < hSeams) {
        // Горизонтальный шов
        int x  = idx % W;          // X-координата пикселя на шве
        int ty = idx / W;          // номер шва (0..nTilesY-2)
        int y0 = (ty + 1) * BKE_TILE - 1;  // последняя строка верхнего тайла
        int y1 = y0 + 1;                    // первая строка нижнего тайла
        if (y1 >= H) return;

        int g0 = y0 * W + x, g1 = y1 * W + x;
        if (__ldg(&brd[g0]) && __ldg(&brd[g1])) {  // оба пикселя граничные
            int r0 = bke_find(L, g0), r1 = bke_find(L, g1);
            if (r0 != r1) {
                int lo = min(r0,r1), hi = max(r0,r1);
                int old = atomicMin(&L[hi], lo);   // атомарное слияние hi -> lo
                if (old > lo) atomicExch(changed, 1);  // сигнал о продолжении
            }
        }
    } else {
        // Вертикальный шов
        int vi = idx - hSeams;
        if (vi >= vSeams) return;
        int y  = vi % H;           // Y-координата пикселя на шве
        int tx = vi / H;           // номер шва (0..nTilesX-2)
        int x0 = (tx + 1) * BKE_TILE - 1;  // последний столбец левого тайла
        int x1 = x0 + 1;                    // первый столбец правого тайла
        if (x1 >= W) return;

        int g0 = y * W + x0, g1 = y * W + x1;
        if (__ldg(&brd[g0]) && __ldg(&brd[g1])) {
            int r0 = bke_find(L, g0), r1 = bke_find(L, g1);
            if (r0 != r1) {
                int lo = min(r0,r1), hi = max(r0,r1);
                int old = atomicMin(&L[hi], lo);
                if (old > lo) atomicExch(changed, 1);
            }
        }
    }
}

// =====================================================================================================
/*
    Kernel 5: k_bke_flatten

    Финальное сжатие путей Union-Find (path compression).
    После k_bke_global дерево Union-Find  может содержать длинные цепочки.
    Алгоритм для каждого граничного пикселя находит корень и записывает его напрямую в L[i], устраняя промежуточные узлы.

    Вывод:
    L[i] = глобальный индекс корня компоненты для пикселя i.
    Все пиксели одной компоненты имеют одинаковый L[i] = label.
*/
__global__
void k_bke_flatten(int* L, const uchar* __restrict__ brd, int N) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= N || !__ldg(&brd[i])) return;  // только граничные пиксели
    int root = bke_find(L, i);  // поднимаемся до корня (path traversal)
    L[i] = root;                // записываем корень напрямую (path compression)
}

// ================================================================================
/*
    Kernel 6: k_scatterLabels

    Для каждого граничного пикселя по его координатам из coords[]
    записывается метка его компоненты (из L[]) в выходной массив ol[].

    Вывод:
    ol[i] = метка компоненты для i-го граничного пикселя.
    Пара (coords[i], ol[i]) = (координата, метка) для каждого пикселя.
    После RadixSort по ol[] пиксели одного объекта стоят подряд.
*/
__global__
void k_scatterLabels(const int* __restrict__ L,
                     const int2* __restrict__ coords,
                     int* ol, int nBrd, int W)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= nBrd) return;
    int2 c = coords[i];         // координаты i-го граничного пикселя
    ol[i] = L[c.y * W + c.x];  // метка компоненты из массива L[]
}

// ================================================================================
/*
    Kernel 7: k_iota

    Заполнение массива значениями [0, 1, 2, ..., N-1].
    Используется для инициализации массива индексов перед RadixSort - 
    сортировка пары (метка=ключ, индекс=значение), чтобы после сортировки знать исходный порядок пикселей для доступа к coords[].
*/
__global__
void k_iota(int* arr, int N) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < N) arr[i] = i;  // arr[i] = i
}



// ================================================================================
/*
    CPU:: Вычисление площади по формуле Пика
    Позволяет вычислять площадь объектов толщиной в 1 пиксель, учитывает сам контур (который теперь находится внутри объекта)
*/
static void computePixelAccurateAreaAndCentroid(
    const std::vector<cv::Point>& contour,
    double& area, cv::Point2f& centroid)
{
    int n = (int)contour.size();
    if (n == 0) { area = 0; centroid = {0,0}; return; }

    // Вырожденный случай: 1-2 точки — полигон не определен,
    // площадь = число уникальных пикселей
    if (n < 3) {
        double sx = 0, sy = 0;
        for (auto& p : contour) { sx += p.x; sy += p.y; }
        area = n;
        centroid = cv::Point2f((float)(sx / n), (float)(sy / n));
        return;
    }

    double signedArea2 = 0.0, sumCx = 0.0, sumCy = 0.0;
    for (int i = 0; i < n; ++i) {
        double x0 = contour[i].x,       y0 = contour[i].y;
        double x1 = contour[(i+1)%n].x, y1 = contour[(i+1)%n].y;
        double cross = x0 * y1 - x1 * y0;
        signedArea2 += cross;
        sumCx += (x0 + x1) * cross;
        sumCy += (y0 + y1) * cross;
    }
    double As = 0.5 * signedArea2;

    // Поправка Пика: shoelace-площадь (через центры пикселей) 
    area = std::abs(As) + (double)n / 2.0 + 1.0;

    if (std::abs(As) < 1e-9) {
        // Вырожденная фигура (линия толщиной 1px) — центроид через отношение sumCx/(6*As)
        double sx = 0, sy = 0;
        for (auto& p : contour) { sx += p.x; sy += p.y; }
        centroid = cv::Point2f((float)(sx / n), (float)(sy / n));
    } else {
        centroid = cv::Point2f((float)(sumCx / (6.0 * As)),
                                (float)(sumCy / (6.0 * As)));
    }
}

// ================================================================================
/*
    CPU: mooreTraceFromCoords - трассировка контура методом Мура.

    Построение упорядоченного контура объекта по его локальной маске.
    Метод Мура (Moore Neighbor Tracing) - алгоритм обхода границы бинарного объекта с критерием остановки.

    Алгоритм:
    1. Старт: startPoint - первый пиксель объекта (top-left в bbox).
    2. Обход 8-и соседей
    3. Первый найденный граничный сосед - следующая точка контура.
    4. Обновление dir = (nd + 5) % 8 - направление "назад" от нового пикселя.
    5. Критерий остановки: возврат в startPoint, направление = firstStepDir.

    Ограниечение итераций:
    safety = 2 * (bW + bH) * 4 - верхняя оценка длины контура.
    Для bbox оценки используетс периметр bbox * 8 как консервативный верхний предел.

    Вход:
    objCoords[]     координаты пикселей объекта
    nPts            количество пикселей объекта
    startX, startY  стартовая точка (глобальные координаты)
    globalOffX/Y    смещение локальной маски (= minX-1, minY-1)
    localMask[]     бинарная маска объекта в координатах bbox+halo
    bW, bH          размеры локальной маски (bbox.width+2, bbox.height+2)
    out             выходной вектор точек контура
*/

// Таблица смещений для 8 соседей:
// idx: 0=N   1=NW  2=W   3=SW  4=S   5=SE  6=E   7=NE
static const int MX[8] = { 0, -1, -1, -1,  0,  1, 1, 1};
static const int MY[8] = {-1, -1,  0,  1,  1,  1, 0,-1};

static void mooreTraceFromCoords(
    const int2* objCoords, int nPts,
    int startX, int startY,
    int globalOffX, int globalOffY,
    const uchar* localMask, int bW, int bH,
    std::vector<cv::Point>& out)
{
    out.clear();

    // Перевод стартовой точки в локальные координаты маски
    int lsx = startX - globalOffX;
    int lsy = startY - globalOffY;

    // Валидация стартовой точки
    if (lsx < 0 || lsx >= bW || lsy < 0 || lsy >= bH) return;
    if (!localMask[lsy * bW + lsx]) return;  // стартовая точка должна быть в маске

    // Лимит итераций
    int safety = 2 * (bW + bH) * 4;

    int backDir = 2;          // начальное направление "назад" = 2, стартовая точка - top-left
    int cx = lsx, cy = lsy;   // текущая позиция (локальные координаты)
    int dir = backDir;        // начало обхода с backDir
    int firstStepDir = -1;    // направление первого шага

    // Добавление стартовой  точки в контур (глобальные координаты)
    out.push_back({startX, startY});

    bool started = false;  // флаг отхода от старта

    for (int step = 0; step < safety; ++step) {
        bool found = false;

        for (int k = 0; k < 8; ++k) {
            int nd = (dir + k) % 8;
            int nx = cx + MX[nd];
            int ny = cy + MY[nd];

            if (nx < 0 || nx >= bW || ny < 0 || ny >= bH) continue;
            if (!localMask[ny * bW + nx]) continue;

            // Критерий остановки: возврат в старт после начала обхода
            if (started && nx == lsx && ny == lsy) {
                return;  // контур замкнут
            }

            started = true;  // установка флага отхода от старта

            out.push_back({nx + globalOffX, ny + globalOffY});
            cx = nx;
            cy = ny;
            dir = (nd + 5) % 8;
            found = true;
            break;
        }

        if (!found) break;
    }

}

// =====================================================================================================
/*
    CPU: StaticThreadPool - пул потоков для параллельной обработки объектов

    Параллельная трассировка контуров нескольких объектов.
    Каждый объект обрабатывается независимо -> идеально для параллелизации.

    Архитектура:
    - N рабочих потоков (по умолчанию = hardware_concurrency)
    - Очередь задач (std::queue<std::function<void()>>)
    - submit() возвращает std::future<void> для синхронизации завершения

    Создается один раз в конструкторе CudaFindContoursBKE.
    Потоки живут все время работы объекта (нет overhead на создание/уничтожение).
*/
class StaticThreadPool {
public:
    explicit StaticThreadPool(int n) : stop_(false) {
        for (int i = 0; i < n; ++i)
            workers_.emplace_back([this] { workerLoop(); });
    }
    ~StaticThreadPool() {
        { std::unique_lock<std::mutex> lk(mu_); stop_ = true; }
        cv_.notify_all();
        for (auto& t : workers_) t.join();
    }

    // Добавление задачи в очередь, возврат future для ожидания результата
    std::future<void> submit(std::function<void()> f) {
        auto task = std::make_shared<std::packaged_task<void()>>(std::move(f));
        auto fut  = task->get_future();
        { std::unique_lock<std::mutex> lk(mu_); q_.push([task]{ (*task)(); }); }
        cv_.notify_one(); 
        return fut;
    }

private:
    void workerLoop() {
        while (true) {
            std::function<void()> fn;
            {
                std::unique_lock<std::mutex> lk(mu_);
                cv_.wait(lk, [this]{ return stop_ || !q_.empty(); });
                if (stop_ && q_.empty()) return;  // завершение работы
                fn = std::move(q_.front()); q_.pop();
            }
            fn(); 
        }
    }
    std::vector<std::thread>          workers_;   // рабочие потоки
    std::queue<std::function<void()>> q_;         // очередь задач
    std::mutex                        mu_;        // мьютекс для очереди
    std::condition_variable           cv_;        // условная переменная
    bool                              stop_;      // флаг завершения
};

// =====================================================================================================
// ObjectStats - результат обработки одного объекта
struct ObjectStats {
    int   label;                       // метка компоненты (глобальный индекс корня Uion-Find)
    int   area;                        // площадь
    cv::Rect bbox;                     // ограничивающий прямоугольник (bound box, bbox)
    cv::Point2f centroid;              // центроид (центр масс, среднее координат)
    std::vector<cv::Point> contour;    // упорядоченный контур
};

// =====================================================================================================
/*
    CudaFindContoursBKE - основной класс общего алгоритма

    Архитектура:
    1. Конструктор: выделение GPU/CPU памяти, создание CUDA events, пул потоков
    2. process(): основной pipeline (GPU kernels + CPU трассировка)
    3. printTiming(): детальная статистика времени по этапам
    4. Деструктор: освобождение всей памяти

    Буферы памяти:
    GPU (device):
        d_brd_         карта граничных пикселей (uchar, N байт)
        d_mask_        карта граничных пикселей (int, N*4 байт, для CUB scan)
        d_scan_        prefix sum d_mask_ (N*4 байт)
        d_coords_      координаты граничных пикселей (N*8 байт)
        d_L_           Union-Find массив меток (N*4 байт)
        d_ol_          метки для граничных пикселей (N*4 байт)
        d_ol_sorted_   отсортированные метки (N*4 байт)
        d_idx_         индексы [0..nBrd) (N*4 байт)
        d_idx_sorted_  отсортированные индексы (N*4 байт)
        d_changed_     флаг изменений для bke_global (4 байт)
        d_cubTmp_      временный буфер CUB

    CPU (pinned memory для быстрого копирования GPU->CPU):
        h_ol_sorted_     отсортированные метки
        h_idx_sorted_    отсортированные индексы
        h_coords_        координаты граничных пикселей
        h_nBrd_buf_      счётчик граничных пикселей
        h_lastMask_buf_  последний элемент mask[]
        h_changed_       флаг изменений bke_global
*/
class CudaFindContoursBKE {
public:
    CudaFindContoursBKE(int W, int H, int nThreads = 0)
        : W_(W), H_(H), N_(W * H),
          pool_(nThreads > 0 ? nThreads : (int)std::thread::hardware_concurrency())
    { alloc(); }

    ~CudaFindContoursBKE() { free_(); }

    /**
     * Основной метод обработки изображения.
     * Выполняет поиск контуров, расчет площадей, bbox и центроидов.
     */
    std::vector<ObjectStats> process(const uchar* d_src,
                                     cudaStream_t stream = 0,
                                     bool need_contours = true)
    {
        auto rec = [&](cudaEvent_t e){ cudaEventRecord(e, stream); };
        rec(ev_[0]);

        /* 
           Этап 1: Детекция границ. 
           Используется shared memory с halo-зоной для проверки 8 соседей. 
        */
        {
            dim3 blk(TILE_W, TILE_H);
            dim3 grd((W_+TILE_W-1)/TILE_W, (H_+TILE_H-1)/TILE_H);
            k_detectBorderAndMask<<<grd, blk, 0, stream>>>(
                d_src, d_brd_, d_mask_, W_, H_);
        }
        rec(ev_[1]);

        /* 
           Этап 2: Префиксное сканирование (CUB). 
           Определение позиции для записи координат каждого граничного пикселя.
        */
        {
            size_t tmp = cubTmpBytes_;
            cub::DeviceScan::ExclusiveSum(d_cubTmp_, tmp,
                                          d_mask_, d_scan_, N_, stream);
        }
        rec(ev_[2]);

        /* 
           Этап 3: Подсчет количества граничных пикселей.
           Копирование результатов сканирования последнего элемента на CPU.
        */
        CHECK_CUDA(cudaMemcpyAsync(h_nBrd_buf_,    d_scan_ + N_ - 1,
                                   sizeof(int), cudaMemcpyDeviceToHost, stream));
        CHECK_CUDA(cudaMemcpyAsync(h_lastMask_buf_, d_mask_ + N_ - 1,
                                   sizeof(int), cudaMemcpyDeviceToHost, stream));
        CHECK_CUDA(cudaStreamSynchronize(stream));
        h_nBrd_ = *h_nBrd_buf_ + *h_lastMask_buf_;

        if (h_nBrd_ == 0) return {};

        /* 
           Этап 4: Сбор координат (Scatter).
           Запись координат (x, y) только для ненулевых пикселей границы.
        */
        {
            int blk = 256, grd = (N_ + blk - 1) / blk;
            k_scatterCoords<<<grd, blk, 0, stream>>>(
                d_brd_, d_scan_, d_coords_, W_, N_);
        }
        rec(ev_[3]);

        /* 
           Этап 5: Локальная инициализация BKE.
           Исполнение Union-Find внутри блоков 16x16 в быстрой shared memory.
        */
        {
            dim3 blk(BKE_TILE, BKE_TILE);
            dim3 grd((W_+BKE_TILE-1)/BKE_TILE, (H_+BKE_TILE-1)/BKE_TILE);
            k_bke_localInit<<<grd, blk, 0, stream>>>(d_brd_, d_L_, W_, H_);
        }
        rec(ev_[4]);

        /* 
           Этап 6: Глобальное слияние (Seam Merging).
           Итеративное объединение компонентовна границах тайлов.
           Цикл продолжается до тех пор, пока происходят изменения (changed=true).
        */
        // Исправление по замечанию №4
        {
            int nTilesX = (W_ + BKE_TILE - 1) / BKE_TILE;
            int nTilesY = (H_ + BKE_TILE - 1) / BKE_TILE;
            int hSeams  = (nTilesY - 1) * W_;
            int vSeams  = (nTilesX - 1) * H_;
            int total   = hSeams + vSeams;
            int blk = 256, grd = (total + blk - 1) / blk;
            const int maxIter = (max(nTilesX, nTilesY) + 1) / 2;

            constexpr int K = 2;  // размер батча (подбирается эмпирически)
            bke_global_iters_ = 0;

            for (int batchStart = 0; batchStart < maxIter; batchStart += K) {
                int batchLen = min(K, maxIter - batchStart);

                // Асинхронный сброс на device  (без host round-trip)
                CHECK_CUDA(cudaMemsetAsync(d_changed_, 0, sizeof(int), stream));

                // K launches подряд, флаг накапливает OR изменений за весь батч
                for (int kk = 0; kk < batchLen; ++kk) {
                    k_bke_global<<<grd, blk, 0, stream>>>(
                        d_brd_, d_L_, d_changed_, W_, H_);
                }

                // Одна проверка на весь батч
                CHECK_CUDA(cudaMemcpyAsync(h_changed_, d_changed_,
                    sizeof(int), cudaMemcpyDeviceToHost, stream));
                CHECK_CUDA(cudaStreamSynchronize(stream));

                bke_global_iters_ += batchLen;
                if (!*h_changed_) break;
            }
        }
        rec(ev_[5]);

         /* 
           Этап 7: Сжатие путей.
           Преобразование дерева Union-Find к плоскому виду - каждый пиксель ссылается сразу на корень.
        */
        {
            int blk = 256, grd = (N_ + blk - 1) / blk;
            k_bke_flatten<<<grd, blk, 0, stream>>>(d_L_, d_brd_, N_);
        }
        rec(ev_[6]);

        /* 
           Этап 8: Сбор меток.
           Сопоставление компактного массива координат с их итоговыми метками компонентов.
        */
        {
            int blk = 256, grd = (h_nBrd_ + blk - 1) / blk;
            k_scatterLabels<<<grd, blk, 0, stream>>>(
                d_L_, d_coords_, d_ol_, h_nBrd_, W_);
        }
        rec(ev_[7]);

        /* 
           Этап 9: Сортировка (Radix Sort).
           Группировка пикселей с одинаковыми метками, чтобы они шли в памяти подряд.
        */
        {
            k_iota<<<(h_nBrd_+255)/256, 256, 0, stream>>>(d_idx_, h_nBrd_);
            size_t tmp = cubTmpBytes_;
            cub::DeviceRadixSort::SortPairs(d_cubTmp_, tmp,
                d_ol_, d_ol_sorted_,
                d_idx_, d_idx_sorted_,
                h_nBrd_, 0, 30, stream);
        }
        rec(ev_[8]);

        /* 
           Этап 10: Передача данных на CPU.
           Копирование только необходимых данных (метки, индексы, координаты).
        */
        CHECK_CUDA(cudaMemcpyAsync(h_ol_sorted_,  d_ol_sorted_,
            h_nBrd_ * sizeof(int),  cudaMemcpyDeviceToHost, stream));
        CHECK_CUDA(cudaMemcpyAsync(h_idx_sorted_, d_idx_sorted_,
            h_nBrd_ * sizeof(int),  cudaMemcpyDeviceToHost, stream));
        CHECK_CUDA(cudaMemcpyAsync(h_coords_,     d_coords_,
            h_nBrd_ * sizeof(int2), cudaMemcpyDeviceToHost, stream));
        CHECK_CUDA(cudaStreamSynchronize(stream));
        rec(ev_[9]);
        
        /* 
           Этап 11: Финальная обработка на CPU.
           Группировка пикселей в объекты и трассировка контуров методом Мура.
        */
        auto tCpu0 = std::chrono::high_resolution_clock::now();
        auto result = cpuGroupAndTrace(need_contours);
        auto tCpu1 = std::chrono::high_resolution_clock::now();
        cpu_trace_ms_ = std::chrono::duration<double,std::milli>(tCpu1-tCpu0).count();

        return result;
    }

    /*
        Вывод отчета о времени выполнения каждого этапа алгоритма.
    */
    void printTiming() const {
        static const char* names[] = {
            "detectBorder+Mask","prefixScan","scatterCoords",
            "bke_localInit","bke_global","bke_flatten",
            "scatterLabels","RadixSort","D2H copy"
        };
        float total = 0;
        std::cout << "\n=== CudaFindContoursBKE timing ====================\n\n";
        for (int i = 0; i < 9; ++i) {
            float ms = 0;
            cudaEventElapsedTime(&ms, ev_[i], ev_[i+1]);
            
            std::cout << "  " << std::left << std::setw(22) << names[i] << " " 
                    << std::fixed << std::setprecision(3) << std::setw(6) << ms << " ms\n";
            total += ms;
        }
        std::cout << "  " << std::left << std::setw(22) << "TOTAL GPU" << " " 
                << std::fixed << std::setprecision(3) << std::setw(6) << total << " ms\n";
        std::cout << "  " << std::left << std::setw(22) << "CPU groupAndTrace" << " " 
                << std::fixed << std::setprecision(3) << std::setw(6) << cpu_trace_ms_ << " ms\n";
        std::cout << "  " << std::left << std::setw(22) << "bke_global iters" << " " 
                << bke_global_iters_ << "\n\n";
    }

private:
     /*
        Группировка отсортированных пикселей в объекты и вычисление их параметров.
        Используется пул потоков для параллельной трассировки контуров.
     */
    // Исправление по замечанию №6
    std::vector<ObjectStats> cpuGroupAndTrace(bool need_contours) {
        // Поиск границ групп с одинаковыми метками в отсортированном массиве
        std::vector<std::pair<int,int>> groups;
        int i = 0;
        while (i < h_nBrd_) {
            int lbl = h_ol_sorted_[i];
            int j = i;
            while (j < h_nBrd_ && h_ol_sorted_[j] == lbl) ++j;
            if (lbl >= 0 ) // && (j - i) > 1
                groups.push_back({i, j});
            i = j;
        }

        int nObj = (int)groups.size();
        std::vector<ObjectStats> result(nObj);
        std::vector<std::future<void>> futs;
        futs.reserve(nObj);

        // Параллельная обработка каждого найденного объекта
        for (int oi = 0; oi < nObj; ++oi) {
            futs.push_back(pool_.submit([&, oi]() {
                auto [gs, ge] = groups[oi];
                ObjectStats& st = result[oi];
                st.label = h_ol_sorted_[gs];

                int nBorderPts = ge - gs;
                int minX = W_, maxX = 0, minY = H_, maxY = 0;
                long long sumX = 0, sumY = 0;

                // Буфер для координат текущего объекта (thread_local для избежания аллокаций)
                thread_local std::vector<int2> tl_objCoords;
                tl_objCoords.resize(nBorderPts);
                
                // Расчет bbox и накопление данных для центроида
                for (int k = gs; k < ge; ++k) {
                    int2 c = h_coords_[h_idx_sorted_[k]];
                    tl_objCoords[k - gs] = c;
                    minX = min(minX, c.x); maxX = max(maxX, c.x);
                    minY = min(minY, c.y); maxY = max(maxY, c.y);
                    sumX += c.x; sumY += c.y;
                }
                st.bbox     = cv::Rect(minX, minY, maxX-minX+1, maxY-minY+1);

                // Трассировка контура и вычисление площади / центроида выполняется опционально
                if (need_contours) {
                    // Построение локальной маски объекта для Moore Tracing
                    int offX = minX - 1, offY = minY - 1;
                    int bW   = maxX - minX + 3;
                    int bH   = maxY - minY + 3;

                    thread_local std::vector<uchar> tl_mask;
                    int maskSz = bW * bH;
                    if ((int)tl_mask.size() < maskSz)
                        tl_mask.resize(maskSz);
                    std::fill(tl_mask.begin(), tl_mask.begin() + maskSz, 0);

                    for (int k = 0; k < nBorderPts; ++k) {
                        int lx = tl_objCoords[k].x - offX;
                        int ly = tl_objCoords[k].y - offY;
                        tl_mask[ly * bW + lx] = 1;
                    }

                    // Стартовая точка
                    int2 startPt = tl_objCoords[0];
                    for (int k = 1; k < nBorderPts; ++k) {
                        int2 c = tl_objCoords[k];
                        if (c.y < startPt.y || (c.y == startPt.y && c.x < startPt.x))
                            startPt = c;
                    }

                    thread_local std::vector<cv::Point> tl_contour;
                    mooreTraceFromCoords(
                        tl_objCoords.data(), nBorderPts,
                        startPt.x, startPt.y,
                        offX, offY,
                        tl_mask.data(), bW, bH,
                        tl_contour);
                    
                    // Вычисление площади и центроида
                    double area;
                    cv::Point2f centroid;
                    computePixelAccurateAreaAndCentroid(tl_contour, area, centroid); // по формуле Пика
                    st.area = (int)std::round(area);
                    st.centroid = centroid;
                    st.contour = tl_contour;
                }
            }));
        }

         // Ожидание завершения всех потоков
        for (auto& f : futs) f.get();

        return result;
    }

    // Выделение памяти на GPU и CPU (Pinned)
    void alloc() {
        CHECK_CUDA(cudaMalloc(&d_brd_,       N_));
        CHECK_CUDA(cudaMalloc(&d_mask_,      N_ * sizeof(int)));
        CHECK_CUDA(cudaMalloc(&d_scan_,      N_ * sizeof(int)));
        CHECK_CUDA(cudaMalloc(&d_coords_,    N_ * sizeof(int2)));
        CHECK_CUDA(cudaMalloc(&d_L_,         N_ * sizeof(int)));
        CHECK_CUDA(cudaMalloc(&d_ol_,        N_ * sizeof(int)));
        CHECK_CUDA(cudaMalloc(&d_ol_sorted_, N_ * sizeof(int)));
        CHECK_CUDA(cudaMalloc(&d_idx_,       N_ * sizeof(int)));
        CHECK_CUDA(cudaMalloc(&d_idx_sorted_,N_ * sizeof(int)));
        CHECK_CUDA(cudaMalloc(&d_changed_,   sizeof(int)));

        // Определение размера временного буфера CUB
        cubTmpBytes_ = 0;
        cub::DeviceScan::ExclusiveSum(nullptr, cubTmpBytes_, d_mask_, d_scan_, N_);
        size_t sortTmp = 0;
        cub::DeviceRadixSort::SortPairs(nullptr, sortTmp,
            d_ol_, d_ol_sorted_, d_idx_, d_idx_sorted_, N_);
        cubTmpBytes_ = max(cubTmpBytes_, sortTmp);
        CHECK_CUDA(cudaMalloc(&d_cubTmp_, cubTmpBytes_));
        
        // Выделение Pinned-памяти для ускоренного DMA
        CHECK_CUDA(cudaMallocHost(&h_ol_sorted_,   N_ * sizeof(int)));
        CHECK_CUDA(cudaMallocHost(&h_idx_sorted_,  N_ * sizeof(int)));
        CHECK_CUDA(cudaMallocHost(&h_coords_,      N_ * sizeof(int2)));
        CHECK_CUDA(cudaMallocHost(&h_nBrd_buf_,    sizeof(int)));
        CHECK_CUDA(cudaMallocHost(&h_lastMask_buf_,sizeof(int)));
        CHECK_CUDA(cudaMallocHost(&h_changed_,     sizeof(int)));

        for (auto& e : ev_) cudaEventCreate(&e);
    }

    //  Освобождение всех выделенных ресурсов
    void free_() {
        cudaFree(d_brd_); cudaFree(d_mask_); cudaFree(d_scan_);
        cudaFree(d_coords_); cudaFree(d_L_);
        cudaFree(d_ol_); cudaFree(d_ol_sorted_);
        cudaFree(d_idx_); cudaFree(d_idx_sorted_);
        cudaFree(d_changed_); cudaFree(d_cubTmp_);
        cudaFreeHost(h_ol_sorted_); cudaFreeHost(h_idx_sorted_);
        cudaFreeHost(h_coords_);
        cudaFreeHost(h_nBrd_buf_); cudaFreeHost(h_lastMask_buf_);
        cudaFreeHost(h_changed_);
        for (auto& e : ev_) cudaEventDestroy(e);
    }

    int W_, H_, N_;
    int h_nBrd_ = 0;

    // Указатели на память GPU
    uchar* d_brd_         = nullptr;
    int*   d_mask_        = nullptr;
    int*   d_scan_        = nullptr;
    int2*  d_coords_      = nullptr;
    int*   d_L_           = nullptr;
    int*   d_ol_          = nullptr;
    int*   d_ol_sorted_   = nullptr;
    int*   d_idx_         = nullptr;
    int*   d_idx_sorted_  = nullptr;
    int*   d_changed_     = nullptr;
    void*  d_cubTmp_      = nullptr;
    size_t cubTmpBytes_   = 0;

    // Указатели на Pinned-память CPU
    int*   h_ol_sorted_    = nullptr;
    int*   h_idx_sorted_   = nullptr;
    int2*  h_coords_       = nullptr;
    int*   h_nBrd_buf_     = nullptr;
    int*   h_lastMask_buf_ = nullptr;
    int*   h_changed_      = nullptr;

    cudaEvent_t ev_[10];
    mutable StaticThreadPool pool_;
    mutable double cpu_trace_ms_   = 0;
    mutable int    bke_global_iters_ = 0;
};

// =====================================================================================================
// Главная функция
int main(int argc, char** argv) {
    // Получение пути к файлу с изображением из строки с аргументами
    const std::string path = (argc > 1) ? argv[1] : "../data/raw/fc_test-01.png";

    // Проверка инициализации CUDA
    int deviceCount = 0;
    cudaGetDeviceCount(&deviceCount);
    std::cout << "CUDA devices: " << deviceCount << "\n";
    cudaDeviceProp prop;
    cudaGetDeviceProperties(&prop, 0);
    std::cout << "GPU: " << prop.name
            << "  compute: " << prop.major << "." << prop.minor << "\n";

    cudaFree(0);
    cudaError_t initErr = cudaGetLastError();
    std::cout << "CUDA init: " << cudaGetErrorString(initErr) << "\n\n";

    // Чтение и бинаризация изображения 
    cv::Mat src = cv::imread(path, cv::IMREAD_GRAYSCALE);
    if (src.empty()) {
        std::cerr << "Can't load image: " << path << std::endl;
        return 1;
    }
    cv::threshold(src, src, 127, 255, cv::THRESH_BINARY_INV);

    // Выделение памяти для изображения на GPU 
    uchar* d_src = nullptr;
    CHECK_CUDA(cudaMalloc(&d_src, src.total()));
    // Копирование изображения из оперативной памяти на GPU
    CHECK_CUDA(cudaMemcpy(d_src, src.data, src.total(), cudaMemcpyHostToDevice));

    // Прогрев GPU - предварительный однократный вызов алгоритма
    {
        CudaFindContoursBKE finder(src.cols, src.rows);
        finder.process(d_src, 0, true);
    }

    // Проведение замера производительности алгоритма CudaFindContoursBKE и стандартного OpenCV findContours
    CudaFindContoursBKE finder(src.cols, src.rows);

    constexpr int RUNS = 10; // Кол-во итераций
    double gpu_total_ms = 0, cpu_total_ms = 0; // Время выполнения 
    std::vector<ObjectStats> gpu_result; // Результаты алгоритма CudaFindContoursBKE
    std::vector<std::vector<cv::Point>> cpu_contours; // Результаты OpenCV findContours - вектор с контурами
    std::vector<cv::Vec4i> hier; // Результаты OpenCV findContours - информация об иерархии контуров

    for (int r = 0; r < RUNS; ++r) {
        auto t0 = std::chrono::high_resolution_clock::now();
        gpu_result = finder.process(d_src, 0, true);
        auto t1 = std::chrono::high_resolution_clock::now();
        cv::findContours(src, cpu_contours, hier, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);
        auto t2 = std::chrono::high_resolution_clock::now();
        gpu_total_ms += std::chrono::duration<double,std::milli>(t1-t0).count();
        cpu_total_ms += std::chrono::duration<double,std::milli>(t2-t1).count();
    }

    //  Вывод результатов замера производительности
    std::cout << "=== FindContours algorithms timing test ==============\n\n";
    std::cout << "Image: " << src.cols << "x" << src.rows << "\n\n";
    std::cout << "GPU (CudaFindContoursBKE):\n";
    std::cout << "  Objects found: " << static_cast<int>(gpu_result.size()) << "\n";
    std::cout << "  Avg time (" << RUNS << " runs): " 
            << std::fixed << std::setprecision(3) << (gpu_total_ms / RUNS) << " ms\n\n";

    std::cout << "CPU (cv::findContours):\n";
    std::cout << "  Objects found: " << static_cast<int>(cpu_contours.size()) << "\n";
    std::cout << "  Avg time (" << RUNS << " runs): " 
            << std::fixed << std::setprecision(3) << (cpu_total_ms / RUNS) << " ms\n\n";
    
    // Вывод подробной информации о времени выполнения каждого этапа алгоритма CudaFindContoursBKE
    finder.printTiming();

    // Вывод информации о найденных объектах
    std::cout << "=== CudaFindContoursBKE result=======================\n\n";
    for (auto& obj : gpu_result) {
        std::cout << "  [" << std::setw(7) << obj.label << "] "
          << "area=" << std::setw(5) << obj.area << "  "
          << "bbox=(" << std::setw(4) << obj.bbox.x << "," 
          << std::setw(4) << obj.bbox.y << "," 
          << std::setw(4) << obj.bbox.width << "," 
          << std::setw(4) << obj.bbox.height << ")  "
          << "centroid=(" << std::fixed << std::setprecision(1) << obj.centroid.x << "," 
          << obj.centroid.y << ")  "
          << "contour=" << static_cast<int>(obj.contour.size()) << " pts\n";
    }
    std::cout << "\n====================================================\n";

    // Отрисовка контуров на изображении и запись результата в файл
    cv::Mat vis;
    cv::cvtColor(src, vis, cv::COLOR_GRAY2BGR);
    for (auto& obj : gpu_result) {
        if (!obj.contour.empty()) {
            std::vector<std::vector<cv::Point>> c = {obj.contour};
            cv::drawContours(vis, c, 0, {0,255,0}, 2);
        }
        cv::rectangle(vis, obj.bbox, {0,0,255}, 1);
    }
    cv::imwrite("../data/output/fc_bke_gpu_result.png", vis);
    std::cout << "\nResult saved to data/output/fc_bke_gpu_result.png\n" << std::endl;

    // Освобождение памяти для изображения на GPU
    cudaFree(d_src); 

    return 0;
}
