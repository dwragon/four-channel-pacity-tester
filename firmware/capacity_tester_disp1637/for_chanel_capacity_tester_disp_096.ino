#define NUM_READS 100
#include "TM1637.h"  //библиотека дисплея

const uint8_t loadPins[] = { A0, A1, A2, A3 };  //пиы нагрузки (аналоговый!!!!)

byte buzz_gnd = 2;  //динамик земля
byte buzz_pin = 5;  //динамик сигнал

byte disp_gnd = 6;      //земля дисплея
byte CLK = 7;           //пин дисплея
byte DIO = 8;           //пин дисплея
byte disp_vcc = 10;     //питание дисплея
TM1637 disp(CLK, DIO);  //обозвать дисплей disp

byte butt_pin = 11;  //состояние кнопки
byte butt_gnd = 12;  //земля кнопки


float relayPins[] = { 13, 14, 15, 26 };  //пин реле
const float typVbg = 1.095;              // 1.0 -- 1.2
float Voff = 2.7;                        // напряжение отсечки (для Li-ion=2.5 В, для LiPo=3.0 В)
float R = 5;                             //сопротивление нагрузки
float I[] = { 0, 0, 0, 0 };
float cap[] = { 0, 0, 0, 0 };  //начальная ёмкость
float V[] = { 0, 0, 0, 0 };
float Vcc[] = { 0, 0, 0, 0 };
float Wh[] = { 0, 0, 0, 0 };
unsigned long prevMillis;
unsigned long testStart;

String cap_string;

void setup() {
  for (int i = 0; i < 4; i++) {
    pinMode(relayPins[i], OUTPUT);     //пин реле как выход
    digitalWrite(relayPins[i], LOW);  //выключить реле (оно обратное у меня)
  }
  pinMode(buzz_pin, OUTPUT);  //пищалка
  pinMode(buzz_gnd, OUTPUT);  //пищалка
  digitalWrite(buzz_gnd, 0);

  pinMode(butt_pin, INPUT_PULLUP);  //кнопка подтянута
  pinMode(butt_gnd, OUTPUT);        //земля кнопки
  digitalWrite(butt_gnd, 0);

  pinMode(disp_vcc, OUTPUT);  //питание дисплея
  pinMode(disp_gnd, OUTPUT);  //земля дисплея
  digitalWrite(disp_vcc, 1);
  digitalWrite(disp_gnd, 0);

  disp.init();  //инициализация дисплея
  disp.set(5);  //яркость (0-7)

  Serial.begin(9600);                                    //открыть порт для связи с компом
  Serial.println("Press any key to start the test...");  //Отправьте любой символ для начала теста
  while (Serial.available() == 0) {
    disp.display(3, 0);
    if (digitalRead(butt_pin) == 0) { break; }  //или нажмите кнопку, чтобы начать тест!
  }
  tone(buzz_pin, 200, 500);
  Serial.println("Test is launched...");
  Serial.print("s");
  Serial.print(" ");
  Serial.print("V");
  Serial.print(" ");
  Serial.print("mA");
  Serial.print(" ");
  Serial.print("mAh");
  Serial.print(" ");
  Serial.print("Wh");
  Serial.print(" ");
  Serial.println("Vcc");
  for (int i = 0; i < 4; i++) {
    digitalWrite(relayPins[i], HIGH);  //Переключить реле (замкнуть акум на нагрузку)
  }
  testStart = millis();   //время начала теста в системе отсчёта ардуины
  prevMillis = millis();  //время первого шага
}

void loop() {
  for (int i = 0; i < 4; i++) {
    Vcc[i] = readVcc();                                    //хитрое считывание опорного напряжения (функция readVcc() находится ниже
    V[i] = (readAnalog(loadPins[i]) * Vcc[i]) / 1023.000;  //считывание напряжения АКБ
    I[i] = V[i] / R;                                            //расчет тока по закону Ома, в Амперах
    cap[i] += I[i] * (millis() - prevMillis) / 3600000 * 1000;  //расчет емкости АКБ в мАч
    Wh[i] += I[i] * V[i] * (millis() - prevMillis) / 3600000;   //расчет емкости АКБ в ВтЧ
    prevMillis = millis();
    sendData();                          // отправка данных
    if (V[i] < Voff) {                   //выключение нагрузки при достижении минимального напряжения
      digitalWrite(relayPins[i], LOW);  //разорвать цепь (отключить акб от нагрузки)
    }

    if (V[0] < Voff && V[1] < Voff && V[2] < Voff && V[3] < Voff) {
      Serial.println("Test is done");  //тест закончен

      for (int i = 0; i < 5; i++) {  //выполнить 5 раз
        tone(buzz_pin, 200, 500);    //пищать на 3 пин частотой 100 герц 500 миллисекунд
                                     //  disp_print(cap_string);
        delay(1000);
        // disp.clearDisplay();
        delay(500);
      }

      while (2 > 1) {  //бесконечный цикл, чтобы loop() не перезапустился + моргаем результатом!
                       //  disp_print(cap_string);
        delay(1000);
        //  disp.clearDisplay();
        delay(500);
      }
    }
  }
}

void sendData() {
  for (int i = 0; i < 4; i++) {  //функция, которая посылает данные в порт
    Serial.print((millis() - testStart) / 1000);
    Serial.print(" ");
    Serial.print(V[i], 3);
    Serial.print(" ");
    Serial.print(I[i], 1);
    Serial.print(" ");
    Serial.print(cap[i], 0);
    Serial.print(" ");
    Serial.print(Wh[i], 2);
    Serial.print(" ");
    Serial.println(Vcc[i], 3);
    cap_string = String(round(cap[i]));
    disp_print(cap_string);
  }
}

//----------Функция точного определения опорного напряжения для измерения напряжения на акуме-------
float readAnalog(int pin) {
  // read multiple values and sort them to take the mode
  int sortedValues[NUM_READS];
  for (int i = 0; i < NUM_READS; i++) {
    delay(25);
    int value = analogRead(pin);
    int j;
    if (value < sortedValues[0] || i == 0) {
      j = 0;  //insert at first position
    } else {
      for (j = 1; j < i; j++) {
        if (sortedValues[j - 1] <= value && sortedValues[j] >= value) {
          // j is insert position
          break;
        }
      }
    }
    for (int k = i; k > j; k--) {
      // move all values higher than current reading up one position
      sortedValues[k] = sortedValues[k - 1];
    }
    sortedValues[j] = value;  //insert current reading
  }
  //return scaled mode of 10 values
  float returnval = 0;
  for (int i = NUM_READS / 2 - 5; i < (NUM_READS / 2 + 5); i++) {
    returnval += sortedValues[i];
  }
  return returnval / 10;
}
//----------Функция точного определения опорного напряжения для измерения напряжения на акуме КОНЕЦ-------


//----------фильтр данных (для уменьшения шумов и разброса данных)-------
float readVcc() {
  // read multiple values and sort them to take the mode
  float sortedValues[NUM_READS];
  for (int i = 0; i < NUM_READS; i++) {
    float tmp = 0.0;
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
    ADCSRA |= _BV(ADSC);  // Start conversion
    delay(25);
    while (bit_is_set(ADCSRA, ADSC))
      ;                   // measuring
    uint8_t low = ADCL;   // must read ADCL first - it then locks ADCH
    uint8_t high = ADCH;  // unlocks both
    tmp = (high << 8) | low;
    float value = (typVbg * 1023.0) / tmp;
    int j;
    if (value < sortedValues[0] || i == 0) {
      j = 0;  //insert at first position
    } else {
      for (j = 1; j < i; j++) {
        if (sortedValues[j - 1] <= value && sortedValues[j] >= value) {
          // j is insert position
          break;
        }
      }
    }
    for (int k = i; k > j; k--) {
      // move all values higher than current reading up one position
      sortedValues[k] = sortedValues[k - 1];
    }
    sortedValues[j] = value;  //insert current reading
  }
  //return scaled mode of 10 values
  float returnval = 0;
  for (int i = NUM_READS / 2 - 5; i < (NUM_READS / 2 + 5); i++) {
    returnval += sortedValues[i];
  }
  return returnval / 10;
}
//----------фильтр данных (для уменьшения шумов и разброса данных) КОНЕЦ-------

void disp_print(String x) {
  disp.point(POINT_OFF);
  switch (x.length()) {  //кароч тут измеряется длина строки и соотвествено выводится всё на дисплей
    case 1:
      disp.display(0, 18);
      disp.display(1, 18);
      disp.display(2, 18);
      disp.display(3, x[0] - '0');
      break;
    case 2:
      disp.display(0, 18);
      disp.display(1, 18);
      disp.display(2, x[0] - '0');
      disp.display(3, x[1] - '0');
      break;
    case 3:
      disp.display(0, 18);
      disp.display(1, x[0] - '0');
      disp.display(2, x[1] - '0');
      disp.display(3, x[2] - '0');
      break;
  }
}
