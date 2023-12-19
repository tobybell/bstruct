#pragma once

#include "backend.hh"

namespace lang {

template <class... Args, u32... I>
void call_stub_helper(Backend& b, u64 stub, Indices<I...>, Args... args) {
  static constexpr reg64 regs[] {rdi, rsi, rdx, rcx, r8, r9};
  (b.mov(regs[I], args), ...);
  b.mov(rax, stub);
  b.call(rax);
}

template <class Ret, class... StubArgs, class... Args>
void call_stub(Backend& b, Ret (*stub)(StubArgs...), Args... args) {
  static_assert(sizeof...(StubArgs) == sizeof...(Args));
  call_stub_helper(b, reinterpret_cast<u64>(stub), make_indices<sizeof...(args)> {}, args...);
}

}

