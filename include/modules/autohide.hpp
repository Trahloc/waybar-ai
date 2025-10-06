#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>

#include "AModule.hpp"
#include "bar.hpp"
#include "modules/hyprland/backend.hpp"

namespace waybar::modules {

class Autohide : public AModule, public waybar::modules::hyprland::EventHandler {
 public:
  Autohide(const std::string& id, const Bar& bar, const Json::Value& config);
  ~Autohide();

 private:
  void startMouseTracking();
  void stopMouseTracking();
  void mouseTrackingThread();
  void checkMousePosition();
  bool getMousePosition(int& x, int& y);
  void update() override;                        // Called on main thread via dp.emit()
  void onEvent(const std::string& ev) override;  // Handle Hyprland events

  const Json::Value& config_;
  Bar* bar_;
  waybar::modules::hyprland::IPC& m_ipc;

  // Configuration
  uint32_t threshold_hidden_y_;
  uint32_t threshold_visible_y_;
  uint32_t delay_show_;
  uint32_t delay_hide_;
  uint32_t check_interval_;
  uint32_t consecutive_checks_before_visible_;

  // State machine - only one state can be true at any time
  enum class WaybarState {
    VISIBLE,          // Waybar is currently visible
    HIDDEN,           // Waybar is currently hidden
    PENDING_VISIBLE,  // Waybar is hidden but show timer is running
    PENDING_HIDDEN    // Waybar is visible but hide timer is running
  };

  std::atomic<WaybarState> waybar_state_;
  std::chrono::steady_clock::time_point timer_start_;

  // Threading
  std::thread mouse_thread_;
  std::atomic<bool> mouse_thread_running_;
  std::atomic<bool> mouse_thread_exit_;

  // Consecutive show trigger counter
  uint32_t consecutive_show_triggers_ = 0;

  // Cached monitor data (updated on main thread, read on background thread)
  struct MonitorCache {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    std::string name;
    bool valid = false;
  };

  mutable std::mutex monitor_cache_mutex_;
  MonitorCache cached_monitor_;
};

}  // namespace waybar::modules
