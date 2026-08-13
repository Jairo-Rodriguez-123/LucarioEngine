/**
 * @file TArray.h
 * @brief Declara la API de TArray dentro del subsistema Structures.
 * @ingroup structures
 */
/*
 * MIT License
 *
 * Copyright (c) 2024 Roberto Charreton
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
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace EU {
	template<typename T>
	class TArray {
	public:
		TArray() = default;
		~TArray() = default;
		TArray(const TArray&) = default;
		TArray(TArray&&) noexcept = default;
		TArray& operator=(const TArray&) = default;
		TArray& operator=(TArray&&) noexcept = default;

		void Add(const T& element) { m_data.push_back(element); }
		void Add(T&& element) { m_data.push_back(std::move(element)); }

		void RemoveAt(std::size_t index) {
			if (index >= m_data.size()) return;
			m_data.erase(m_data.begin() + static_cast<std::ptrdiff_t>(index));
		}

		T& operator[](std::size_t index) {
			if (index >= m_data.size()) throw std::out_of_range("TArray index out of range");
			return m_data[index];
		}

		const T& operator[](std::size_t index) const {
			if (index >= m_data.size()) throw std::out_of_range("TArray index out of range");
			return m_data[index];
		}

		std::size_t Num() const noexcept { return m_data.size(); }
		std::size_t GetCapacity() const noexcept { return m_data.capacity(); }
		bool IsEmpty() const noexcept { return m_data.empty(); }
		void Clear() noexcept { m_data.clear(); }

	private:
		std::vector<T> m_data;
	};
}
