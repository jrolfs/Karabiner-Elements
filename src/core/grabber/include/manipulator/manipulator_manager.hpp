#pragma once

#include "manipulator/manipulator_factory.hpp"

namespace krbn {
namespace manipulator {
class manipulator_manager final {
public:
  manipulator_manager(const manipulator_manager&) = delete;

  manipulator_manager(void) {
  }

  void push_back_manipulator(const nlohmann::json& json,
                             const core_configuration::profile::complex_modifications::parameters& parameters) {
    manipulators_.push_back(manipulator_factory::make_manipulator(json, parameters));
  }

  void push_back_manipulator(std::shared_ptr<details::base> ptr) {
    manipulators_.push_back(ptr);
  }

  void manipulate(event_queue& input_event_queue,
                  event_queue& output_event_queue) {
    while (!input_event_queue.empty()) {
      auto& front_input_event = input_event_queue.get_front_event();

      switch (front_input_event.get_event().get_type()) {
        case event_queue::queued_event::event::type::device_keys_are_released:
          output_event_queue.erase_all_active_modifier_flags_except_lock(front_input_event.get_device_id());
          break;

        case event_queue::queued_event::event::type::device_pointing_buttons_are_released:
          output_event_queue.erase_all_active_pointing_buttons_except_lock(front_input_event.get_device_id());
          break;

        case event_queue::queued_event::event::type::device_ungrabbed:
          // Reset modifier_flags and pointing_buttons before `handle_device_ungrabbed_event`
          // in order to send key_up events in `post_event_to_virtual_devices::handle_device_ungrabbed_event`.
          output_event_queue.erase_all_active_modifier_flags(front_input_event.get_device_id());
          output_event_queue.erase_all_active_pointing_buttons(front_input_event.get_device_id());
          for (auto&& m : manipulators_) {
            m->handle_device_ungrabbed_event(front_input_event.get_device_id(),
                                             output_event_queue,
                                             front_input_event.get_time_stamp());
          }
          break;

        case event_queue::queued_event::event::type::event_from_ignored_device:
          if (auto original_type = front_input_event.get_event().get_original_type()) {
            if (auto original_integer_value = front_input_event.get_event().get_original_integer_value()) {
              for (auto&& m : manipulators_) {
                m->handle_event_from_ignored_device(*original_type,
                                                    *original_integer_value,
                                                    front_input_event.get_event_type(),
                                                    output_event_queue,
                                                    front_input_event.get_time_stamp());
              }
            }
          }
          break;

        default:
          for (auto&& m : manipulators_) {
            m->manipulate(front_input_event,
                          input_event_queue,
                          output_event_queue);
          }
          break;
      }

      if (input_event_queue.get_front_event().get_valid()) {
        output_event_queue.push_back_event(input_event_queue.get_front_event());
      }

      input_event_queue.erase_front_event();
    }

    remove_invalid_manipulators();
  }

  void invalidate_manipulators(void) {
    for (auto&& m : manipulators_) {
      m->set_valid(false);
    }

    remove_invalid_manipulators();
  }

  size_t get_manipulators_size(void) {
    return manipulators_.size();
  }

private:
  void remove_invalid_manipulators(void) {
    manipulators_.erase(std::remove_if(std::begin(manipulators_),
                                       std::end(manipulators_),
                                       [](const auto& it) {
                                         // Keep active manipulators.
                                         return !it->get_valid() && !it->active();
                                       }),
                        std::end(manipulators_));
  }

  std::vector<std::shared_ptr<details::base>> manipulators_;
};
} // namespace manipulator
} // namespace krbn
