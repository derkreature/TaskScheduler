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

#ifndef __MT_FIBER_OPTIMIZED__
#define __MT_FIBER_OPTIMIZED__


#include <MTAllocator.h>
#include "MTAtomic.h"

#if defined(_X86_)

#define ReadTeb(offset) __readfsdword(offset);
#define WriteTeb(offset, v) __writefsdword(offset, v)

#else

#define ReadTeb(offset) __readgsqword(offset);
#define WriteTeb(offset, v) __writegsqword(offset, v)

#endif



namespace MT
{

	//
	// Windows fiber implementation through GetThreadContext / SetThreadContext
	// I don't use standard Windows Fibers since they are wasteful use of Virtual Memory space for the stack. ( 1Mb for each Fiber )
	//
	class Fiber
	{
		void* funcData;
		TThreadEntryPoint func;

		Memory::StackDesc stackDesc;

		CONTEXT fiberContext;
		bool isInitialized;

#if defined(_X86_)
	// https://en.wikipedia.org/wiki/X86_calling_conventions#stdcall
	// The stdcall calling convention is a variation on the Pascal calling convention in which the callee is responsible for cleaning up the stack,
	// but the parameters are pushed onto the stack in right-to-left order, as in the _cdecl calling convention.
	static void __stdcall FiberFuncInternal(void *pFiber)
#else
	// https://en.wikipedia.org/wiki/X86_calling_conventions#Microsoft_x64_calling_convention
	// The Microsoft x64 calling convention is followed on Microsoft Windows.
  // It uses registers RCX, RDX, R8, R9 for the first four integer or pointer arguments (in that order), and XMM0, XMM1, XMM2, XMM3 are used for floating point arguments.
	
	// Additional arguments are pushed onto the stack (right to left). 
	static void __stdcall FiberFuncInternal(long /*ecx*/, long /*edx*/, long /*r8*/, long /*r9*/, void *pFiber)
#endif
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
			memset(&fiberContext, 0, sizeof(CONTEXT));
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
			MT_USED_IN_ASSERT(thread);

			MT_ASSERT(!isInitialized, "Already initialized");
			MT_ASSERT(thread.IsCurrentThread(), "ERROR: Can create fiber only from current thread!");

			fiberContext.ContextFlags = CONTEXT_FULL;
			BOOL res = GetThreadContext( GetCurrentThread(), &fiberContext );
			MT_USED_IN_ASSERT(res);
			MT_ASSERT(res != 0, "GetThreadContext - failed");

			func = nullptr;
			funcData = nullptr;

			//Get thread stack information from thread environment block.
			stackDesc.stackTop = (void*)ReadTeb(FIELD_OFFSET(NT_TIB, StackBase));
			stackDesc.stackBottom = (void*)ReadTeb(FIELD_OFFSET(NT_TIB, StackLimit));

			isInitialized = true;
		}

		void Create(size_t stackSize, TThreadEntryPoint entryPoint, void* userData)
		{
			MT_ASSERT(!isInitialized, "Already initialized");

			func = entryPoint;
			funcData = userData;

			fiberContext.ContextFlags = CONTEXT_FULL;
			BOOL res = GetThreadContext( GetCurrentThread(), &fiberContext );
			MT_USED_IN_ASSERT(res);
			MT_ASSERT(res != 0, "GetThreadContext - failed");

			stackDesc = Memory::AllocStack(stackSize);

			void (*func)() = (void(*)())&FiberFuncInternal;

			char* sp  = (char *)stackDesc.stackTop;
			char * paramOnStack = nullptr;

			// setup function address and stack pointer
#if defined(_X86_)

			sp -= sizeof(void*); // reserve stack space for one pointer argument
			paramOnStack  = sp;
			sp -= sizeof(void*);
			fiberContext.Esp = (unsigned long long)sp;
			fiberContext.Eip = (unsigned long long) func;

#else

			// http://blogs.msdn.com/b/oldnewthing/archive/2004/01/14/58579.aspx
			// Furthermore, space for the register parameters is reserved on the stack, in case the called function wants to spill them

			sp -= 16; // pointer size and stack alignment
			paramOnStack  = sp;
			sp -= 40; // reserve for register params
			fiberContext.Rsp = (unsigned long long)sp;
			MT_ASSERT(((unsigned long long)paramOnStack & 0xF) == 0, "Params on X64 stack must be alligned to 16 bytes");
			fiberContext.Rip = (unsigned long long) func;
#endif

			//copy param to stack here
			*(void**)paramOnStack = (void *)this;

			fiberContext.ContextFlags = CONTEXT_FULL;

			isInitialized = true;
		}


		static void SwitchTo(Fiber & from, Fiber & to)
		{
			HardwareFullMemoryBarrier();

			MT_ASSERT(from.isInitialized, "Invalid from fiber");
			MT_ASSERT(to.isInitialized, "Invalid to fiber");

			HANDLE thread = GetCurrentThread();

			from.fiberContext.ContextFlags = CONTEXT_FULL;
			BOOL res = GetThreadContext(thread, &from.fiberContext );
			MT_ASSERT(res != 0, "GetThreadContext - failed");

			// Modify current stack information in TEB
			//
			// __chkstk function use TEB info and probe sampling to commit new stack pages
			// https://support.microsoft.com/en-us/kb/100775
			//
			WriteTeb(FIELD_OFFSET(NT_TIB, StackBase), (DWORD64)to.stackDesc.stackTop);
			WriteTeb(FIELD_OFFSET(NT_TIB, StackLimit), (DWORD64)to.stackDesc.stackBottom);

			res = SetThreadContext(thread, &to.fiberContext );
			MT_ASSERT(res != 0, "SetThreadContext - failed");

			//Restore stack information
			WriteTeb(FIELD_OFFSET(NT_TIB, StackBase), (DWORD64)from.stackDesc.stackTop);
			WriteTeb(FIELD_OFFSET(NT_TIB, StackLimit), (DWORD64)from.stackDesc.stackBottom);
		}


	};


}

#undef ReadTeb
#undef WriteTeb


#endif