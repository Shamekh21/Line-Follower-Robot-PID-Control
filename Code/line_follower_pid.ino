// ===================== Motor Driver =====================
#define ENA 5
#define IN1 9
#define IN2 10
#define ENB 6
#define IN3 11
#define IN4 12

// ===================== Sensors ==========================
#define S1 8
#define S2 4
#define S3 13
#define S4 3
#define S5 2

const int sensorPins[5] = {S1, S2, S3, S4, S5};

// إذا الحساسات بتدي LOW على الأسود
const int BLACK = LOW;
const int WHITE = HIGH;

// ===================== Control ==========================
int baseSpeed = 120;
int maxSpeed  = 255;
int minSpeed  = 0;

// ===================== PID Tuned ========================
float Kp = 3.5;
float Kd = 1.8;
float Ki = 0.015;

// ===================== PID Variables ====================
float lastError = 0;
float integral  = 0;

// ===================== Timing ===========================
unsigned long lastTime = 0;
const int loopTime = 2;

// ===================== Motor Calibration ================
float motorTrimLeft  = 1.00;
float motorTrimRight = 1.00;

// ===================== Setup ============================
void setup() {
  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);

  pinMode(ENB, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  for (int i = 0; i < 5; i++) {
    pinMode(sensorPins[i], INPUT);
  }

  Serial.begin(9600);

  stopMotors();
  delay(2000);
}

// ===================== Main Loop ========================
void loop() {
  if (millis() - lastTime < loopTime) return;
  lastTime = millis();

  // قراءة الحساسات
  int s1 = digitalRead(S1);
  int s2 = digitalRead(S2);
  int s3 = digitalRead(S3);
  int s4 = digitalRead(S4);
  int s5 = digitalRead(S5);

  // حساب الخطأ
  int error = getError(s1, s2, s3, s4, s5);

  // Debug على السيريال
  Serial.print(s1); Serial.print(" ");
  Serial.print(s2); Serial.print(" ");
  Serial.print(s3); Serial.print(" ");
  Serial.print(s4); Serial.print(" ");
  Serial.print(s5); Serial.print("  E=");
  Serial.println(error);

  // ===================== FIX #1 ========================
  // لو الخط ضاع: اتجاه الدوران صُحِّح
  // lastError < 0 → الخط كان على اليسار → ادور يسار
  // lastError >= 0 → الخط كان على اليمين → ادور يمين
  if (error == 100) {
    if (lastError < 0)
      turnLeftHard();   // كان على اليسار → ارجع يسار
    else
      turnRightHard();  // كان على اليمين → ارجع يمين
    return;
  }

  // ================= PID =================
  integral += error;
  integral = constrain(integral, -20, 20);

  // Derivative مع تخفيف
  float derivative = (error - lastError) * 0.7;

  float correction = (Kp * error) +
                     (Kd * derivative) +
                     (Ki * integral);

  correction = constrain(correction, -70, 70);

  lastError = error;

  // حساب السرعات
  int leftSpeed  = baseSpeed - correction;
  int rightSpeed = baseSpeed + correction;

  leftSpeed  = constrain((int)(leftSpeed  * motorTrimLeft),  minSpeed, maxSpeed);
  rightSpeed = constrain((int)(rightSpeed * motorTrimRight), minSpeed, maxSpeed);

  // Deadband
  if (error == 0) {
    forward(baseSpeed, baseSpeed);
  }
  else if (abs(error) == 1) {
    int smallCorrection = (int)(correction * 0.45);
    leftSpeed  = constrain(baseSpeed - smallCorrection, minSpeed, maxSpeed);
    rightSpeed = constrain(baseSpeed + smallCorrection, minSpeed, maxSpeed);
    forward(leftSpeed, rightSpeed);
  }
  else {
    forward(leftSpeed, rightSpeed);
  }
}

// ===================== FIX #2 ========================
// getError() مُحسَّنة - تغطي جميع حالات الحساسات المزدوجة والثلاثية
// تمنع الإرجاع الخاطئ لـ 100 (خط مفقود) وقت المنعطفات
int getError(int s1, int s2, int s3, int s4, int s5) {

  // كل الحساسات بيشوفوا أبيض فقط → الخط ضاع فعلًا
  if (s1 == WHITE && s2 == WHITE && s3 == WHITE && s4 == WHITE && s5 == WHITE)
    return 100;

  // ===== تقاطع أو خط عريض =====
  if (s1 == BLACK && s2 == BLACK && s3 == BLACK && s4 == BLACK && s5 == BLACK)
    return 0;  // تقاطع كامل → امشي مستقيم

  if (s2 == BLACK && s3 == BLACK && s4 == BLACK)
    return 0;  // خط عريض مركزي

  // ===== المنتصف =====
  if (s3 == BLACK && s2 == WHITE && s4 == WHITE)
    return 0;

  // ===== انحراف بسيط =====
  if (s2 == BLACK && s3 == WHITE)
    return -1;

  if (s4 == BLACK && s3 == WHITE)
    return 1;

  // ===== انحراف كبير =====
  if (s1 == BLACK && s2 == WHITE)
    return -2;

  if (s5 == BLACK && s4 == WHITE)
    return 2;

  // ===== حالات مزدوجة =====
  if (s2 == BLACK && s3 == BLACK)
    return -1;

  if (s3 == BLACK && s4 == BLACK)
    return 1;

  if (s1 == BLACK && s2 == BLACK)
    return -2;

  if (s4 == BLACK && s5 == BLACK)
    return 2;

  // ===== حالات ثلاثية (جديدة - تمنع الخطأ عند المنعطفات) =====
  if (s1 == BLACK && s2 == BLACK && s3 == BLACK)
    return -2;

  if (s3 == BLACK && s4 == BLACK && s5 == BLACK)
    return 2;

  // ===== حالة واحدة فقط على الأطراف =====
  if (s1 == BLACK)
    return -2;

  if (s5 == BLACK)
    return 2;

  // الخط ضاع فعلًا
  return 100;
}

// ===================== Motor Control ====================
void setLeftMotor(int speed) {
  speed = constrain(speed, -255, 255);
  if (speed >= 0) {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    analogWrite(ENA, speed);
  } else {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
    analogWrite(ENA, -speed);
  }
}

void setRightMotor(int speed) {
  speed = constrain(speed, -255, 255);
  if (speed >= 0) {
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
    analogWrite(ENB, speed);
  } else {
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);
    analogWrite(ENB, -speed);
  }
}

void forward(int leftSpeed, int rightSpeed) {
  setLeftMotor(leftSpeed);
  setRightMotor(rightSpeed);
}

// ===================== FIX #1 (دوران قوي مصحح) =========
// turnLeftHard: الموتور الأيمن للأمام، الأيسر للخلف
void turnLeftHard() {
  setLeftMotor(-100);
  setRightMotor(160);
}

// turnRightHard: الموتور الأيسر للأمام، الأيمن للخلف
void turnRightHard() {
  setLeftMotor(160);
  setRightMotor(-100);
}

void stopMotors() {
  setLeftMotor(0);
  setRightMotor(0);
}
