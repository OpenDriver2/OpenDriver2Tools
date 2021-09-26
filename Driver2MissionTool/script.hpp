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

void initStack(Stack* stack);
void addThread(Stack* stack, Thread* thread);
void init_thread(Thread* thread, uint size);
void push_thread(Thread* thread, uint op_code);
uint pop_thread(Thread* thread);
void print_thread(Thread thread);
void printStack(Stack stack);
void processThreads(Stack* stack);
