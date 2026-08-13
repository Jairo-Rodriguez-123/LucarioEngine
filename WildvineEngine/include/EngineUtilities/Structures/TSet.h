/**
 * @file TSet.h
 * @brief Declara la API de TSet dentro del subsistema Structures.
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
#include <utility>
#include <vector>

namespace EU {
	template<typename T>
	class TSet {
	public:
		TSet() = default;
		~TSet() = default;
		TSet(const TSet&) = default;
		TSet(TSet&&) noexcept = default;
		TSet& operator=(const TSet&) = default;
		TSet& operator=(TSet&&) noexcept = default;

		void Add(const T& element) {
			if (!Contains(element)) m_data.push_back(element);
		}

		void Add(T&& element) {
			if (!Contains(element)) m_data.push_back(std::move(element));
		}

		void Remove(const T& element) {
			for (auto it = m_data.begin(); it != m_data.end(); ++it) {
				if (*it == element) { m_data.erase(it); return; }
			}
		}

		bool Contains(const T& element) const {
			for (const auto& item : m_data) if (item == element) return true;
			return false;
		}

		std::size_t Num() const noexcept { return m_data.size(); }
		std::size_t GetCapacity() const noexcept { return m_data.capacity(); }
		bool IsEmpty() const noexcept { return m_data.empty(); }
		void Clear() noexcept { m_data.clear(); }

	private:
		std::vector<T> m_data;
	};
}
