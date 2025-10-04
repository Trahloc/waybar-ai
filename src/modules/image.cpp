#include "modules/image.hpp"

/**
 * @brief Constructs an Image module, initializes its widgets, and starts its update worker.
 *
 * Initializes the module with the given id and JSON configuration, creates and configures
 * the horizontal container and image widget, applies CSS classes, emits an initial display
 * population event, reads sizing and refresh-interval configuration, and starts the worker
 * that will periodically trigger updates.
 *
 * @param id Identifier used for the module and as a CSS class when non-empty.
 * @param config JSON configuration for the module. Recognized keys:
 *               - "size": integer pixel size for the image (defaults to 16 when 0 or missing).
 *               - "interval": if the string "once", the module will use a one-shot (max) interval;
 *                 if numeric, treated as seconds and converted to milliseconds with a minimum of 1 ms;
 *                 otherwise a one-shot (max) interval is used.
 */
waybar::modules::Image::Image(const std::string& id, const Json::Value& config)
    : AModule(config, "image", id), box_(Gtk::ORIENTATION_HORIZONTAL, 0) {
  box_.pack_start(image_);
  box_.set_name("image");
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  box_.get_style_context()->add_class(MODULE_CLASS);
  event_box_.add(box_);

  dp.emit();

  size_ = config["size"].asInt();

  if (config_["interval"].isString() && config_["interval"].asString() == "once") {
    interval_ = std::chrono::milliseconds::max();
  } else if (config_["interval"].isNumeric()) {
    auto seconds = config_["interval"].asDouble();
    auto millis = static_cast<long>(seconds * 1000);
    interval_ = std::chrono::milliseconds(std::max(1L, millis));
  } else {
    interval_ = std::chrono::milliseconds::max();
  }

  if (size_ == 0) {
    size_ = 16;
  }

  delayWorker();
}

/**
 * @brief Prepare the worker task that triggers periodic image updates.
 *
 * Assigns a callable to `thread_` which emits the module's display update signal
 * and then waits for the configured `interval_` before returning.
 */
void waybar::modules::Image::delayWorker() {
  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(interval_);
  };
}

void waybar::modules::Image::refresh(int sig) {
  if (sig == SIGRTMIN + config_["signal"].asInt()) {
    thread_.wake_up();
  }
}

auto waybar::modules::Image::update() -> void {
  if (config_["path"].isString()) {
    path_ = config_["path"].asString();
  } else if (config_["exec"].isString()) {
    output_ = util::command::exec(config_["exec"].asString(), "");
    parseOutputRaw();
  } else {
    path_ = "";
  }

  if (Glib::file_test(path_, Glib::FILE_TEST_EXISTS)) {
    Glib::RefPtr<Gdk::Pixbuf> pixbuf;

    int scaled_icon_size = size_ * image_.get_scale_factor();
    pixbuf = Gdk::Pixbuf::create_from_file(path_, scaled_icon_size, scaled_icon_size);

    auto surface = Gdk::Cairo::create_surface_from_pixbuf(pixbuf, image_.get_scale_factor(),
                                                          image_.get_window());
    image_.set(surface);
    image_.show();

    if (tooltipEnabled() && !tooltip_.empty()) {
      if (box_.get_tooltip_markup() != tooltip_) {
        box_.set_tooltip_markup(tooltip_);
      }
    }

    box_.get_style_context()->remove_class("empty");
  } else {
    image_.clear();
    image_.hide();
    box_.get_style_context()->add_class("empty");
  }

  AModule::update();
}

void waybar::modules::Image::parseOutputRaw() {
  std::istringstream output(output_.out);
  std::string line;
  int i = 0;
  while (getline(output, line)) {
    if (i == 0) {
      path_ = line;
    } else if (i == 1) {
      tooltip_ = line;
    } else {
      break;
    }
    i++;
  }
}