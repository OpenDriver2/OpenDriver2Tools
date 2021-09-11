#pragma once

#include <nstd/Base.hpp>

typedef struct Thread
{
    uint* operations; // All the op code
    uint offset; // Size of the op code (last element)
} Thread;

typedef struct Stack
{
    uint* operations; // Concatenation of all operations threads
    uint* thread_offsets; // Address(index) where each thread start (16 max)
    uint nbThreads; // >= 1 (max.16)
    uint nbOperations; // Number of operations (sum of each nbOperations of all Thread)
} Stack;