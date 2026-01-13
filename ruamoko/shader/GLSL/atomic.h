#ifndef __qfcc_shader_glsl_atomic_h
#define __qfcc_shader_glsl_atomic_h

// atomic functions
@overload uint atomicAdd(uintr mem, const uint data)
	= SPV(OpAtomicIAdd) [mem, Scope.Device, MemorySemantics.Relaxed, data];
@overload int atomicAdd(intr mem, const int data)
	= SPV(OpAtomicIAdd) [mem, Scope.Device, MemorySemantics.Relaxed, data];
@overload uint atomicMin(uintr mem, const uint data)
	= SPV(OpAtomicUMin) [mem, Scope.Device, MemorySemantics.Relaxed, data];
@overload int atomicMin(intr mem, const int data)
	= SPV(OpAtomicSMin) [mem, Scope.Device, MemorySemantics.Relaxed, data];
@overload uint atomicMax(uintr mem, const uint data)
	= SPV(OpAtomicUMax) [mem, Scope.Device, MemorySemantics.Relaxed, data];
@overload int atomicMax(intr mem, const int data)
	= SPV(OpAtomicUMax) [mem, Scope.Device, MemorySemantics.Relaxed, data];
@overload uint atomicAnd(uintr mem, const uint data)
	= SPV(OpAtomicAnd) [mem, Scope.Device, MemorySemantics.Relaxed, data];
@overload int atomicAnd(intr mem, const int data)
	= SPV(OpAtomicAnd) [mem, Scope.Device, MemorySemantics.Relaxed, data];
@overload uint atomicOr(uintr mem, const uint data)
	= SPV(OpAtomicOr) [mem, Scope.Device, MemorySemantics.Relaxed, data];
@overload int atomicOr(intr mem, const int data)
	= SPV(OpAtomicOr) [mem, Scope.Device, MemorySemantics.Relaxed, data];
@overload uint atomicXor(uintr mem, const uint data)
	= SPV(OpAtomicXor) [mem, Scope.Device, MemorySemantics.Relaxed, data];
@overload int atomicXor(intr mem, const int data)
	= SPV(OpAtomicXor) [mem, Scope.Device, MemorySemantics.Relaxed, data];
@overload uint atomicExchange(uintr mem, const uint data)
	= SPV(OpAtomicExchange) [mem, Scope.Device, MemorySemantics.Relaxed, data];
@overload int atomicExchange(intr mem, const int data)
	= SPV(OpAtomicExchange) [mem, Scope.Device, MemorySemantics.Relaxed, data];
@overload uint atomicCompSwap(uintr mem, const uint compare, const uint data)
	= SPV(OpAtomicCompareExchange) [mem, Scope.Device,
									MemorySemantics.Relaxed, data];
@overload int atomicCompSwap(intr mem, const int compare, const int data)
	= SPV(OpAtomicCompareExchange) [mem, Scope.Device,
									MemorySemantics.Relaxed, data];

#endif//__qfcc_shader_glsl_atomic_h
