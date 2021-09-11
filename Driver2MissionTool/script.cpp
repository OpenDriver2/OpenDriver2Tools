#include "script.hpp"

#include <cstdio>
#include <cstdlib>
#include "mission.hpp"

void initStack(Stack* stack)
{
    stack->nbThreads = 0;
    stack->nbOperations = 0;
    stack->operations = (uint*)malloc(sizeof(uint) * 128);
    stack->thread_offsets = (uint*)malloc(sizeof(uint) * 16);

    for (int i = 0; i < 128; i++)
    {
        stack->operations[i] = 0;
    }
}

void addThread(Stack* stack, Thread* thread)
{
    stack->thread_offsets[stack->nbThreads] = stack->nbOperations;
    stack->nbThreads++;
    for (int i = stack->nbOperations; i < (stack->nbOperations + thread->offset); i++)
    {
        stack->operations[i] = thread->operations[i - stack->nbOperations];
    }
    stack->nbOperations += thread->offset;
}

void init(Thread* thread, uint size)
{
    thread->operations = (uint*)malloc(sizeof(uint) * size);
    thread->offset = 0;

    for (uint i = 0; i < size; i++)
    {
        thread->operations[i] = 0;
    }
}

void push(Thread* thread, uint op_code)
{
    thread->operations[thread->offset++] = op_code;
}

uint pop(Thread* thread)
{
    uint popValue = thread->operations[thread->offset];
    thread->operations[thread->offset] = 0;
    thread->offset--;

    return popValue;
}

void print(Thread thread)
{
    uint* operations = thread.operations;
    for (int i = 0; i < thread.offset; i++)
    {
        uint addr = (operations + i) - operations;
        printf("(%u): %u\n", addr, operations[i]);
    }

}

void printStack(Stack stack)
{
    for (int i = 0; i < stack.nbOperations; i++)
    {
        uint addr = (stack.operations + i) - stack.operations;
        printf("(%u): 0x%x\n", addr, stack.operations[i]);
    }
}

void processThreads(Stack* stack)
{
    for (int i = 0; i < stack->nbOperations; i++)
    {
        // Check if this is a thread call
        if (stack->operations[i] == CMD_StartThreadForPlayer || stack->operations[i] == CMD_StartThread2)
        {
            // If yes we want to replace the threadId by the offset where it's in the stack
            uint threadId = stack->operations[i - 1];
            stack->operations[i - 1] = stack->thread_offsets[threadId] - i + 1;
        }
    }
}
