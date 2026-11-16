// This file is part of Desktop App Toolkit,
// a set of libraries for developing nice desktop applications.
//
// For license and copyright information please follow this link:
// https://github.com/desktop-app/legal/blob/master/LEGAL
//
#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <utility>

namespace Ui::Emoji::internal {

// Tracks queued work as well as running work, so its dependencies stay alive
// until shutdown has stopped accepting work and every task has returned.
class BackgroundTasks final {
  public:
	class Token final {
	  public:
		Token(Token &&other) noexcept
			: _owner(std::exchange(other._owner, nullptr)) {}
		Token &operator=(Token &&) = delete;
		Token(const Token &) = delete;
		Token &operator=(const Token &) = delete;
		~Token() {
			if (_owner) {
				_owner->finish();
			}
		}

	  private:
		explicit Token(BackgroundTasks *owner) : _owner(owner) {}

		BackgroundTasks *_owner;
		friend class BackgroundTasks;
	};

	[[nodiscard]] std::optional<Token> start() {
		const auto lock = std::lock_guard(_mutex);
		if (_stopped) {
			return std::nullopt;
		}
		++_pending;
		return Token(this);
	}

	void stopAndWait() {
		auto lock = std::unique_lock(_mutex);
		_stopped = true;
		_finished.wait(lock, [&] { return _pending == 0; });
	}

	void restart() {
		const auto lock = std::lock_guard(_mutex);
		_stopped = false;
	}

  private:
	void finish() {
		const auto lock = std::lock_guard(_mutex);
		if (--_pending == 0) {
			_finished.notify_all();
		}
	}

	std::mutex _mutex;
	std::condition_variable _finished;
	int _pending = 0;
	bool _stopped = true;
};

// The callable is destroyed before its token, so its captures are gone
// before shutdown can release the resources they might reference.
template <typename Callable> struct TrackedTask final {
	BackgroundTasks::Token token;
	Callable callable;

	void operator()() { callable(); }
};

} // namespace Ui::Emoji::internal
