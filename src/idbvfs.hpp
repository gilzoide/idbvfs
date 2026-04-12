/** @file idbvfs.hpp
 *
 * C++ helper templates for idbvfs
 *
 * @see idbvfs.h
 */
/*
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * For more information, please refer to <http://unlicense.org/>
 */
#pragma once

#include "idbvfs.h"

#include <functional>
#include <memory>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace idbvfs {

namespace detail {

template<typename Fn>
void invoke_bound_callback_after_idbvfs_mounted(void *userdata) {
	if (idbvfs_is_mounted()) {
		std::unique_ptr<Fn> callback(static_cast<Fn *>(userdata));
		(*callback)();
	}
#ifdef __EMSCRIPTEN__
	else {
		emscripten_async_call(invoke_bound_callback_after_idbvfs_mounted<Fn>, userdata, 0);
	}
#endif
}

}

/**
 * Execute a callback with the given arguments after the folder where idbvfs stores databases has been mounted.
 * In Emscripten, this will be invoked using `emscripten_async_call` when `idbvfs_is_mounted` returns true.
 */
template<typename Fn, typename... Args>
void async_call_after_mounted(Fn&& fn, Args&&... args) {
	auto callback = std::bind(fn, std::forward<Args>(args)...);
	auto callback_ptr = new decltype(callback)(std::move(callback));
	detail::invoke_bound_callback_after_idbvfs_mounted<decltype(callback)>(callback_ptr);
}

}
