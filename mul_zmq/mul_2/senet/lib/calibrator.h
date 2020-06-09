/**
MIT License

Copyright (c) 2018 NVIDIA CORPORATION. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*
*/
#ifndef _CALIBRATOR_H_
#define _CALIBRATOR_H_

#include "NvInfer.h"
#include "ds_image.h"
#include "trt_utils.h"

#define NV_CUDA_CHECK(status)                                                                      \
    {                                                                                              \
        if (status != 0)                                                                           \
        {                                                                                          \
            std::cout << "Cuda failure: " << cudaGetErrorString(status) << " in file " << __FILE__ \
                      << " at line " << __LINE__ << std::endl;                                     \
            abort();                                                                               \
        }                                                                                          \
    }

class Int8EntropyCalibrator : public nvinfer1::IInt8EntropyCalibrator
{
public:
    Int8EntropyCalibrator(const uint& batchSize, const std::string& calibImagesFileDir,
                          const std::string& calibTableFilePath, const uint64_t& inputSize,
                          const uint& inputH, const uint& inputW, const std::string& inputBlobName);
    virtual ~Int8EntropyCalibrator() { NV_CUDA_CHECK(cudaFree(m_DeviceInput)); }

    int getBatchSize() const override { return m_BatchSize; }
    bool getBatch(void* bindings[], const char* names[], int nbBindings) override;
    const void* readCalibrationCache(size_t& length) override;
    void writeCalibrationCache(const void* cache, size_t length) override;

private:
    const uint m_BatchSize;
    const uint m_InputH;
    const uint m_InputW;
    const uint64_t m_InputSize;
    const uint64_t m_InputCount;
    const std::string m_InputBlobName;
    const std::string m_CalibImagesFileDir;
    const std::string m_CalibTableFilePath;
    uint m_ImageIndex;
    bool m_ReadCache{true};
    void* m_DeviceInput{nullptr};
    std::vector<pair<std::string, uint32_t>> m_ImageList;
    std::vector<char> m_CalibrationCache;
};

#endif