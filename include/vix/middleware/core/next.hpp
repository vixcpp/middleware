#pragma once

#include <functional>
#include <utility>

namespace vix::middleware
{
    using NextFn = std::function<void()>;

    class Next final
    {
    public:
        Next() = default;

        explicit Next(NextFn fn)
            : fn_(std::move(fn))
        {
        }

        bool try_call()
        {
            if (called_)
                return false;
            called_ = true;
            if (fn_)
                fn_();
            return true;
        }

        void operator()()
        {
            (void)try_call();
        }

        bool called() const noexcept { return called_; }

        explicit operator bool() const noexcept
        {
            return static_cast<bool>(fn_);
        }

    private:
        NextFn fn_{};
        bool called_{false};
    };

    using NextOnce = Next;

} // namespace vix::middleware
