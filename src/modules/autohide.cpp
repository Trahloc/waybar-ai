#include "modules/autohide.hpp"

#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

namespace waybar::modules {

Autohide::Autohide(const std::string& id, const Bar& bar, const Json::Value& config)
    : AModule(config, "autohide", id, false, false),
      config_(config),
      bar_(const_cast<Bar*>(&bar)),
      m_ipc(waybar::modules::hyprland::IPC::inst()),
      waybar_state_(WaybarState::VISIBLE),  // Start with waybar visible (as it is by default)
      mouse_thread_running_(false),
      mouse_thread_exit_(false) {
  // Set modulesReady flag - this is required for IPC to work
  // This is safe because all Hyprland modules do this
  waybar::modules::hyprland::modulesReady = true;

  // Get configuration values
  threshold_hidden_y_ =
      config_["threshold-hidden-y"].isUInt() ? config_["threshold-hidden-y"].asUInt() : 1;
  threshold_visible_y_ =
      config_["threshold-visible-y"].isUInt() ? config_["threshold-visible-y"].asUInt() : 50;
  delay_show_ = config_["delay-show"].isUInt() ? config_["delay-show"].asUInt() : 0;
  delay_hide_ = config_["delay-hide"].isUInt() ? config_["delay-hide"].asUInt() : 3000;
  check_interval_ = config_["check-interval"].isUInt() ? config_["check-interval"].asUInt() : 100;
  consecutive_checks_before_visible_ = config_["consecutive-checks-before-visible"].isUInt()
                                           ? config_["consecutive-checks-before-visible"].asUInt()
                                           : 2;

  spdlog::info(
      "Autohide module initialized - hidden_y: {}, visible_y: {}, delay_show: {}ms, delay_hide: "
      "{}ms, interval: {}ms, consecutive_checks: {}",
      threshold_hidden_y_, threshold_visible_y_, delay_show_, delay_hide_, check_interval_,
      consecutive_checks_before_visible_);

  // Initialize cached monitor data (will be updated in update() method)
  {
    std::lock_guard<std::mutex> lock(monitor_cache_mutex_);
    cached_monitor_.valid = false;
  }

  // Register for workspace events - the IPC system will handle the registration
  // even if it's not ready yet (it will queue the registration)
  spdlog::info("Autohide: Registering for workspace events");
  m_ipc.registerForIPC("workspacev2", this);
  m_ipc.registerForIPC("focusedmonv2", this);

  // dp.emit() will automatically call update() on the main thread

  // Start mouse tracking thread
  startMouseTracking();
}

Autohide::~Autohide() {
  stopMouseTracking();
  m_ipc.unregisterForIPC(this);
}

void Autohide::startMouseTracking() {
  if (mouse_thread_running_.load()) {
    return;
  }

  spdlog::debug("Autohide: Starting mouse tracking thread");
  mouse_thread_exit_.store(false);
  mouse_thread_ = std::thread(&Autohide::mouseTrackingThread, this);
  mouse_thread_running_.store(true);
}

void Autohide::stopMouseTracking() {
  if (!mouse_thread_running_.load()) {
    return;
  }

  spdlog::debug("Autohide: Stopping mouse tracking thread");
  mouse_thread_exit_.store(true);

  if (mouse_thread_.joinable()) {
    mouse_thread_.join();
  }
  mouse_thread_running_.store(false);
}

void Autohide::mouseTrackingThread() {
  spdlog::debug("Autohide: Mouse tracking thread started");

  while (!mouse_thread_exit_.load()) {
    if (bar_) {
      checkMousePosition();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(check_interval_));
  }

  spdlog::debug("Autohide: Mouse tracking thread stopped");
}

void Autohide::checkMousePosition() {
  int mouse_x, mouse_y;

  if (!getMousePosition(mouse_x, mouse_y)) {
    spdlog::debug("Autohide: Failed to get mouse position");
    return;  // Failed to get mouse position
  }

  // Get cached monitor data (thread-safe, no GTK access)
  MonitorCache monitor_cache;
  bool cache_valid = false;
  {
    std::lock_guard<std::mutex> lock(monitor_cache_mutex_);
    monitor_cache = cached_monitor_;
    cache_valid = monitor_cache.valid;
  }

  if (!cache_valid) {
    spdlog::debug("Autohide: No valid monitor cache available");
    return;
  }

  // Check if mouse is actually on this monitor
  if (mouse_x < monitor_cache.x || mouse_x >= monitor_cache.x + monitor_cache.width ||
      mouse_y < monitor_cache.y || mouse_y >= monitor_cache.y + monitor_cache.height) {
    spdlog::debug("Autohide: Mouse at ({},{}) not on monitor {} (geometry: x={}, y={}, w={}, h={})",
                  mouse_x, mouse_y, monitor_cache.name, monitor_cache.x, monitor_cache.y,
                  monitor_cache.width, monitor_cache.height);
    return;  // Mouse is not on this monitor, ignore
  }

  // Convert to monitor-relative coordinates
  int monitor_mouse_y = mouse_y - monitor_cache.y;

  // Log mouse position changes (trace level to avoid spam)
  spdlog::trace("Autohide: Mouse at screen ({},{}) -> monitor y={}, state={}", mouse_x, mouse_y,
                monitor_mouse_y, static_cast<int>(waybar_state_.load()));

  // Simple logic: mouse_y <= 1px = visible, mouse_y > 50px = hidden
  if (monitor_mouse_y <= static_cast<int>(threshold_hidden_y_)) {
    // Mouse at top 1px - should show waybar (requires configurable consecutive events)
    consecutive_show_triggers_++;

    if (consecutive_show_triggers_ >= consecutive_checks_before_visible_) {
      // We have enough consecutive show triggers
      if (waybar_state_ == WaybarState::HIDDEN) {
        spdlog::debug(
            "Autohide: Mouse at y={} (<=1px) on monitor {} - {} consecutive triggers, "
            "scheduling show",
            monitor_mouse_y, bar_->output->name, consecutive_show_triggers_);
        waybar_state_.store(WaybarState::PENDING_VISIBLE, std::memory_order_seq_cst);
        timer_start_ = std::chrono::steady_clock::now();
      } else if (waybar_state_ == WaybarState::PENDING_HIDDEN) {
        spdlog::debug(
            "Autohide: Mouse at y={} (<=1px) on monitor {} - {} consecutive triggers, canceling "
            "hide, scheduling show",
            monitor_mouse_y, bar_->output->name, consecutive_show_triggers_);
        waybar_state_.store(WaybarState::PENDING_VISIBLE, std::memory_order_seq_cst);
        timer_start_ = std::chrono::steady_clock::now();
      }
    } else {
      // Not enough consecutive triggers yet
      spdlog::trace(
          "Autohide: Mouse at y={} (<=1px) on monitor {} - {}/{} consecutive triggers, waiting for "
          "more",
          monitor_mouse_y, bar_->output->name, consecutive_show_triggers_,
          consecutive_checks_before_visible_);
    }
  } else if (monitor_mouse_y > static_cast<int>(threshold_visible_y_)) {
    // Mouse below 50px - should hide waybar (only if currently visible)
    if (waybar_state_ == WaybarState::VISIBLE) {
      spdlog::trace("Autohide: Mouse at y={} (>50px) on monitor {} - scheduling hide",
                    monitor_mouse_y, bar_->output->name);
      waybar_state_.store(WaybarState::PENDING_HIDDEN, std::memory_order_seq_cst);
      timer_start_ = std::chrono::steady_clock::now();
    } else if (waybar_state_ == WaybarState::PENDING_VISIBLE) {
      spdlog::trace(
          "Autohide: Mouse at y={} (>50px) on monitor {} - canceling show, scheduling hide",
          monitor_mouse_y, bar_->output->name);
      waybar_state_.store(WaybarState::PENDING_HIDDEN, std::memory_order_seq_cst);
      timer_start_ = std::chrono::steady_clock::now();
    }
    // If already PENDING_HIDDEN, don't reset the timer - let it continue counting
    // This ensures the timer only starts once when entering the hide zone

    // Reset the show trigger counter when mouse moves to hide zone
    consecutive_show_triggers_ = 0;
  } else {
    // Mouse is between 1px and 50px - reset the show trigger counter
    consecutive_show_triggers_ = 0;
  }

  // Check if pending actions should execute
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - timer_start_).count();

  if (waybar_state_ == WaybarState::PENDING_VISIBLE) {
    // Use minimum 10ms delay to avoid race conditions and timing issues
    uint32_t effective_delay = std::max(delay_show_, 10u);
    if (elapsed >= static_cast<long>(effective_delay)) {
      spdlog::debug("Autohide: Executing delayed show after {}ms", elapsed);
      waybar_state_.store(WaybarState::VISIBLE, std::memory_order_seq_cst);
      dp.emit();
    }
  } else if (waybar_state_ == WaybarState::PENDING_HIDDEN) {
    // Use minimum 10ms delay to avoid race conditions and timing issues
    uint32_t effective_delay = std::max(delay_hide_, 10u);
    if (elapsed >= static_cast<long>(effective_delay)) {
      spdlog::debug("Autohide: Executing delayed hide after {}ms", elapsed);
      waybar_state_.store(WaybarState::HIDDEN, std::memory_order_seq_cst);
      dp.emit();
    }
  }
}

bool Autohide::getMousePosition(int& x, int& y) {
  // Use Hyprland IPC to get global cursor position
  try {
    auto reply = m_ipc.getSocket1Reply("cursorpos");
    if (reply.empty()) {
      return false;
    }

    // Parse the reply format: "x,y" (e.g., "1234,567")
    size_t comma_pos = reply.find(',');
    if (comma_pos == std::string::npos) {
      return false;
    }

    x = std::stoi(reply.substr(0, comma_pos));
    y = std::stoi(reply.substr(comma_pos + 1));
    return true;
  } catch (const std::exception& e) {
    spdlog::debug("Autohide: Failed to get cursor position via IPC: {}", e.what());
    return false;
  }
}

void Autohide::update() {
  // This method runs on the main thread, so it's safe to call GTK operations
  if (!bar_) {
    spdlog::warn("Autohide: Bar pointer is null, cannot update visibility");
    return;
  }

  // Cache monitor data on main thread for background thread to use safely
  {
    std::lock_guard<std::mutex> lock(monitor_cache_mutex_);
    if (bar_->output && bar_->output->monitor) {
      auto monitor_geometry = *bar_->output->monitor->property_geometry().get_value().gobj();

      cached_monitor_.x = monitor_geometry.x;
      cached_monitor_.y = monitor_geometry.y;
      cached_monitor_.width = monitor_geometry.width;
      cached_monitor_.height = monitor_geometry.height;
      cached_monitor_.name = bar_->output->name;
      cached_monitor_.valid = true;
    } else {
      // Clear cache if monitor is not available
      cached_monitor_.valid = false;
    }
  }

  switch (waybar_state_.load()) {
    case WaybarState::VISIBLE:
    case WaybarState::PENDING_HIDDEN:
      bar_->setMode(Bar::MODE_DEFAULT);
      break;
    case WaybarState::HIDDEN:
    case WaybarState::PENDING_VISIBLE:
      bar_->setMode(Bar::MODE_INVISIBLE);
      break;
  }
}

void Autohide::onEvent(const std::string& ev) {
  std::string eventName;
  size_t pos = ev.find_first_of('>');
  if (pos == std::string::npos) {
    eventName = ev;
  } else {
    eventName = ev.substr(0, pos);
  }

  if (eventName == "workspacev2" || eventName == "focusedmonv2") {
    spdlog::trace("Autohide: Workspace/monitor changed - forcing waybar visible");

    // Force waybar to be visible when workspace changes
    waybar_state_ = WaybarState::VISIBLE;
    dp.emit();  // Trigger update() on main thread
  }
}

}  // namespace waybar::modules
