#include "Tests.h"
#include <UnitTest++.h>
#include <MTScheduler.h>
#include <MTConcurrentQueueLIFO.h>
#include <MTConcurrentRingBuffer.h>
#include <MTArrayView.h>
#include <MTStackArray.h>

SUITE(FoundationTests)
{

TEST(QueueTest)
{
	MT::ConcurrentQueueLIFO<int> lifoQueue;

	lifoQueue.Push(1);
	lifoQueue.Push(3);
	lifoQueue.Push(7);
	lifoQueue.Push(10);
	lifoQueue.Push(13);

	int res = 0;

	CHECK(lifoQueue.TryPopBack(res) == true);
	CHECK_EQUAL(res, 13);

	CHECK(lifoQueue.TryPopBack(res) == true);
	CHECK_EQUAL(res, 10);

	CHECK(lifoQueue.TryPopBack(res) == true);
	CHECK_EQUAL(res, 7);

	CHECK(lifoQueue.TryPopBack(res) == true);
	CHECK_EQUAL(res, 3);

	CHECK(lifoQueue.TryPopBack(res) == true);
	CHECK_EQUAL(res, 1);

	CHECK(lifoQueue.TryPopBack(res) == false);

	lifoQueue.Push(4);

	CHECK(lifoQueue.TryPopBack(res) == true);
	CHECK_EQUAL(res, 4);

	CHECK(lifoQueue.TryPopBack(res) == false);

	CHECK(lifoQueue.IsEmpty() == true);

	lifoQueue.Push(101);
	lifoQueue.Push(103);
	lifoQueue.Push(107);
	lifoQueue.Push(1010);
	lifoQueue.Push(1013);

	CHECK(lifoQueue.IsEmpty() == false);


	CHECK(lifoQueue.TryPopFront(res) == true);
	CHECK_EQUAL(res, 101);

	CHECK(lifoQueue.TryPopFront(res) == true);
	CHECK_EQUAL(res, 103);


	int tempData[16];
	size_t elementsCount = lifoQueue.PopAll(tempData, MT_ARRAY_SIZE(tempData));
	CHECK_EQUAL(elementsCount, (size_t)3);

	CHECK_EQUAL(tempData[0], 107);
	CHECK_EQUAL(tempData[1], 1010);
	CHECK_EQUAL(tempData[2], 1013);

}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TEST(RingBufferTest)
{
	MT::ConcurrentRingBuffer<int, 32> ringBuffer;

	ringBuffer.Push(-1);
	ringBuffer.Push(1);

	int tempData[32];
	size_t elementsCount = ringBuffer.PopAll(tempData, MT_ARRAY_SIZE(tempData));
	CHECK_EQUAL(elementsCount, (size_t)2);

	CHECK_EQUAL(tempData[0], -1);
	CHECK_EQUAL(tempData[1], 1);

	int j;
	for(j = 0; j < 507; j++)
	{
		ringBuffer.Push(3 + j);
	}

	elementsCount = ringBuffer.PopAll(tempData, MT_ARRAY_SIZE(tempData));
	CHECK_EQUAL(elementsCount, (size_t)32);

	size_t i;
	for(i = 0; i < elementsCount; i++)
	{
		CHECK_EQUAL(tempData[i], (int)((507+3-32) + i));
	}

}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TEST(StackArrayTest)
{
	const int elementsCount = 128;

	MT::StackArray<int, elementsCount> stackArray;

	CHECK(stackArray.IsEmpty() == true);

	stackArray.PushBack(200);
	CHECK(stackArray.IsEmpty() == false);
	CHECK_EQUAL(stackArray.Size(), (size_t)1);

	for(int i = 1; i < elementsCount; i++)
	{
		stackArray.PushBack(200 + i);
	}

	CHECK(stackArray.IsEmpty() == false);
	CHECK_EQUAL(stackArray.Size(), (size_t)elementsCount);

	for(int i = 0; i < elementsCount; i++)
	{
		CHECK_EQUAL(stackArray[i], (200 + i));
	}

}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TEST(ArrayViewTest)
{

	MT::ArrayView<int> emptyArrayView(nullptr, 0);
	CHECK(emptyArrayView.IsEmpty() == true);

	const int elementsCount = 128;
	void * rawMemory = malloc(sizeof(int) * elementsCount);

	MT::ArrayView<int> arrayView(rawMemory, elementsCount);
	CHECK(arrayView.IsEmpty() == false);

	for (int i = 0; i < elementsCount; i++)
	{
		arrayView[i] = (100 + i);
	}
	
	const int* buffer = static_cast<const int*>(rawMemory);
	for (int i = 0; i < elementsCount; i++)
	{
		CHECK_EQUAL(buffer[i], arrayView[i]);
	}

	free(rawMemory);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

}
