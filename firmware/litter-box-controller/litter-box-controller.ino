/**
 * ============================================================================
 * AUTO-EMPTYING CAT LITTER BOX CONTROLLER
 * Production Firmware for ESP32
 * ============================================================================
 *
 * This firmware implements a strict finite state machine for controlling
 * a linear gantry-based auto-emptying cat litter box.
 *
 * Features:
 * - Load cell-based cat detection with weight averaging and outlier rejection
 * - Configurable cleaning delay timer (default: 10 minutes)
 * - Emergency stop via weight detection during cleaning cycle
 * - Precision homing with limit switches
 * - Belt-driven gantry control via TMC2209 stepper driver
 * - Optional servo control for motorized rake tilt
 *
 * Hardware Requirements:
 * - ESP32 DevKit (30-pin)
 * - TMC2209 Stepper Driver
 * - NEMA 17 Stepper Motor
 * - 4× 50kg Load Cells + HX711 ADC
 * - 2× Mechanical Limit Switches (NO)
 * - Optional: SG90 Servo for rake tilt
 *
 * Pin Assignments (see Phase 4 documentation):
 * - GPIO 16: STEP
 * - GPIO 17: DIR
 * - GPIO 18: ENABLE
 * - GPIO 19: HOME_SWITCH
 * - GPIO 21: FAR_SWITCH
 * - GPIO 22: HX711_DOUT
 * - GPIO 23: HX711_SCK
 * - GPIO 25: SERVO_PWM (optional)
 *
 * Author: Open-Source Project
 * License: See LICENSE file
 * Version: 1.0.0
 *
 * ============================================================================
 */

#include <Arduino.h>
#include <HX711.h>

// Optional: Uncomment to enable servo control
// #define SERVO_ENABLED
#ifdef SERVO_ENABLED
  #include <ESP32Servo.h>
#endif

// ============================================================================
// PIN DEFINITIONS
// ============================================================================

// Stepper Motor Control
const int STEP_PIN = 16;
const int DIR_PIN = 17;
const int ENABLE_PIN = 18;

// Limit Switches
const int HOME_SWITCH_PIN = 19;
const int FAR_SWITCH_PIN = 21;

// Load Cell (HX711)
const int HX711_DOUT_PIN = 22;
const int HX711_SCK_PIN = 23;

// Servo (Optional)
#ifdef SERVO_ENABLED
  const int SERVO_PIN = 25;
#endif

// ============================================================================
// USER-CONFIGURABLE PARAMETERS
// ============================================================================

// Weight Detection (kg)
const float CAT_THRESHOLD = 2.0;              // Minimum weight to detect cat presence
const float WEIGHT_STABILITY_TIME = 3.0;      // Seconds weight must be stable before logging
const float EMERGENCY_WEIGHT_THRESHOLD = 1.0; // Weight during cleaning triggers emergency stop

// Cleaning Delay (ms)
const unsigned long CLEANING_DELAY = 600000;  // 10 minutes (600,000 ms)
                                              // Change to 300000 for 5 minutes
                                              // Change to 900000 for 15 minutes

// Motion Parameters
const int STEPS_PER_MM = 100;                 // Steps per mm (20-tooth pulley, 1/16 microstep)
                                              // Calculate: (200 steps/rev * 16) / (20 teeth * 2mm pitch)
const int HOMING_SPEED = 20;                  // mm/s - Slow speed for homing
const int CLEANING_SPEED = 50;                // mm/s - Speed during sifting pass
const int RETURN_SPEED = 100;                 // mm/s - Fast return after cleaning

// Travel Limits (mm)
const int HOME_POSITION = 0;                  // Home position (motor end)
const int FAR_POSITION = 700;                 // Maximum travel before drop zone
const int DROP_POSITION = 750;                // Push waste through flap

// Timeout Values (ms)
const unsigned long HOMING_TIMEOUT = 30000;   // 30 seconds
const unsigned long MOVE_TIMEOUT = 60000;     // 60 seconds per move
const unsigned long DEBOUNCE_DELAY = 50;      // Switch debouncing

// Load Cell Calibration
float CALIBRATION_FACTOR = 22.5;              // Units per gram - CALIBRATE THIS!
long TARE_OFFSET = 0;                         // Raw offset - set during tare

// Servo Angles (if enabled)
#ifdef SERVO_ENABLED
  const int RAKE_ANGLE_SIFTING = 15;          // Degrees - forward angle for sifting
  const int RAKE_ANGLE_DUMPING = 45;          // Degrees - tilt angle for dumping waste
#endif

// Debug Mode
const bool DEBUG_ENABLED = true;              // Enable serial diagnostic output

// ============================================================================
// STATE MACHINE DEFINITIONS
// ============================================================================

enum State {
  HOMING,          // Initial homing sequence
  IDLE,            // Ready, monitoring for cat
  CAT_DETECTED,    // Cat weight detected, checking stability
  WAITING,         // Countdown delay before cleaning
  CLEANING,        // Active cleaning cycle
  RETURNING,       // Returning to home after cleaning
  ERROR            // Fault state (requires reset)
};

State currentState = HOMING;
int currentPosition = 0;  // Current position in mm

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================

HX711 scale;

#ifdef SERVO_ENABLED
  Servo rakeServo;
#endif

// ============================================================================
// WEIGHT FILTERING
// ============================================================================

#define WEIGHT_BUFFER_SIZE 20
float weightBuffer[WEIGHT_BUFFER_SIZE];
int weightBufferIndex = 0;
bool weightBufferFilled = false;

// Helper functions
float calculateMean(float* buffer, int size) {
  float sum = 0;
  for (int i = 0; i < size; i++) {
    sum += buffer[i];
  }
  return sum / size;
}

float calculateStdDev(float* buffer, int size, float mean) {
  float sumSquares = 0;
  for (int i = 0; i < size; i++) {
    float diff = buffer[i] - mean;
    sumSquares += diff * diff;
  }
  return sqrt(sumSquares / size);
}

float readFilteredWeight() {
  // Check if HX711 is ready
  if (!scale.is_ready()) {
    return 0.0;
  }

  // Read raw value (average 3 samples)
  long rawValue = scale.read_average(3);

  // Convert to weight in grams
  float currentWeight = (rawValue - TARE_OFFSET) / CALIBRATION_FACTOR;

  // Add to circular buffer
  weightBuffer[weightBufferIndex] = currentWeight;
  weightBufferIndex = (weightBufferIndex + 1) % WEIGHT_BUFFER_SIZE;
  if (weightBufferIndex == 0) weightBufferFilled = true;

  // If buffer not filled yet, return current weight
  if (!weightBufferFilled) {
    return currentWeight / 1000.0; // Convert to kg
  }

  // Calculate average excluding outliers
  float mean = calculateMean(weightBuffer, WEIGHT_BUFFER_SIZE);
  float stdDev = calculateStdDev(weightBuffer, WEIGHT_BUFFER_SIZE, mean);

  float sum = 0;
  int validSamples = 0;

  for (int i = 0; i < WEIGHT_BUFFER_SIZE; i++) {
    // Reject samples > 2 standard deviations from mean
    if (abs(weightBuffer[i] - mean) < 2 * stdDev) {
      sum += weightBuffer[i];
      validSamples++;
    }
  }

  // Return filtered average in kg
  return (validSamples > 0) ? (sum / validSamples) / 1000.0 : 0.0;
}

// ============================================================================
// EMERGENCY STOP
// ============================================================================

volatile bool emergencyStopFlag = false;

void IRAM_ATTR emergencyStopISR() {
  // This is called from FreeRTOS task, not a hardware interrupt
  // Just sets the flag for main loop to handle
  emergencyStopFlag = true;
}

// FreeRTOS task for emergency stop monitoring
void emergencyStopTask(void *parameter) {
  while (true) {
    if (currentState == CLEANING) {
      float weight = readFilteredWeight();
      if (weight > EMERGENCY_WEIGHT_THRESHOLD) {
        emergencyStopISR();
      }
    }
    vTaskDelay(50 / portTICK_PERIOD_MS); // Check every 50ms
  }
}

void handleEmergencyStop() {
  // Immediately disable motor
  digitalWrite(ENABLE_PIN, HIGH);

  // Transition to IDLE
  currentState = IDLE;

  // Reset flag
  emergencyStopFlag = false;

  Serial.println("⚠️  EMERGENCY STOP: Weight detected during cleaning! System halted.");
  Serial.println("Returning to IDLE state.");
}

// ============================================================================
// MOTION CONTROL
// ============================================================================

void moveTo(int targetPositionMm, int speedMmPerSec) {
  long targetSteps = (long)targetPositionMm * STEPS_PER_MM;
  long currentSteps = (long)currentPosition * STEPS_PER_MM;
  long deltaSteps = abs(targetSteps - currentSteps);

  if (deltaSteps == 0) return; // Already at target

  // Set direction
  if (targetSteps > currentSteps) {
    digitalWrite(DIR_PIN, HIGH); // Forward
  } else {
    digitalWrite(DIR_PIN, LOW);  // Reverse
  }

  // Calculate step delay for desired speed
  unsigned long stepDelayMicros = 1000000 / (speedMmPerSec * STEPS_PER_MM);

  // Enable motor
  digitalWrite(ENABLE_PIN, LOW);

  // Move with timeout protection
  unsigned long moveStartTime = millis();

  for (long i = 0; i < deltaSteps; i++) {
    // Emergency stop check
    if (emergencyStopFlag) {
      digitalWrite(ENABLE_PIN, HIGH);
      return;
    }

    // Timeout check
    if (millis() - moveStartTime > MOVE_TIMEOUT) {
      currentState = ERROR;
      digitalWrite(ENABLE_PIN, HIGH);
      Serial.println("❌ ERROR: Move timeout exceeded");
      return;
    }

    // Check limit switches
    if (digitalRead(HOME_SWITCH_PIN) == HIGH) {
      if (targetPositionMm == HOME_POSITION) {
        currentPosition = HOME_POSITION;
        return; // Successfully reached home
      } else {
        // Unexpected home switch trigger
        currentState = ERROR;
        digitalWrite(ENABLE_PIN, HIGH);
        Serial.println("❌ ERROR: Home switch triggered unexpectedly");
        return;
      }
    }

    if (digitalRead(FAR_SWITCH_PIN) == HIGH) {
      currentState = ERROR;
      digitalWrite(ENABLE_PIN, HIGH);
      Serial.println("❌ ERROR: Far limit switch triggered (should not reach)");
      return;
    }

    // Step pulse
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(10); // Minimum pulse width for TMC2209
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(stepDelayMicros);
  }

  // Update position
  currentPosition = targetPositionMm;
}

// ============================================================================
// HOMING SEQUENCE
// ============================================================================

void performHoming() {
  Serial.println("🏠 Starting homing sequence...");

  // Enable motor
  digitalWrite(ENABLE_PIN, LOW);

  // Set direction toward home (reverse)
  digitalWrite(DIR_PIN, LOW);

  // Move slowly until home switch is triggered
  unsigned long homingStartTime = millis();
  bool homeSwitchFound = false;

  while (digitalRead(HOME_SWITCH_PIN) == LOW) {
    // Timeout check
    if (millis() - homingStartTime > HOMING_TIMEOUT) {
      currentState = ERROR;
      Serial.println("❌ ERROR: Homing timeout - home switch not found");
      digitalWrite(ENABLE_PIN, HIGH);
      return;
    }

    // Step pulse
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(1000000 / (HOMING_SPEED * STEPS_PER_MM));
  }

  // Home switch triggered, back off 5mm
  Serial.println("✓ Home switch contacted, backing off...");
  delay(DEBOUNCE_DELAY);

  digitalWrite(DIR_PIN, HIGH); // Forward
  for (int i = 0; i < 5 * STEPS_PER_MM; i++) {
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(1000);
  }

  delay(100);

  // Precision homing (very slow approach)
  Serial.println("🎯 Precision homing...");
  digitalWrite(DIR_PIN, LOW); // Reverse

  while (digitalRead(HOME_SWITCH_PIN) == LOW) {
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(5000); // Very slow (5mm/s)
  }

  delay(DEBOUNCE_DELAY);

  // Set position to zero
  currentPosition = HOME_POSITION;
  Serial.println("✅ Homing complete. Position = 0 mm");

  // Transition to IDLE
  currentState = IDLE;
  Serial.println("STATE: IDLE");
}

// ============================================================================
// RAKE ANGLE CONTROL (if servo enabled)
// ============================================================================

#ifdef SERVO_ENABLED
void setRakeAngle(int angleDegrees) {
  rakeServo.write(angleDegrees);
  Serial.print("🔧 Rake angle set to ");
  Serial.print(angleDegrees);
  Serial.println("°");
}
#endif

// ============================================================================
// CLEANING SEQUENCE
// ============================================================================

void executeCleaningSequence() {
  Serial.println("🧹 Starting cleaning sequence...");

  // STEP 1: Move to home position (if not already there)
  if (currentPosition != HOME_POSITION) {
    Serial.println("Moving to home position...");
    moveTo(HOME_POSITION, CLEANING_SPEED);
    if (emergencyStopFlag || currentState == ERROR) return;
  }

  // STEP 2: Set rake to sifting angle
  #ifdef SERVO_ENABLED
    setRakeAngle(RAKE_ANGLE_SIFTING);
    delay(500);
  #endif

  // STEP 3: Sifting pass (home → far position)
  Serial.println("Performing sifting pass...");
  moveTo(FAR_POSITION, CLEANING_SPEED);
  if (emergencyStopFlag || currentState == ERROR) return;

  // STEP 4: Tilt rake to dump angle
  #ifdef SERVO_ENABLED
    setRakeAngle(RAKE_ANGLE_DUMPING);
    delay(500);
  #endif

  // STEP 5: Push waste through flap
  Serial.println("Pushing waste through flap...");
  moveTo(DROP_POSITION, CLEANING_SPEED);
  if (emergencyStopFlag || currentState == ERROR) return;

  delay(1000); // Give time for waste to drop

  // STEP 6: Return rake to sifting angle
  #ifdef SERVO_ENABLED
    setRakeAngle(RAKE_ANGLE_SIFTING);
    delay(500);
  #endif

  // STEP 7: Fast return to home
  Serial.println("Returning to home...");
  moveTo(HOME_POSITION, RETURN_SPEED);
  if (emergencyStopFlag || currentState == ERROR) return;

  // STEP 8: Mark cleaning complete
  currentState = RETURNING;
  Serial.println("✅ Cleaning sequence complete");
}

// ============================================================================
// STATE MACHINE
// ============================================================================

unsigned long stateEnterTime = 0;
unsigned long lastDebugOutput = 0;

void updateStateMachine() {
  float currentWeight = readFilteredWeight();

  switch (currentState) {
    case HOMING:
      // Handled in setup() and performHoming()
      break;

    case IDLE:
      // Monitor for cat presence
      if (currentWeight > CAT_THRESHOLD) {
        currentState = CAT_DETECTED;
        stateEnterTime = millis();
        Serial.println("STATE: CAT_DETECTED");
        Serial.print("Weight detected: ");
        Serial.print(currentWeight);
        Serial.println(" kg");
      }
      break;

    case CAT_DETECTED:
      // Check if weight is stable for required time
      if (currentWeight < CAT_THRESHOLD) {
        currentState = IDLE;
        Serial.println("STATE: IDLE (false alarm - weight dropped)");
      } else if (millis() - stateEnterTime > WEIGHT_STABILITY_TIME * 1000) {
        currentState = WAITING;
        stateEnterTime = millis();
        Serial.println("STATE: WAITING");
        Serial.print("Cleaning will start in ");
        Serial.print(CLEANING_DELAY / 1000);
        Serial.println(" seconds...");
      }
      break;

    case WAITING:
      // Wait for cleaning delay, abort if cat returns
      if (currentWeight > CAT_THRESHOLD) {
        currentState = IDLE;
        Serial.println("STATE: IDLE (cat re-entry detected, aborting cleaning)");
      } else if (millis() - stateEnterTime > CLEANING_DELAY) {
        currentState = CLEANING;
        stateEnterTime = millis();
        Serial.println("STATE: CLEANING");
      }
      break;

    case CLEANING:
      // Handled in executeState()
      break;

    case RETURNING:
      // Transition to IDLE after cleaning
      currentState = IDLE;
      Serial.println("STATE: IDLE (ready for next cycle)");
      break;

    case ERROR:
      // Halt system
      digitalWrite(ENABLE_PIN, HIGH); // Disable motor
      if (millis() - lastDebugOutput > 5000) {
        Serial.println("❌ ERROR STATE: System halted. Check hardware and power cycle to reset.");
        lastDebugOutput = millis();
      }
      break;
  }
}

void executeState() {
  switch (currentState) {
    case CLEANING:
      executeCleaningSequence();
      break;

    default:
      // Other states don't require active execution
      break;
  }
}

// ============================================================================
// DIAGNOSTICS
// ============================================================================

void printDiagnostics() {
  Serial.println("\n========== DIAGNOSTICS ==========");

  Serial.print("State: ");
  switch (currentState) {
    case HOMING: Serial.println("HOMING"); break;
    case IDLE: Serial.println("IDLE"); break;
    case CAT_DETECTED: Serial.println("CAT_DETECTED"); break;
    case WAITING: Serial.println("WAITING"); break;
    case CLEANING: Serial.println("CLEANING"); break;
    case RETURNING: Serial.println("RETURNING"); break;
    case ERROR: Serial.println("ERROR"); break;
  }

  Serial.print("Weight: ");
  Serial.print(readFilteredWeight(), 2);
  Serial.println(" kg");

  Serial.print("Position: ");
  Serial.print(currentPosition);
  Serial.println(" mm");

  Serial.print("Home Switch: ");
  Serial.println(digitalRead(HOME_SWITCH_PIN) == HIGH ? "TRIGGERED" : "OPEN");

  Serial.print("Far Switch: ");
  Serial.println(digitalRead(FAR_SWITCH_PIN) == HIGH ? "TRIGGERED" : "OPEN");

  Serial.println("=================================\n");
}

// ============================================================================
// CALIBRATION
// ============================================================================

void calibrateLoadCells() {
  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║  LOAD CELL CALIBRATION PROCEDURE  ║");
  Serial.println("╚════════════════════════════════════╝\n");

  Serial.println("Step 1: Remove all weight from platform.");
  Serial.println("Press any key and Enter when ready...");
  while (!Serial.available()) delay(100);
  while (Serial.available()) Serial.read(); // Clear buffer

  Serial.println("Taring scale...");
  scale.tare(10); // Average 10 readings
  TARE_OFFSET = scale.read_average(10);
  Serial.println("✓ Platform tared (zero set).");
  Serial.print("Tare offset: ");
  Serial.println(TARE_OFFSET);

  Serial.println("\nStep 2: Place a known weight on the platform.");
  Serial.println("Enter the weight in grams (e.g., 5000 for 5kg) and press Enter:");
  while (!Serial.available()) delay(100);
  long knownWeight = Serial.parseInt();
  while (Serial.available()) Serial.read(); // Clear buffer

  Serial.println("Reading load cells...");
  long rawValue = scale.read_average(10);
  Serial.print("Raw value with known weight: ");
  Serial.println(rawValue);

  // Calculate calibration factor
  float calibrationFactor = rawValue / (float)knownWeight;
  Serial.print("\n✓ Calibration factor: ");
  Serial.println(calibrationFactor, 4);

  Serial.println("\n╔════════════════════════════════════════════╗");
  Serial.println("║  UPDATE FIRMWARE WITH THIS VALUE:         ║");
  Serial.print("║  CALIBRATION_FACTOR = ");
  Serial.print(calibrationFactor, 4);
  Serial.println("         ║");
  Serial.println("╚════════════════════════════════════════════╝\n");

  Serial.println("Re-upload firmware with updated value, then restart.");
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  // Initialize serial
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n╔═══════════════════════════════════════════╗");
  Serial.println("║  AUTO-EMPTYING CAT LITTER BOX v1.0.0     ║");
  Serial.println("║  ESP32 Firmware - Open Source Project    ║");
  Serial.println("╚═══════════════════════════════════════════╝\n");

  // Initialize GPIO
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  pinMode(HOME_SWITCH_PIN, INPUT);
  pinMode(FAR_SWITCH_PIN, INPUT);

  // Disable motor initially
  digitalWrite(ENABLE_PIN, HIGH);
  digitalWrite(STEP_PIN, LOW);

  Serial.println("✓ GPIO initialized");

  // Initialize HX711
  scale.begin(HX711_DOUT_PIN, HX711_SCK_PIN);
  Serial.println("✓ HX711 initialized");

  // Check if calibration is needed
  if (CALIBRATION_FACTOR == 0.0) {
    Serial.println("⚠️  Calibration factor not set!");
    Serial.println("Starting calibration procedure...\n");
    calibrateLoadCells();
    while (true) delay(1000); // Halt until firmware updated
  }

  // Set calibration factor
  scale.set_scale(CALIBRATION_FACTOR);
  scale.tare(10);
  Serial.println("✓ Load cells calibrated and tared");

  // Initialize servo (if enabled)
  #ifdef SERVO_ENABLED
    rakeServo.attach(SERVO_PIN);
    setRakeAngle(RAKE_ANGLE_SIFTING);
    Serial.println("✓ Servo initialized");
  #endif

  // Start emergency stop monitoring task
  xTaskCreatePinnedToCore(
    emergencyStopTask,   // Task function
    "EmergencyStop",     // Task name
    2048,                // Stack size
    NULL,                // Parameters
    2,                   // Priority (high)
    NULL,                // Task handle
    0                    // Core 0
  );
  Serial.println("✓ Emergency stop task started");

  // Perform initial homing
  Serial.println("\nInitializing system...");
  performHoming();

  if (currentState == ERROR) {
    Serial.println("❌ Homing failed. Check hardware connections.");
    while (true) delay(1000);
  }

  Serial.println("\n✅ System ready!");
  Serial.println("Monitoring for cat presence...\n");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  // 1. Update state machine
  updateStateMachine();

  // 2. Execute state-specific actions
  executeState();

  // 3. Handle emergency stop
  if (emergencyStopFlag) {
    handleEmergencyStop();
  }

  // 4. Diagnostic output (if enabled)
  if (DEBUG_ENABLED && millis() - lastDebugOutput > 10000) {
    printDiagnostics();
    lastDebugOutput = millis();
  }

  // 5. Small delay (yield to FreeRTOS)
  delay(10);
}

// ============================================================================
// END OF FIRMWARE
// ============================================================================

/**
 * TROUBLESHOOTING:
 *
 * 1. Motor not moving:
 *    - Check 24V power to TMC2209
 *    - Verify ENABLE pin is LOW during movement
 *    - Measure STEP pulses on oscilloscope
 *    - Adjust TMC2209 current limit (V_REF potentiometer)
 *
 * 2. Load cells reading zero:
 *    - Run calibration procedure
 *    - Check HX711 wiring (DT, SCK, VCC, GND)
 *    - Verify load cell connections (E+/E-/S+/S-)
 *
 * 3. Homing fails:
 *    - Check home switch wiring and polarity
 *    - Test switch continuity with multimeter
 *    - Verify pull-down resistor (10kΩ to GND)
 *
 * 4. System resets during motor movement:
 *    - Add 1000µF capacitor across 5V rail (near ESP32)
 *    - Check buck converter output voltage under load
 *    - Reduce motor current limit
 *
 * 5. Emergency stop not triggering:
 *    - Verify weight reading in diagnostics output
 *    - Lower EMERGENCY_WEIGHT_THRESHOLD
 *    - Check FreeRTOS task is running (add debug prints)
 */
