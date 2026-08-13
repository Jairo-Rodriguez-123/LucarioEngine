/**
 * @file TSharedPointer.h
 * @brief Shared pointer wrapper used by the engine.
 * @ingroup memory
 */
#pragma once
#include <memory>
#include <utility>

namespace EU {
	template<typename T> class TWeakPointer;
	template<typename T> class TSharedPointer;
	template<typename T, typename... Args> TSharedPointer<T> MakeShared(Args&&... args);

	template<typename T>
	class TSharedPointer {
	private:
		// Keep the owner declared before `ptr`: C++ initializes members in
		// declaration order, not initializer-list order.
		std::shared_ptr<T> m_owner;

		explicit TSharedPointer(std::shared_ptr<T> owner) noexcept
			: m_owner(std::move(owner)), ptr(m_owner.get()) {}

	public:
		TSharedPointer() noexcept = default;
		explicit TSharedPointer(T* rawPtr) : m_owner(rawPtr), ptr(m_owner.get()) {}

		TSharedPointer(const TSharedPointer& other) noexcept
			: m_owner(other.m_owner), ptr(m_owner.get()) {}

		TSharedPointer(TSharedPointer&& other) noexcept
			: m_owner(std::move(other.m_owner)), ptr(m_owner.get()) {
			other.ptr = nullptr;
		}

		TSharedPointer& operator=(const TSharedPointer& other) noexcept {
			if (this != &other) {
				m_owner = other.m_owner;
				ptr = m_owner.get();
			}
			return *this;
		}

		TSharedPointer& operator=(TSharedPointer&& other) noexcept {
			if (this != &other) {
				m_owner = std::move(other.m_owner);
				ptr = m_owner.get();
				other.ptr = nullptr;
			}
			return *this;
		}

		~TSharedPointer() = default;

		T& operator*() const { return *m_owner; }
		T* operator->() const noexcept { return m_owner.get(); }
		T* get() const noexcept { return m_owner.get(); }
		explicit operator bool() const noexcept { return static_cast<bool>(m_owner); }
		bool isNull() const noexcept { return !m_owner; }
		long use_count() const noexcept { return m_owner.use_count(); }

		void reset(T* newPtr = nullptr) {
			m_owner.reset(newPtr);
			ptr = m_owner.get();
		}

		void swap(TSharedPointer& other) noexcept {
			m_owner.swap(other.m_owner);
			ptr = m_owner.get();
			other.ptr = other.m_owner.get();
		}

		template<typename U>
		TSharedPointer<U> dynamic_pointer_cast() const noexcept {
			return TSharedPointer<U>(std::dynamic_pointer_cast<U>(m_owner));
		}

		// Public only for compatibility with existing engine code. Prefer get().
		T* ptr = nullptr;

	private:
		template<typename U> friend class TSharedPointer;
		template<typename U> friend class TWeakPointer;
		template<typename U, typename... Args> friend TSharedPointer<U> MakeShared(Args&&... args);
	};

	template<typename T, typename... Args>
	TSharedPointer<T> MakeShared(Args&&... args) {
		return TSharedPointer<T>(std::make_shared<T>(std::forward<Args>(args)...));
	}
}
