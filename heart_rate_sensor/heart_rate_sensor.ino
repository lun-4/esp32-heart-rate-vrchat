#define PLUS

#ifdef PLUS
#include <M5StickCPlus.h>
#else
#include <M5StickC.h>
#endif

void setup() {
  analogReadResolution(10);
  analogWriteResolution(10);
  pinMode(10, OUTPUT);
  pinMode(36, INPUT);
  M5.begin();
  M5.Lcd.print("hi");
  analogWrite(10, 1000);
  Serial.begin(9600);

  xTaskCreatePinnedToCore(sense_task, "sense", 10000, NULL, 10, NULL, 1);
}

void loop() {}

uint16_t sensor_samples[100];
uint8_t current_sample = 0;

TickType_t ticks_since_last_signal_threshold = 0;
uint8_t heart_rate = 0;

#define pdTICKS_TO_MS( xTicks )   ( ( ( TickType_t ) ( xTicks ) * 1000u ) / configTICK_RATE_HZ )

enum sense_task_state_t { OffBeat, InBeat };
sense_task_state_t current_state = OffBeat;
TickType_t in_beat_start_tick = 0;

// code options, use GRAPH if you want to see raw sensor data
// or RATE if you want to see the bpm
//#define GRAPH_SAMPLES
#define PRINT_RATE

void sense_task(void *param) {
  memset(&sensor_samples, 0, sizeof(sensor_samples));
  while (1) {
    TickType_t current_tick = xTaskGetTickCount();

    M5.Lcd.fillScreen(BLACK);

    // single sample coming from the g36 pin
    uint32_t raw_sample = analogRead(36);

    // TODO filter signal (moving average algo?)
    // TODO use map() to prevent us from doing total_delta

    sensor_samples[current_sample++] = raw_sample;
    if (current_sample == 100) current_sample = 0;

    // calculate threshold as 75% of the highest peak over
    // last 100 samples

    uint16_t highest_sample = 0;
    uint16_t lowest_sample = 0xffff;
    for (uint8_t i=0;i<100;i++) {
      if (sensor_samples[i] > highest_sample) highest_sample = sensor_samples[i];
      if (sensor_samples[i] < lowest_sample) lowest_sample = sensor_samples[i];
    }

    uint16_t total_delta = highest_sample - lowest_sample;
    uint16_t threshold = total_delta * 0.25;
    uint16_t signal_beat_threshold = highest_sample - threshold;

#ifdef GRAPH_SAMPLES
    Serial.print(highest_sample);
    Serial.print(",");
    Serial.print(lowest_sample);
    Serial.print(",");
    Serial.print(signal_beat_threshold);
    Serial.print(",");
    Serial.print(raw_sample);
    Serial.println();
#endif

    switch(current_state) {
      case OffBeat:
        if (raw_sample >= signal_beat_threshold) {
          // we are now in a heart beat
          in_beat_start_tick = current_tick;
          current_state = InBeat;
        }
        break;
      case InBeat:
        if (raw_sample < signal_beat_threshold) {
          // we are now out of the heart beat
          current_state = OffBeat;

          // average in_beat_start_tick and current_tick, that becomes ticks_since_last_signal_threshold
          TickType_t beat_tick = current_tick + in_beat_start_tick / 2;

          // only calculate heart rate if we already had a beat beforehand
          if (ticks_since_last_signal_threshold > 0) {
            TickType_t heart_rate_as_ticks = beat_tick - ticks_since_last_signal_threshold;
            uint32_t heart_rate_as_ms = pdTICKS_TO_MS(heart_rate_as_ticks);
            float heart_rate_as_sec = heart_rate_as_ms / 1000.0f;
            heart_rate = floor(heart_rate_as_sec * 60);
            #ifdef PRINT_RATE
            Serial.print("heart rate as ticks:");
            Serial.println(heart_rate_as_ticks);
            Serial.print("heart rate as ms:");
            Serial.println(heart_rate_as_ms);
            Serial.print("heart rate as sec");
            Serial.println(heart_rate_as_sec);
            Serial.print("bpm:");
            Serial.println(heart_rate);
            #endif
            analogWrite(10, 1023-map(raw_sample, lowest_sample, highest_sample, 0, 1023));
          }
          ticks_since_last_signal_threshold = beat_tick;
        }
        // if still in beat, ignore signal
        break;
    }

    M5.Lcd.setCursor(10, 10);
    M5.Lcd.print(raw_sample, DEC);
    M5.Lcd.setCursor(50, 50);
    M5.Lcd.print(heart_rate, DEC);
    //M5.Lcd.print(map(d, val_min, val_max, 0, 1023), DEC);
    
    vTaskDelay(pdMS_TO_TICKS(20));
    analogWrite(10, 1023);
  }
}
