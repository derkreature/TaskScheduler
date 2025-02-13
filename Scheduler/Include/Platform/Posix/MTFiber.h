// The MIT License (MIT)
//
// 	Copyright (c) 2015 Sergey Makeev, Vadim Slyusarev
//
// 	Permission is hereby granted, free of charge, to any person obtaining a copy
// 	of this software and associated documentation files (the "Software"), to deal
// 	in the Software without restriction, including without limitation the rights
// 	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// 	copies of the Software, and to permit persons to whom the Software is
// 	furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
// 	all copies or substantial portions of the Software.
//
// 	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// 	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// 	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// 	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// 	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// 	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// 	THE SOFTWARE.

#pragma once

#ifndef __MT_FIBER__
#define __MT_FIBER__


#include <ucontext.h>
#include <stdlib.h>
#include <string.h>

#define _DARWIN_C_SOURCE
#include <sys/mman.h>

#ifndef MAP_ANONYMOUS
    #define MAP_ANONYMOUS MAP_ANON
#endif

#ifndef MAP_STACK
    #define MAP_STACK (0)
#endif

#include <MTAllocator.h>
#include "MTAtomic.h"

namespace MT
{

	//
	//
	//
	class Fiber
	{
		void* funcData;
		TThreadEntryPoint func;

		Memory::StackDesc stackDesc;

		ucontext_t fiberContext;
		bool isInitialized;

		static void FiberFuncInternal(void* pFiber)
		{
			MT_ASSERT(pFiber != nullptr, "Invalid fiber");
			Fiber* self = (Fiber*)pFiber;

			MT_ASSERT(self->isInitialized == true, "Using non initialized fiber");

			MT_ASSERT(self->func != nullptr, "Invalid fiber func");
			self->func(self->funcData);
		}

	public:

		MT_NOCOPYABLE(Fiber);

		Fiber()
			: funcData(nullptr)
			, func(nullptr)
			, isInitialized(false)
		{
			memset(&fiberContext, 0, sizeof(ucontext_t));
		}

		~Fiber()
		{
			if (isInitialized)
			{
				// if func != null than we have stack memory ownership
				if (func != nullptr)
				{
					Memory::FreeStack(stackDesc);
				}

				isInitialized = false;
			}
		}


		void CreateFromThread(Thread & thread)
		{
			MT_ASSERT(!isInitialized, "Already initialized");
			MT_ASSERT(thread.IsCurrentThread(), "ERROR: Can create fiber only from current thread!");

			int res = getcontext(&fiberContext);
			MT_USED_IN_ASSERT(res);
			MT_ASSERT(res == 0, "getcontext - failed");

			fiberContext.uc_link = nullptr;
			fiberContext.uc_stack.ss_sp = thread.GetStackBottom();
			fiberContext.uc_stack.ss_size = thread.GetStackSize();
			fiberContext.uc_stack.ss_flags = 0;

			func = nullptr;
			funcData = nullptr;

			isInitialized = true;
		}


		void Create(size_t stackSize, TThreadEntryPoint entryPoint, void *userData)
		{
			MT_ASSERT(!isInitialized, "Already initialized");
			MT_ASSERT(stackSize >= PTHREAD_STACK_MIN, "Stack to small");

			func = entryPoint;
			funcData = userData;

			int res = getcontext(&fiberContext);
			MT_USED_IN_ASSERT(res);
			MT_ASSERT(res == 0, "getcontext - failed");

			stackDesc = Memory::AllocStack(stackSize);

			fiberContext.uc_link = nullptr;
			fiberContext.uc_stack.ss_sp = stackDesc.stackBottom;
			fiberContext.uc_stack.ss_size = stackDesc.GetStackSize();
			fiberContext.uc_stack.ss_flags = 0;

			makecontext(&fiberContext, (void(*)())&FiberFuncInternal, 1, (void *)this);

			isInitialized = true;
		}

		static void SwitchTo(Fiber & from, Fiber & to)
		{
			HardwareFullMemoryBarrier();

			MT_ASSERT(from.isInitialized, "Invalid from fiber");
			MT_ASSERT(to.isInitialized, "Invalid to fiber");

			int res = swapcontext(&from.fiberContext, &to.fiberContext);
			MT_USED_IN_ASSERT(res);
			MT_ASSERT(res == 0, "setcontext - failed");

		}



	};


}


#endif