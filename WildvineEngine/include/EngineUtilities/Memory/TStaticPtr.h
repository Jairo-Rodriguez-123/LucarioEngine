/**
 * @file TStaticPtr.h
 * @brief Declara la API de TStaticPtr dentro del subsistema Memory.
 * @ingroup memory
 */
/*
 * MIT License
 *
 * Copyright (c) 2025 Roberto Charreton
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * In addition, any project or software that uses this library or class must include
 * the following acknowledgment in the credits:
 *
 * "This project uses software developed by Roberto Charreton and Attribute Overload."
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
*/
#pragma once
#include <memory>

namespace EU {
	template<typename T>
	class TStaticPtr {
	public:
		TStaticPtr() = default;
		explicit TStaticPtr(T* rawPtr) { reset(rawPtr); }
		~TStaticPtr() = default;

		TStaticPtr(const TStaticPtr&) = delete;
		TStaticPtr& operator=(const TStaticPtr&) = delete;

		static T* get() noexcept { return instance.get(); }
		static bool isNull() noexcept { return !instance; }
		static void reset(T* rawPtr = nullptr) { instance.reset(rawPtr); }

	private:
		inline static std::unique_ptr<T> instance{};
	};
}
