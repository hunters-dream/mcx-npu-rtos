#include "npu_task.hpp"
#include "board_init.h"
#include "model.h"
#include "image.h"
#include "output_postproc.h"
#include "timer.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/kernels/neutron/neutron.h"
#include "model_data.h"

static uint8_t tensor_arena[TENSOR_ARENA_SIZE];

static tensor_dims_t inputDims;
static tensor_type_t inputType;
static tensor_dims_t outputDims;
static tensor_type_t outputType;
static uint8_t *inputData  = nullptr;
static uint8_t *outputData = nullptr;

void Inference_Init(void)
{
    TIMER_Init();

    if (MODEL_Init() != kStatus_Success)
    {
        for (;;) {}
    }

    inputData  = MODEL_GetInputTensorData(&inputDims, &inputType);
    outputData = MODEL_GetOutputTensorData(&outputDims, &outputType);
}

void Inference_Run(void)
{
    IMAGE_GetImage(inputData, inputDims.data[2], inputDims.data[1], inputDims.data[3]);
    MODEL_ConvertInput(inputData, &inputDims, inputType);

    auto startTime = TIMER_GetTimeInUS();
    MODEL_RunInference();
    auto endTime = TIMER_GetTimeInUS();

    MODEL_ProcessOutput(outputData, &outputDims, outputType, endTime - startTime);
}

void npu_task(void)
{
    Inference_Init();

    while (1)
    {
        Inference_Run();
    }
}
