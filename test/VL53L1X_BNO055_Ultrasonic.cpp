#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>
#include <VL53L1X.h>

VL53L1X sensor;
#define BNO055_SAMPLERATE_DELAY_MS (10)
#define Wire1_SDA (33)
#define Wire1_SCL (32)
Adafruit_BNO055 bno = Adafruit_BNO055(-1, 0x28, &Wire1);
TaskHandle_t thp[4];

// data of each thread------------
// loop(tof)
int tof_mm;
// Core0GyS(Gyro)
int Gyro_degree[3];
// Core1USL(ultrasonic-left)
int ultrasonic_left_cm;
// Core0USR(ultraconic-right)
int ultrasonic_right_cm;

bool canUpdate = true;

void setup()
{
  Serial.begin(115200);
  Wire1.begin(Wire1_SDA, Wire1_SCL);
  Wire.begin();
  Wire.setClock(100000); // use 100 kHz I2C

  pinMode(5, OUTPUT);
  pinMode(18, OUTPUT);

  sensor.setTimeout(500);
  if (!sensor.init())
  {
    Serial.println("Failed to detect and initialize sensor!");
    while (1);
  }
  sensor.setDistanceMode(VL53L1X::Long); // long-mode
  sensor.setMeasurementTimingBudget(50000);
  sensor.startContinuous(10);

  if(!bno.begin())
  {
    /* There was a problem detecting the BNO055 ... check your connections */
    Serial.print("Ooops, no BNO055 detected ... Check your wiring or I2C ADDR!");
    while(1);
  }

  delay(1000);

  /* Use external crystal for better accuracy */
  bno.setExtCrystalUse(true);
  xTaskCreatePinnedToCore(Core0GyS, "CoreGyS", 4096, NULL, 3, &thp[0], 0); 
  xTaskCreatePinnedToCore(Core1US, "Core1US", 4096, NULL, 3, &thp[1], 1);
  xTaskCreatePinnedToCore(Core1tof, "Core1tof", 4096, NULL, 3, &thp[3], 1);
}

// main-loop------------------------------------------------------------------------------
void loop()
{
  delay(100);
  delayMicroseconds(10);
  Serial.print("tof;");
  Serial.println(tof_mm);
  Serial.print("Gyro;");
  for (int i = 0; i < 3; ++i) {
    Serial.print(Gyro_degree[i]);
    Serial.print(",");
  }
  Serial.println();
  Serial.print("ultrasonic left;");
  Serial.println(ultrasonic_left_cm);
  Serial.print("ultrasonic right;");
  Serial.println(ultrasonic_right_cm);
}

// tof------------------------------------------------------------------------------------
void Core1tof(void *args) {//Core1で実行するプログラム
  while (1) {//ここで無限ループを作っておく
    int tof_mm_tmp =sensor.read();
    if (sensor.timeoutOccurred()) { // タイムアウトの監視 
      Serial.print(" TIMEOUT");
      vlxReset(19);
    }
    if (canUpdate){
      tof_mm = tof_mm_tmp;
    }
  }
}

// restart tof - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void vlxReset(uint8_t resetPin) {
  Serial.println("Resetting Sensor");
 /* Stop the Sensor reading and even stop the i2c Transmission */
  sensor.stopContinuous();
  Wire.endTransmission();

/* Reset the sensor over here, you can change the delay */
  digitalWrite(resetPin, LOW);
  vTaskDelay(600 / portTICK_PERIOD_MS);  // Change to delay(600); if not using rtos
  digitalWrite(resetPin, HIGH);
  vTaskDelay(600 / portTICK_PERIOD_MS);

  if (!sensor.init()) {
    ESP_LOGE(TAG, "Failed To Detect Sensor.. Restarting!!");
    ESP.restart();
  }
  sensor.setTimeout(500);

  sensor.startContinuous(10);
}

// Gyro-------------------------------------------------------------------------------------
void Core0GyS(void *args) {//サブCPU(Core0)で実行するプログラム
  while (1) {//ここで無限ループを作っておく
    /* Get a new sensor event */
    sensors_event_t event;
    bno.getEvent(&event);

    /* The processing sketch expects data as roll, pitch, heading */
    int xyz_degree[] = {(int)event.orientation.x,(int)event.orientation.y,(int)event.orientation.z};

    if (canUpdate){
      for (int i = 0; i < 3; ++i) {
        Gyro_degree[i] = xyz_degree[i];
      }
    }

    delay(BNO055_SAMPLERATE_DELAY_MS);
  }
}

// ultrasonic-Left--------------------------------------------------------------------------
void Core1US(void *args) {// Core1で実行するプログラム
  const int pingPin_left = 18; // set for 18 pin
  unsigned long duration_left;
  int cm_left;
  const int pingPin_right = 5; // set for 5 pin
  unsigned long duration_right;
  int cm_right;
  while (1) {//ここで無限ループを作っておく
    // 18-pin-------------------------------------------------------------------------------
    //ピンをOUTPUTに設定（パルス送信のため）
    pinMode(pingPin_left, OUTPUT);
    //LOWパルスを送信
    digitalWrite(pingPin_left, LOW);
    delayMicroseconds(2);  
    //HIGHパルスを送信
    digitalWrite(pingPin_left, HIGH);  
    //5uSパルスを送信してPingSensorを起動
    delayMicroseconds(5); 
    digitalWrite(pingPin_left, LOW); 
    
    //入力パルスを読み取るためにデジタルピンをINPUTに変更（シグナルピンを入力に切り替え）
    pinMode(pingPin_left, INPUT);   
    
    //入力パルスの長さを測定
    duration_left = pulseIn(pingPin_left, HIGH);  
  
    //パルスの長さを半分に分割
    duration_left=duration_left/2;  
    //cmに変換
    cm_left = int(duration_left/29); 

    if (canUpdate){
      ultrasonic_left_cm = cm_left;
    }
    delay(5);
    // 5-pin--------------------------------------------------------------------------------
    //ピンをOUTPUTに設定（パルス送信のため）
    pinMode(pingPin_right, OUTPUT);
    //LOWパルスを送信
    digitalWrite(pingPin_right, LOW);
    delayMicroseconds(2);  
    //HIGHパルスを送信
    digitalWrite(pingPin_right, HIGH);  
    //5uSパルスを送信してPingSensorを起動
    delayMicroseconds(5); 
    digitalWrite(pingPin_right, LOW); 
    
    //入力パルスを読み取るためにデジタルピンをINPUTに変更（シグナルピンを入力に切り替え）
    pinMode(pingPin_right, INPUT);   
    
    //入力パルスの長さを測定
    duration_right = pulseIn(pingPin_right, HIGH);  
  
    //パルスの長さを半分に分割
    duration_right=duration_right/2;  
    //cmに変換
    cm_right = int(duration_right/29); 

    if (canUpdate){
      ultrasonic_right_cm = cm_right;
    }
    delay(5);
  }
}
