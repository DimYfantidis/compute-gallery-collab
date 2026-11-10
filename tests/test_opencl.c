// C standard includes
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <locale.h>
#include <inttypes.h>

// OpenCL includes
#include <CL/cl.h>

#include <common.h>


// OpenCL hello world program that calculates sin(x) in parallel.
int main(int argc, char *argv[])
{
    // Initialize system.
    srand(time(NULL));
    setlocale(LC_NUMERIC, "");

    // Initialize benchmarking variables:
    double begin;
    double end;
    double elapsed_kernel;
    double elapsed_setup;
    double elapsed_native;

    // Initialize our data
    const size_t LENGTH = 2560 * 1440 * 3;
    const size_t DATA_SIZE = sizeof(cl_float) * LENGTH;
    cl_float *const data = fetch_random_data(LENGTH, -CL_M_PI_F, CL_M_PI_F);
    cl_float *v1 = array_copy(data, LENGTH);

    // Setup OpenCL
    cl_device_id device = select_device();

    // Setup Device
    cl_context context = clCreateContext(NULL, 1, &device, NULL, NULL, NULL);
    cl_queue_properties queue_properties[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    cl_command_queue queue = clCreateCommandQueueWithProperties(context, device, queue_properties, NULL);

    // Define our kernel. This is the function that calculates the sin of the input data in parallel.
    const char *source =
        "__kernel void expensiveFunction(__global const float * restrict input,\n"
        "                                __global float * restrict output) {\n"
        "    size_t id = get_global_id(0);\n"
        "    output[id] = atanh(sin(input[id]) * cos(input[id])) + tanh(input[id]);\n"
        "}\n";

    // Compile the program (i.e. the collection of kernels).
    cl_program program = clCreateProgramWithSource(context, 1, (const char **)&source, NULL, NULL);
    clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
    cl_kernel kernel = clCreateKernel(program, "expensiveFunction", NULL);

    printf("\nDispatching Kernel...\n");
    begin = read_time_sec();

    // Create the memory object.
    cl_mem input_buffer = clCreateBuffer(context, CL_MEM_READ_ONLY, DATA_SIZE, NULL, NULL);
    cl_mem output_buffer = clCreateBuffer(context, CL_MEM_WRITE_ONLY, DATA_SIZE, NULL, NULL);

    size_t global_dimensions[] = {LENGTH, 0, 0};
    {
        cl_event event;

        // Copy the data to the input.
        clEnqueueWriteBuffer(queue, input_buffer, CL_FALSE, 0, DATA_SIZE, v1, 0, NULL, &event);

        // Execute the kernel
        clSetKernelArg(kernel, 0, sizeof(cl_mem), &input_buffer);
        clSetKernelArg(kernel, 1, sizeof(cl_mem), &output_buffer);

        clEnqueueNDRangeKernel(queue, kernel, 1, NULL, global_dimensions, NULL, 0, NULL, &event);
        clWaitForEvents(1, &event);

        cl_ulong time_start;
        cl_ulong time_end;
        clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, NULL);
        clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(time_end), &time_end, NULL);

        // Read back the results.
        clEnqueueReadBuffer(queue, output_buffer, CL_FALSE, 0, DATA_SIZE, v1, 0, NULL, &event);

        elapsed_kernel = (time_end - time_start) * 1e-9;
        printf("> [WARM] Kernel exution finshed after: %.5lf sec \n", elapsed_kernel);

        clReleaseEvent(event);
        event = NULL;
    }
    // Wait for everything to finish.
    clFinish(queue);

    end = read_time_sec();
    elapsed_setup = end - begin;
    printf("> [COLD] Finished after %.5lf sec\n", elapsed_setup);

    // Perform the same computation without OpenCL
    float *v2 = array_copy(data, LENGTH);

    printf("\nComputing locally...\n");

    begin = read_time_sec();

    for (size_t i = 0; i < LENGTH; ++i)
    {
        float x = v2[i];
        v2[i] = atanhf(sinf(x) * cosf(x)) + tanhf(x);
    }

    end = read_time_sec();
    elapsed_native = end - begin;
    printf("> [NATIVE] Finished after %.4lf sec\n\n", elapsed_native);

    assert_equals(v1, v2, LENGTH, 1e-6);

    printf("Success:       All %'zu elements at %p and %p are equal\n", LENGTH, v1, v2);
    printf("Processed:     %'3lf MiB\n", (double)DATA_SIZE / (1024 * 1024));
    printf("Speedup:       %+'.2lf%%\n", 100 * (elapsed_native - elapsed_kernel) / elapsed_kernel);
    printf("Prep Overhead: %'.5lf sec\n\n", elapsed_setup - elapsed_kernel);

    // -------------------------------------------------------------------------
    // Cleanup
    // -------------------------------------------------------------------------

    /*
     * At this point clEnqueueReadBuffer(..., CL_TRUE, ...) guaranteed that
     * all preceding commands in this in-order queue completed.
     *
     * clFinish() is therefore technically redundant, but leaving it here
     * makes the lifetime requirement explicit and protects this cleanup if
     * the read is later changed back to CL_FALSE.
     */
    clFinish(queue);

    /*
     * Release OpenCL resources roughly in reverse creation/dependency order.
     */

    clReleaseMemObject(output_buffer);
    clReleaseMemObject(input_buffer);

    clReleaseKernel(kernel);
    clReleaseProgram(program);

    clReleaseCommandQueue(queue);
    clReleaseContext(context);

    /*
     * Do not call clReleaseDevice(device) here for a normal root device
     * obtained from clGetDeviceIDs().
     */

    /*
     * Finally release ordinary host data.
     */
    free(v2);
    free(v1);
    free(data);

    return EXIT_SUCCESS;
}