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

#ifndef __MT_STACK__
#define __MT_STACK__

#include <array>
#include <limits>

namespace MT
{
	static const int32 invalidStackId = 0;
	static const int32 invalidStorageId = 0;


	//
	// Scope descriptor
	//
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	class ScopeDesc
	{
	protected:

		//descriptor name
		const char* name;

		//descriptor declaration file/line
		const char* file;
		int32 line;

	public:

		ScopeDesc(const char* srcFile, int32 srcLine, const char* scopeName)
			: name(scopeName)
			, file(srcFile)
			, line(srcLine)
		{
		}

		const char* GetSourceFile() const
		{
			return file;
		}

		int32 GetSourceLine() const
		{
			return line;
		}

		const char* GetName() const
		{
			return name;
		}


	};


	//
	// Scope stack entry
	//
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	class ScopeStackEntry
	{
		int32 parentIndex;
		int32 descIndex;

	public:

		ScopeStackEntry(int32 _parentIndex, int32 _descIndex)
			: parentIndex(_parentIndex)
			, descIndex(_descIndex)
		{
		}

#ifdef _DEBUG
		~ScopeStackEntry()
		{
			parentIndex = std::numeric_limits<int32>::lowest();
			descIndex = std::numeric_limits<int32>::lowest();
		}
#endif

		int32 GetParentId() const
		{
			return parentIndex;
		}

		int32 GetDescriptionId() const
		{
			return descIndex;
		}


	};


	//
	// Persistent scope descriptor storage
	//
	//   persistent storage used to store scope descriptors
	//   descriptors lifetime is equal to the storage lifetime    
	//
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	template<typename T, uint32 capacity>
	class PersistentScopeDescriptorStorage
	{
		MT::AtomicInt32 top;
		byte rawMemory[ capacity * sizeof(T) ];

	public:

		PersistentScopeDescriptorStorage()
		{
			static_assert(std::is_base_of<MT::ScopeDesc, T>::value, "Type must be derived from MT::ScopeDesc");
			top.Store(0);
		}

		~PersistentScopeDescriptorStorage()
		{
			int32 count = top.Store(0);
			for (int32 i = 0; i < count; i++)
			{
				T* pObject = (T*)&rawMemory[i * sizeof(T)];
				MT_UNUSED(pObject);
				pObject->~T();
			}
		}

		int32 Alloc(const char* srcFile, int32 srcLine, const char* scopeName)
		{
			//new element index
			int32 index = top.IncFetch() - 1;
			MT_VERIFY(index < (int32)capacity, "Area allocator is full. Can't allocate more memory.", return -1);

			//get memory for object
			T* pObject = (T*)&rawMemory[index * sizeof(T)];

			//placement ctor
			new(pObject) T(srcFile, srcLine, scopeName);

			int32 id = (index + 1);
			return id;
		}


		T* Get(int32 id)
		{
			MT_VERIFY(id > invalidStorageId, "Invalid ID", return nullptr );
			MT_VERIFY(id <= top.Load(), "Invalid ID", return nullptr );

			int32 index = ( id - 1);
			T* pObject = (T*)&rawMemory[index * sizeof(T)];

			return pObject;
		}

	};


	//
	// Weak scope stack
	//
	//  Weak stack, which means that any data from the stack become invalid after stack entry is popped from stack
	//  Weak stack uses a small amount of memory, but in the case of deferred use the stack entires you must copy this entries to extend lifetime.
	//
	//  Well suited as asset/resource names stack.
	//
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	template<typename T, uint32 capacity>
	class WeakScopeStack
	{
		volatile int32 top;
		byte rawMemory[ capacity * sizeof(T) ];

		T* IndexToObject(int32 index)
		{
			T* pObject = (T*)&rawMemory[ index * sizeof(T) ];
			return pObject;
		}


		T* AllocObject()
		{
			int32 index = top;
			MT_VERIFY(index < (int32)capacity, "Stack allocator overflow. Can't allocate more memory.", return nullptr);
			top++;
			T* pObject = IndexToObject(index);
			return pObject;
		}

	public:

		WeakScopeStack()
		{
			static_assert(std::is_base_of<MT::ScopeStackEntry, T>::value, "Type must be derived from MT::ScopeStackEntry");
			top = invalidStackId;
		}

		~WeakScopeStack()
		{
			for(int32 i = 0; i < top; i++)
			{
				T* pObject = IndexToObject(i);
				MT_UNUSED(pObject);
				pObject->~T();
			}
			top = 0;
		}


		T* Get(int32 id)
		{
			MT_VERIFY(id > invalidStackId, "Invalid id", return nullptr);
			int32 index = (id - 1);
			return IndexToObject(index);
		}

		int32 Top()
		{
			int32 id = top;
			return id;
		}

		void Pop()
		{
			top--;
			int32 index = top;
			MT_ASSERT(index >= 0, "Stack already empty. Invalid call.");
			T* pObject = IndexToObject(index);
			MT_UNUSED(pObject);
			pObject->~T();
		}

		T* Push()
		{
			T* pObject = AllocObject();
			new(pObject) T();
			return pObject;
		}

		template<typename T1>
		T* Push(T1 p1)
		{
			T* pObject = AllocObject();
			new(pObject) T(p1);
			return pObject;
		}

		template<typename T1, typename T2>
		T* Push(T1 p1, T2 p2)
		{
			T* pObject = AllocObject();
			new(pObject) T(p1, p2);
			return pObject;
		}

		template<typename T1, typename T2, typename T3>
		T* Push(T1 p1, T2 p2, T3 p3)
		{
			T* pObject = AllocObject();
			new(pObject) T(p1, p2, p3);
			return pObject;
		}

		template<typename T1, typename T2, typename T3, typename T4>
		T* Push(T1 p1, T2 p2, T3 p3, T4 p4)
		{
			T* pObject = AllocObject();
			new(pObject) T(p1, p2, p3, p4);
			return pObject;
		}
	};


	//
	// Strong scope stack
	//
	//  Strong stack, which means that any data from the stack is always valid, until you call Reset();
	//  Strong stack uses a lot of memory, but in the case of deferred use of stack entires you can store single pointer to current stack entry.
	//
	//  Well suited as CPU profiler timings stack.
	//
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	template<typename T, uint32 capacity>
	class StrongScopeStack
	{
		volatile int32 count;

		volatile int32 top;

		//max stack deep
		std::array<int32, 256> stackId;

		byte rawMemory[ capacity * sizeof(T) ];

		T* IndexToObject(int32 index)
		{
			T* pObject = (T*)&rawMemory[ index * sizeof(T) ];
			return pObject;
		}


		T* AllocObject()
		{
			int32 stackIndex = top;
			MT_VERIFY(stackIndex < (int32)stackId.size(), "Stack is too deep.", return nullptr);
			top++;

			int32 index = count;
			MT_VERIFY(index < (int32)capacity, "Stack allocator overflow. Can't allocate more memory.", return nullptr);
			count++;
			T* pObject = IndexToObject(index);

			stackId[stackIndex] = (index + 1);

			return pObject;
		}


	public:

		StrongScopeStack()
		{
			static_assert(std::is_base_of<MT::ScopeStackEntry, T>::value, "Type must be derived from MT::ScopeStackEntry");
			top = invalidStackId;
			count = 0;
		}

		~StrongScopeStack()
		{
			Reset();
		}

		T* Get(int32 id)
		{
			MT_VERIFY(id > invalidStackId, "Invalid id", return nullptr);
			int32 index = (id - 1);
			return IndexToObject(index);
		}

		int32 Top()
		{
			if (top == invalidStackId)
			{
				return invalidStackId;
			}

			return stackId[top - 1];
		}

		void Pop()
		{
			top--;
			int32 index = top;
			MT_ASSERT(index >= 0, "Stack already empty. Invalid call.");

			stackId[index] = 0;
		}


		T* Push()
		{
			T* pObject = AllocObject();
			new(pObject) T();
			return pObject;
		}

		template<typename T1, typename T2>
		T* Push(T1 p1, T2 p2)
		{
			T* pObject = AllocObject();
			new(pObject) T(p1, p2);
			return pObject;
		}

		template<typename T1, typename T2, typename T3>
		T* Push(T1 p1, T2 p2, T3 p3)
		{
			T* pObject = AllocObject();
			new(pObject) T(p1, p2, p3);
			return pObject;
		}

		template<typename T1, typename T2, typename T3, typename T4>
		T* Push(T1 p1, T2 p2, T3 p3, T4 p4)
		{
			T* pObject = AllocObject();
			new(pObject) T(p1, p2, p3, p4);
			return pObject;
		}


		void Reset()
		{
			for(int32 i = 0; i < count; i++)
			{
				T* pObject = IndexToObject(i);
				MT_UNUSED(pObject);
				pObject->~T();
			}

#ifdef _DEBUG
			int32 stackIdCount = (int32)stackId.size();
			for(int32 i = 0; i < stackIdCount; i++)
			{
				stackId[i] = std::numeric_limits<int32>::lowest();
			}
#endif
			count = 0;
			top = invalidStackId;
		}

	};

} //MT namespace

#define DECLARE_SCOPE_DESCRIPTOR_IMPL( file, line, name, storagePointer ) \
	const int32 scope_notInitialized = 0; \
	const int32 scope_notYetInitialized = -1; \
	\
	static MT::AtomicInt32Base scope_descriptorIndex = { scope_notInitialized }; \
	static_assert(std::is_pod<MT::AtomicInt32Base>::value == true, "AtomicInt32Base type should be POD, to be placed in bss/data section"); \
	\
	int32 scope_descId = scope_notInitialized; \
	\
	int32 scope_state = scope_descriptorIndex.CompareAndSwap(scope_notInitialized, scope_notYetInitialized); \
	switch(scope_state) \
	{ \
		/* first time here, need to allocate descriptor*/ \
		case scope_notInitialized: \
		{ \
			MT_ASSERT( storagePointer != nullptr, "Scopes storage pointer was not initialized!"); \
			scope_descId = storagePointer -> Alloc(file, line, name); \
			scope_descriptorIndex.Store(scope_descId);  \
			break; \
		} \
		\
		/* allocation in progress */ \
		/* wait until the allocation is finished */ \
		case scope_notYetInitialized: \
		{ \
			for(;;) \
			{ \
				scope_descId = scope_descriptorIndex.Load(); \
				if (scope_descId != scope_notYetInitialized) \
				{ \
					break; \
				} \
				MT::YieldCpu(); \
			} \
			break; \
		} \
		/* description already allocated */ \
		default: \
		{ \
			scope_descId = scope_state; \
			break; \
		} \
	} \


// declare scope descriptor for current scope.
#define DECLARE_SCOPE_DESCRIPTOR(name, storagePointer) DECLARE_SCOPE_DESCRIPTOR_IMPL(__FILE__, __LINE__, name, storagePointer)

// push new stack entry to stack
#define SCOPE_STACK_PUSH(scopeDescriptorId, stackPointer) \
	MT_ASSERT(stackPointer != nullptr, "Stack pointer is not initialized for current thread."); \
	int32 scope_stackParentId = stackPointer -> Top(); \
	MT_ASSERT(scope_stackParentId >= 0, "Invalid parent ID"); \
	stackPointer -> Push(scope_stackParentId, scopeDescriptorId); \


// push new stack entry to stack
#define SCOPE_STACK_PUSH1(scopeDescriptorId, param1, stackPointer) \
	MT_ASSERT(stackPointer != nullptr, "Stack pointer is not initialized for current thread."); \
	int32 scope_stackParentId = stackPointer -> Top(); \
	MT_ASSERT(scope_stackParentId >= 0, "Invalid parent ID"); \
	stackPointer -> Push(scope_stackParentId, scopeDescriptorId, param1); \

// push new stack entry to stack
#define SCOPE_STACK_PUSH2(scopeDescriptorId, param1, param2, stackPointer) \
	MT_ASSERT(stackPointer != nullptr, "Stack pointer is not initialized for current thread."); \
	int32 scope_stackParentId = stackPointer -> Top(); \
	MT_ASSERT(scope_stackParentId >= 0, "Invalid parent ID"); \
	stackPointer -> Push(scope_stackParentId, scopeDescriptorId, param1, param2); \


// pop from the stack
#define SCOPE_STACK_POP(stackPointer) \
	MT_ASSERT(stackPointer != nullptr, "Stack pointer is not initialized for current thread."); \
	stackPointer -> Pop(); \

// get top of the stack
#define SCOPE_STACK_TOP(stackPointer) \
	stackPointer -> Get( stackPointer -> Top() )


#define SCOPE_STACK_GET_PARENT(stackEntry, stackPointer) \
	(stackEntry -> GetParentId() == MT::invalidStackId) ? nullptr : stackPointer -> Get( stackEntry -> GetParentId() )




#endif