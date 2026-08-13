/**
 * @file TMap.h
 * @brief Declara la API de TMap dentro del subsistema Structures.
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
	template<typename K, typename V>
	class TMap {
	private:
		struct Pair {
			K Key;
			V Value;
		};

	public:
		TMap() = default;
		~TMap() = default;
		TMap(const TMap&) = default;
		TMap(TMap&&) noexcept = default;
		TMap& operator=(const TMap&) = default;
		TMap& operator=(TMap&&) noexcept = default;

		void Add(const K& key, const V& value) {
			for (auto& pair : m_data) {
				if (pair.Key == key) { pair.Value = value; return; }
			}
			m_data.push_back(Pair{ key, value });
		}

		void Remove(const K& key) {
			for (auto it = m_data.begin(); it != m_data.end(); ++it) {
				if (it->Key == key) { m_data.erase(it); return; }
			}
		}

		V& operator[](const K& key) {
			for (auto& pair : m_data) if (pair.Key == key) return pair.Value;
			throw std::out_of_range("TMap key not found");
		}

		const V& operator[](const K& key) const {
			for (const auto& pair : m_data) if (pair.Key == key) return pair.Value;
			throw std::out_of_range("TMap key not found");
		}

		bool Contains(const K& key) const {
			for (const auto& pair : m_data) if (pair.Key == key) return true;
			return false;
		}

		std::size_t Num() const noexcept { return m_data.size(); }
		std::size_t GetCapacity() const noexcept { return m_data.capacity(); }
		bool IsEmpty() const noexcept { return m_data.empty(); }
		void Clear() noexcept { m_data.clear(); }

	private:
		std::vector<Pair> m_data;
	};
}
