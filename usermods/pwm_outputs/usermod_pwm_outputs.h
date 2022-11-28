#pragma once
#include "wled.h"

#ifndef ESP32
  #error This usermod does not support the ESP8266.
#endif

#ifndef USERMOD_PWM_OUTPUT_PINS
  #define USERMOD_PWM_OUTPUT_PINS 3
#endif


class PwmOutput {
  public:

    void open(int8_t pin, uint32_t freq) {
      DEBUG_PRINTF("pwm_output[%d]: setup to freq %d\n", pin_, freq_);

      if (enabled_) {
        if (pin == pin && freq == freq_) {
          return;  // PWM output is already open
        } else {
          close();  // Config has changed, close and reopen
        }
      }

      pin_ = pin;
      freq_ = freq;
      if (pin_ < 0 || !pinManager.allocatePin(pin_, true, PinOwner::UM_PWM_OUTPUTS))
        return;
      
      channel_ = pinManager.allocateLedc(1);
      if (channel_ == 255) {
        DEBUG_PRINTF("pwm_output[%d]: failed to quire ledc\n", pin_);
        pinManager.deallocatePin(pin_, PinOwner::UM_PWM_OUTPUTS);
        return;
      }

      ledcSetup(channel_, freq_, bit_depth_);
      ledcAttachPin(pin_, channel_);
      DEBUG_PRINTF("pwm_output[%d]: init successful\n", pin_);
      enabled_ = true;
    }

    void close() {
      DEBUG_PRINTF("pwm_output[%d]: close\n", pin_);
      if (!enabled_)
        return;
      pinManager.deallocatePin(pin_, PinOwner::UM_PWM_OUTPUTS);
      if (channel_ != 255)
        pinManager.deallocateLedc(channel_, 1);
      channel_ = 255;
      duty_ = 0.0f;
      enabled_ = false;
    }

    void setDuty(const float duty) {
      DEBUG_PRINTF("pwm_output[%d]: set duty %f\n", pin_, duty);
      if (!enabled_)
        return;
      duty_ = min(1.0f, max(0.0f, duty));
      const uint32_t value = static_cast<uint32_t>((1 << bit_depth_) * duty_);
      ledcWrite(channel_, value);
    }

    bool isEnabled() const {
      return enabled_;
    }

    void addToJsonState(JsonObject& pwmState) const {
      pwmState[F("duty")] = duty_;
    }

    void readFromJsonState(JsonObject& pwmState) {
      if (pwmState.isNull())
        return;
      getJsonValue(pwmState[F("duty")], duty_);
    }

    void addToJsonInfo(JsonObject& user) const {
      if (!enabled_)
        return;
      JsonArray data = user.createNestedArray("PWM pin " + String(pin_));
      data.add(1e2f * duty_);
      data.add(F("%"));
    }

    void addToConfig(JsonObject& pwmConfig) const {
      pwmConfig[F("pin")] = pin_;
      pwmConfig[F("freq")] = freq_;
    }

    bool readFromConfig(JsonObject& pwmConfig) {
      if (pwmConfig.isNull())
        return false;
        
      bool configComplete = false;
      int8_t newPin = pin_;
      uint32_t newFreq = freq_;
      configComplete &= getJsonValue(pwmConfig[F("pin")], newPin);  
      configComplete &= getJsonValue(pwmConfig[F("freq")], newFreq);  
      
      open(newPin, newFreq);

      return configComplete;
    }

  private:
    int8_t pin_ {-1};
    uint32_t freq_ {50};
    static const uint8_t bit_depth_ {12};
    uint8_t channel_ {255};
    float duty_ {0.0f};
    bool enabled_ {false};
};


class PwmOutputsUsermod : public Usermod {
  public:

    static const char USERMOD_NAME[];

    void setup() {
      // By default all PWM outputs are disabled, no setup do be done
    }

    void loop() {
    }

    void addToJsonState(JsonObject& root) {
      JsonObject pwmStates = root.createNestedObject(USERMOD_NAME);
      for (int i = 0; i < USERMOD_PWM_OUTPUT_PINS; i++) {
        const PwmOutput& pwm = pwms_[i];
        if (!pwm.isEnabled())
          continue;
        JsonObject pwmState = pwmStates.createNestedObject(String(i));
        pwm.addToJsonState(pwmState);
      }
    }

    void readFromJsonState(JsonObject& root) {
      JsonObject pwmStates = root[USERMOD_NAME];
      if (pwmStates.isNull())
        return;

      for (int i = 0; i < USERMOD_PWM_OUTPUT_PINS; i++) {
        PwmOutput& pwm = pwms_[i];
        if (!pwm.isEnabled())
          continue;
        JsonObject pwmState = root[String(i)];
        pwm.readFromJsonState(pwmState);
      }
    }

    void addToJsonInfo(JsonObject& root) {
      JsonObject user = root[F("u")];
      if (user.isNull())
        user = root.createNestedObject(F("u"));

      for (int i = 0; i < USERMOD_PWM_OUTPUT_PINS; i++) {
        const PwmOutput& pwm = pwms_[i];
        pwm.addToJsonInfo(user);
      }
    }

    void addToConfig(JsonObject& root) {
      JsonObject top = root.createNestedObject(USERMOD_NAME);
      for (int i = 0; i < USERMOD_PWM_OUTPUT_PINS; i++) {
        const PwmOutput& pwm = pwms_[i];
        JsonObject pwmConfig = top.createNestedObject(String(i));
        pwm.addToConfig(pwmConfig);
      }
    }

    bool readFromConfig(JsonObject& root) {
      JsonObject top = root[USERMOD_NAME];
      bool configComplete = !top.isNull();
      for (int i = 0; i < USERMOD_PWM_OUTPUT_PINS; i++) {
        PwmOutput& pwm = pwms_[i];
        JsonObject pwmConfig = root[String(i)];
        configComplete &= pwm.readFromConfig(pwmConfig);
      }
      return configComplete;
    }

    uint16_t getId() {
      return USERMOD_ID_PWM_OUTPUTS;
    }

  private:
    PwmOutput pwms_[USERMOD_PWM_OUTPUT_PINS];

};

const char PwmOutputsUsermod::USERMOD_NAME[] PROGMEM = "PwmOutputs";
