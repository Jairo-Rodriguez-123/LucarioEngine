/**
 * @file TUniquePtr.h
 * @brief Declara la API de TUniquePtr dentro del subsistema Memory.
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
#include <utility>

namespace EU {
	template<typename T>
	class TUniquePtr {
	public:
		TUniquePtr() noexcept = default;
		explicit TUniquePtr(T* rawPtr) noexcept : ptr(rawPtr) {}
		~TUniquePtr() { delete ptr; }

		TUniquePtr(const TUniquePtr&) = delete;
		TUniquePtr& operator=(const TUniquePtr&) = delete;

		TUniquePtr(TUniquePtr&& other) noexcept : ptr(other.release()) {}
		TUniquePtr& operator=(TUniquePtr&& other) noexcept {
			if (this != &other) reset(other.release());
			return *this;
		}

		T& operator*() const { return *ptr; }
		T* operator->() const noexcept { return ptr; }
		T* get() const noexcept { return ptr; }
		explicit operator bool() const noexcept { return ptr != nullptr; }
		bool isNull() const noexcept { return ptr == nullptr; }

		T* release() noexcept {
			T* old = ptr;
			ptr = nullptr;
			return old;
		}

		void reset(T* rawPtr = nullptr) noexcept {
			if (ptr == rawPtr) return;
			T* old = ptr;
			ptr = rawPtr;
			delete old;
		}

		void swap(TUniquePtr& other) noexcept { std::swap(ptr, other.ptr); }

	private:
		T* ptr = nullptr;
	};

	template<typename T, typename... Args>
	TUniquePtr<T> MakeUnique(Args&&... args) {
		return TUniquePtr<T>(new T(std::forward<Args>(args)...));
	}
}
