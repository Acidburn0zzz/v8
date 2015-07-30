// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/assembler.h"
#include "src/macro-assembler.h"

#include "src/compiler/linkage.h"

#include "src/zone.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {
// Platform-specific configuration for C calling convention.
LinkageLocation regloc(Register reg) {
  return LinkageLocation(Register::ToAllocationIndex(reg));
}


LinkageLocation stackloc(int i) {
  DCHECK_LT(i, 0);
  return LinkageLocation(i);
}


#if V8_TARGET_ARCH_IA32
// ===========================================================================
// == ia32 ===================================================================
// ===========================================================================
#define RETURN_REGISTER_0 eax
#define RETURN_REGISTER_1 edx
#define CALLEE_SAVE_REGISTERS esi.bit() | edi.bit() | ebx.bit()

#elif V8_TARGET_ARCH_X64
// ===========================================================================
// == x64 ====================================================================
// ===========================================================================
#define RETURN_REGISTER_0 rax
#define RETURN_REGISTER_1 rdx

#ifdef _WIN64
// == x64 windows ============================================================
#define STACK_SHADOW_WORDS 4
#define PARAM_REGISTERS rcx, rdx, r8, r9
#define CALLEE_SAVE_REGISTERS                                             \
  rbx.bit() | rdi.bit() | rsi.bit() | r12.bit() | r13.bit() | r14.bit() | \
      r15.bit()
#define CALLEE_SAVE_FP_REGISTERS                                        \
  (1 << xmm6.code()) | (1 << xmm7.code()) | (1 << xmm8.code()) |        \
      (1 << xmm9.code()) | (1 << xmm10.code()) | (1 << xmm11.code()) |  \
      (1 << xmm12.code()) | (1 << xmm13.code()) | (1 << xmm14.code()) | \
      (1 << xmm15.code())
#else
// == x64 other ==============================================================
#define PARAM_REGISTERS rdi, rsi, rdx, rcx, r8, r9
#define CALLEE_SAVE_REGISTERS \
  rbx.bit() | r12.bit() | r13.bit() | r14.bit() | r15.bit()
#endif

#elif V8_TARGET_ARCH_X87
// ===========================================================================
// == x87 ====================================================================
// ===========================================================================
#define RETURN_REGISTER_0 eax
#define RETURN_REGISTER_1 edx
#define CALLEE_SAVE_REGISTERS esi.bit() | edi.bit() | ebx.bit()

#elif V8_TARGET_ARCH_ARM
// ===========================================================================
// == arm ====================================================================
// ===========================================================================
#define PARAM_REGISTERS r0, r1, r2, r3
#define RETURN_REGISTER_0 r0
#define RETURN_REGISTER_1 r1
#define CALLEE_SAVE_REGISTERS \
  r4.bit() | r5.bit() | r6.bit() | r7.bit() | r8.bit() | r9.bit() | r10.bit()
#define CALLEE_SAVE_FP_REGISTERS                                  \
  (1 << d8.code()) | (1 << d9.code()) | (1 << d10.code()) |       \
      (1 << d11.code()) | (1 << d12.code()) | (1 << d13.code()) | \
      (1 << d14.code()) | (1 << d15.code())


#elif V8_TARGET_ARCH_ARM64
// ===========================================================================
// == arm64 ====================================================================
// ===========================================================================
#define PARAM_REGISTERS x0, x1, x2, x3, x4, x5, x6, x7
#define RETURN_REGISTER_0 x0
#define RETURN_REGISTER_1 x1
#define CALLEE_SAVE_REGISTERS                                     \
  (1 << x19.code()) | (1 << x20.code()) | (1 << x21.code()) |     \
      (1 << x22.code()) | (1 << x23.code()) | (1 << x24.code()) | \
      (1 << x25.code()) | (1 << x26.code()) | (1 << x27.code()) | \
      (1 << x28.code()) | (1 << x29.code()) | (1 << x30.code())


#define CALLEE_SAVE_FP_REGISTERS                                  \
  (1 << d8.code()) | (1 << d9.code()) | (1 << d10.code()) |       \
      (1 << d11.code()) | (1 << d12.code()) | (1 << d13.code()) | \
      (1 << d14.code()) | (1 << d15.code())

#elif V8_TARGET_ARCH_MIPS
// ===========================================================================
// == mips ===================================================================
// ===========================================================================
#define PARAM_REGISTERS a0, a1, a2, a3
#define RETURN_REGISTER_0 v0
#define RETURN_REGISTER_1 v1
#define CALLEE_SAVE_REGISTERS                                                  \
  s0.bit() | s1.bit() | s2.bit() | s3.bit() | s4.bit() | s5.bit() | s6.bit() | \
      s7.bit()
#define CALLEE_SAVE_FP_REGISTERS \
  f20.bit() | f22.bit() | f24.bit() | f26.bit() | f28.bit() | f30.bit()

#elif V8_TARGET_ARCH_MIPS64
// ===========================================================================
// == mips64 =================================================================
// ===========================================================================
#define PARAM_REGISTERS a0, a1, a2, a3, a4, a5, a6, a7
#define RETURN_REGISTER_0 v0
#define RETURN_REGISTER_1 v1
#define CALLEE_SAVE_REGISTERS                                                  \
  s0.bit() | s1.bit() | s2.bit() | s3.bit() | s4.bit() | s5.bit() | s6.bit() | \
      s7.bit()
#define CALLEE_SAVE_FP_REGISTERS \
  f20.bit() | f22.bit() | f24.bit() | f26.bit() | f28.bit() | f30.bit()

#elif V8_TARGET_ARCH_PPC || V8_TARGET_ARCH_PPC64
// ===========================================================================
// == ppc & ppc64 ============================================================
// ===========================================================================
#define PARAM_REGISTERS r3, r4, r5, r6, r7, r8, r9, r10
#define RETURN_REGISTER_0 r3
#define RETURN_REGISTER_1 r4
#define CALLEE_SAVE_REGISTERS                                                 \
  r14.bit() | r15.bit() | r16.bit() | r17.bit() | r18.bit() | r19.bit() |     \
      r20.bit() | r21.bit() | r22.bit() | r23.bit() | r24.bit() | r25.bit() | \
      r26.bit() | r27.bit() | r28.bit() | r29.bit() | r30.bit() | fp.bit()

#else
// ===========================================================================
// == unknown ================================================================
// ===========================================================================
// Don't define anything. The below code will dynamically fail.
#endif
}  // namespace


// General code uses the above configuration data.
CallDescriptor* Linkage::GetSimplifiedCDescriptor(
    Zone* zone, const MachineSignature* msig) {
  LocationSignature::Builder locations(zone, msig->return_count(),
                                       msig->parameter_count());
#if 0  // TODO(titzer): instruction selector tests break here.
  // Check the types of the signature.
  // Currently no floating point parameters or returns are allowed because
  // on x87 and ia32, the FP top of stack is involved.

  for (size_t i = 0; i < msig->return_count(); i++) {
    MachineType type = RepresentationOf(msig->GetReturn(i));
    CHECK(type != kRepFloat32 && type != kRepFloat64);
  }
  for (size_t i = 0; i < msig->parameter_count(); i++) {
    MachineType type = RepresentationOf(msig->GetParam(i));
    CHECK(type != kRepFloat32 && type != kRepFloat64);
  }
#endif

#ifdef RETURN_REGISTER_0
  // Add return location(s).
  CHECK(locations.return_count_ <= 2);

  if (locations.return_count_ > 0) {
    locations.AddReturn(regloc(RETURN_REGISTER_0));
  }
  if (locations.return_count_ > 1) {
    locations.AddReturn(regloc(RETURN_REGISTER_1));
  }
#else
  // This method should not be called on unknown architectures.
  V8_Fatal(__FILE__, __LINE__,
           "requested C call descriptor on unsupported architecture");
  return nullptr;
#endif

  const int parameter_count = static_cast<int>(msig->parameter_count());

#ifdef PARAM_REGISTERS
  static const Register kParamRegisters[] = {PARAM_REGISTERS};
  static const int kParamRegisterCount =
      static_cast<int>(arraysize(kParamRegisters));
#else
  static const Register* kParamRegisters = nullptr;
  static const int kParamRegisterCount = 0;
#endif

#ifdef STACK_SHADOW_WORDS
  int stack_offset = STACK_SHADOW_WORDS;
#else
  int stack_offset = 0;
#endif
  // Add register and/or stack parameter(s).
  for (int i = 0; i < parameter_count; i++) {
    if (i < kParamRegisterCount) {
      locations.AddParam(regloc(kParamRegisters[i]));
    } else {
      locations.AddParam(stackloc(-1 - stack_offset));
      stack_offset++;
    }
  }

#ifdef CALLEE_SAVE_REGISTERS
  const RegList kCalleeSaveRegisters = CALLEE_SAVE_REGISTERS;
#else
  const RegList kCalleeSaveRegisters = 0;
#endif

#ifdef CALLEE_SAVE_FP_REGISTERS
  const RegList kCalleeSaveFPRegisters = CALLEE_SAVE_FP_REGISTERS;
#else
  const RegList kCalleeSaveFPRegisters = 0;
#endif

  // The target for C calls is always an address (i.e. machine pointer).
  MachineType target_type = kMachPtr;
  LinkageLocation target_loc = LinkageLocation::AnyRegister();
  return new (zone) CallDescriptor(  // --
      CallDescriptor::kCallAddress,  // kind
      target_type,                   // target MachineType
      target_loc,                    // target location
      msig,                          // machine_sig
      locations.Build(),             // location_sig
      0,                             // js_parameter_count
      Operator::kNoProperties,       // properties
      kCalleeSaveRegisters,          // callee-saved registers
      kCalleeSaveFPRegisters,        // callee-saved fp regs
      CallDescriptor::kNoFlags,      // flags
      "c-call");
}
}
}
}
