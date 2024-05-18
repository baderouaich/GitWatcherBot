#pragma once
#include <utility>

template <class F>
class FinalAction
{
public:
  explicit FinalAction(const F& ff) noexcept : f{ff} { }
  explicit FinalAction(F&& ff) noexcept : f{std::move(ff)} { }

  ~FinalAction() noexcept { if (invoke) f(); }

  FinalAction(FinalAction&& other) noexcept
    : f(std::move(other.f)), invoke(std::exchange(other.invoke, false))
  { }

  FinalAction(const FinalAction&)   = delete;
  void operator=(const FinalAction&) = delete;
  void operator=(FinalAction&&)      = delete;

private:
  F f;
  bool invoke = true;
};