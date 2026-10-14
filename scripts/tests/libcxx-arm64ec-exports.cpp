/* Build for the exact Water ARM64EC Microsoft C++ ABI and query the
 * public symbols that real libc++ consumers need from libwhp-libcxx.a.
 * This is deliberately not an x64 or GNU C++ ABI compatibility test. */
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <locale>
#include <mutex>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>

#if !defined(__arm64ec__) || !defined(_MSC_VER)
# error Water libc++ export probe must use the ARM64EC MSVC ABI
#endif
#ifndef _LIBCPP_ABI_MICROSOFT
# error Water libc++ export probe must use the Microsoft libc++ ABI
#endif
#ifndef _LIBCPP_ABI_VCRUNTIME
# error Water libc++ export probe must use the vcruntime provider
#endif

int whp_arm64ec_libcxx_exports(std::mutex &mutex,
                              std::condition_variable &condition,
                              const char *text)
{
    mutex.lock();
    mutex.unlock();
    condition.notify_one();
    condition.notify_all();

    std::string value(text);
    value.append("!");
    const std::locale &locale = std::locale::classic();
    std::error_code error(1, std::generic_category());
    const auto now = std::chrono::steady_clock::now();
    const auto threads = std::thread::hardware_concurrency();
    std::cout.flush();
    if (value.empty() || locale.name().empty() || error.message().empty())
        throw std::runtime_error("libc++ runtime export probe");
    return static_cast<int>(now.time_since_epoch().count() & 1) +
           static_cast<int>(threads & 1);
}
