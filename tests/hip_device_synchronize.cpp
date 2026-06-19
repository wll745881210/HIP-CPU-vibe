/* -----------------------------------------------------------------------------
 * Copyright (c) 2020 Advanced Micro Devices, Inc. All Rights Reserved.
 * See 'LICENSE' in the project root for license information.
 * -------------------------------------------------------------------------- */
#include <hip/hip_runtime.h>

#include "../external/catch2/catch.hpp"

constexpr auto byte_cnt{sizeof(int) * 1024 * 1024};
constexpr auto stream_cnt{2};

__global__
void Iter(int* Ad, int num)
{
    int tx = threadIdx.x + blockIdx.x * blockDim.x;

    if (tx) return;

    // Kernel loop designed to execute very slowly... ... ...   so we can test
    // timing-related behaviour below
    for (int i = 0; i != num; ++i) ++Ad[tx];
}

TEST_CASE("hipDeviceSynchronize()", "[host][hipDevice]")
{
    int* A[stream_cnt];
    int* Ad[stream_cnt];
    hipStream_t stream[stream_cnt];

    for (auto i = 0; i != stream_cnt; ++i) {
        REQUIRE(
            hipHostMalloc(&A[i], byte_cnt, hipHostMallocDefault) == hipSuccess);

        A[i][0] = 1;

        REQUIRE(hipMalloc(&Ad[i], byte_cnt) == hipSuccess);
        REQUIRE(hipStreamCreate(&stream[i]) == hipSuccess);
    }

    for (auto i = 0; i != stream_cnt; ++i) {
        REQUIRE(hipMemcpyAsync(
            Ad[i], A[i], byte_cnt, hipMemcpyHostToDevice, stream[i]) ==
            hipSuccess);
    }
    for (auto i = 0; i != stream_cnt; ++i) {
        hipLaunchKernelGGL(
            Iter, dim3(1), dim3(1), 0, stream[i], Ad[i], 1 << 30);
    }
    for (auto i = 0; i != stream_cnt; ++i) {
        REQUIRE(hipMemcpyAsync(
            A[i], Ad[i], byte_cnt, hipMemcpyDeviceToHost, stream[i]) ==
            hipSuccess);
    }

    // This first check but relies on the kernel running for so long that the
    // D2H async memcopy has not started yet. This will be true in an optimal
    // asynchronous implementation. Conservative implementations which
    // synchronize the hipMemcpyAsync will fail.
    REQUIRE(A[stream_cnt - 1][0] - 1 != 1 << 30);
    REQUIRE(hipDeviceSynchronize() == hipSuccess);
    REQUIRE(A[stream_cnt - 1][0] - 1 == 1 << 30);
}

__global__
void Vadd_Float(const float* a, const float* b, float* c, int N)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < N) c[idx] = a[idx] + b[idx];
}

TEST_CASE("hipKernel() launch syntax", "[host][hipDevice][launch_syntax]")
{
    constexpr auto N{1024};
    float *a_d{}, *b_d{}, *c_d{};
    float a_h[N], b_h[N], c_h[N];

    for (auto i = 0; i != N; ++i) {
        a_h[i] = static_cast<float>(i);
        b_h[i] = static_cast<float>(i * 2);
    }

    REQUIRE(hipMalloc(&a_d, N * sizeof(float)) == hipSuccess);
    REQUIRE(hipMalloc(&b_d, N * sizeof(float)) == hipSuccess);
    REQUIRE(hipMalloc(&c_d, N * sizeof(float)) == hipSuccess);

    REQUIRE(hipMemcpy(
        a_d, a_h, N * sizeof(float), hipMemcpyHostToDevice) == hipSuccess);
    REQUIRE(hipMemcpy(
        b_d, b_h, N * sizeof(float), hipMemcpyHostToDevice) == hipSuccess);

    hipKernel(Vadd_Float, dim3(N / 256), dim3(256))(a_d, b_d, c_d, N);

    REQUIRE(hipDeviceSynchronize() == hipSuccess);

    REQUIRE(hipMemcpy(
        c_h, c_d, N * sizeof(float), hipMemcpyDeviceToHost) == hipSuccess);

    for (auto i = 0; i != N; ++i) {
        REQUIRE(c_h[i] == a_h[i] + b_h[i]);
    }

    REQUIRE(hipFree(a_d) == hipSuccess);
    REQUIRE(hipFree(b_d) == hipSuccess);
    REQUIRE(hipFree(c_d) == hipSuccess);
}