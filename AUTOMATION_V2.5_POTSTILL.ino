#include <LiquidCrystal_I2C.h>   //БИБЛИОТЕКА LCD ДИСПЛЕЯ
#include <GyverTimers.h>         //ТАЙМЕРЫ
#include <microDS18B20.h>        //ТЕМПЕРАТУРА DS18B20
//ПИНЫ
#define ZERO_PIN 2     // ПИН ДЕТЕКТОРА НУЛЯ
#define INT_NUM 0      // НОМЕР ПРЕРЫВАНИЯ ДЕТЕКТОРА НУЛЯ
#define DIMMER_PIN 3   // УПРАВЛЕНИЕ СИМИСТОРА
#define R_HEAT A6      // МОЩНОСТЬ НАГРЕВА
#define R_COOL A7      // СКОРОСТЬ КУЛЕРОВ
#define BTN_PIN 7      // КНОПКА
#define PWM_PIN 6      // ШИМ КУЛЕРОВ
#define PUMP_PIN 10    // MOSFET ПОМПА
#define TC_PIN 4       // ТЕМПЕРАТУРА КУБ
#define TW_PIN 5       // ТЕМПЕРАТУРА ВОДА
#define TUO_PIN 9      // ТЕМПЕРАТУРА УЗЕЛ ОТБОРА
//КОНСТАНТЫ РЕЖИМОВ
#define STOP_M1 98     // ТЕМПЕРАТУРА ОСТАНОВКИ РЕЖИМА 1
#define STOP_M2 96     // ТЕМПЕРАТУРА ОСТАНОВКИ РЕЖИМА 1
#define FAIL_STOP_C 99 // ТЕМПЕРАТУРА АВАРИЙНОЙ ОСТАНОВКИ(КУБ)
#define FAIL_STOP_W 45 // ТЕМПЕРАТУРА АВАРИЙНОЙ ОСТАНОВКИ(ВОДА)
#define P_START_C  70  // ТЕМПЕРАТУРА ВКЛ ПОМПЫ ПО КУБУ

//i2C ДИСПЛЕЯ ПОДКЛЮЧЕН НА A4(SDA), A5(SCL). ДИСПЛЕЙ 20x4
LiquidCrystal_I2C lcd(0x27, 20, 4);
//ИНИЦИАЛИЗАЦИЯ ДАТЧИКОВ ТЕМПЕРАТУРЫ. MicroDS18B20<PIN> <object_name>;
MicroDS18B20<TC_PIN> sensor_cube;
MicroDS18B20<TW_PIN> sensor_water;
MicroDS18B20<TUO_PIN> sensor_out;

//ОПРЕДЕЛЯЕМ ПЕРЕМЕННЫЕ
float cube_temp;        // ТЕМП. В КУБЕ (идет на дисплей)
float water_temp;       // ТЕМП. ВОДЫ В ОХЛАЖДЕНИИ (идет на дисплей)
float uo_temp;          // ТЕМП. УЗЛА ОТБОРА (идет на дисплей) 
int mode = 3;           // ГЛАВНЫЙ РЕЖИМ РАБОТЫ. ПО УМОЛЧАНИЮ 3(РУЧНОЙ)
int pwm_pow;            // ШИМ КУЛЕРОВ (для дисплея)
int pwm_pow_map;        // ШИМ КУЛЕРОВ ДЛЯ РАСЧЕТОВ
int ten_pow;            // МОЩНОСТЬ ТЭН (для дисплея)
int ten_pow_map;        // МОЩНОСТЬ ТЭН ДЛЯ РАСЧЕТОВ
int dimmer;             // ДИММЕР. ХРАНИТ ВРЕМЯ ВКЛ СИМИСТОРА В мкс ОТ ПРОХОЖДЕНИЯ НУЛЯ СИНУСОИДОЙ
int rheat;              // ЧТЕНИЕ ЗАДАЮЩЕГО R НАГРЕВА
bool star;              //ФЛАГ ИНДИКАТОРА РАБОТЫ
String pump_state;      // ON/OFF ДЛЯ ДИСПЛЕЯ
String mode_desc;       // ОПИСАНИЕ РЕЖИМА ДЛЯ ДИСПЛЕЯ

//НАЧАЛЬНЫЕ УСТАНОВКИ
void setup() {
analogWrite(6, 0);      //КУЛЕРЫ НА НОЛЬ
lcd.init();             //INIT ДИСПЛЕЯ
lcd.backlight();        //ПОДСВЕТКА ВКЛ
// ЗАСТАВКА ПРИ СТАРТЕ
lcd.blink();
char line1[] = "SAMOGON";
char line2[] = "AUTOMATION V 2.1";
char line3[] = "....................";
lcd.setCursor(0, 0);
  for (int i = 0; i < strlen(line1); i++) {
    lcd.print(line1[i]);
    delay(40);
  }
lcd.setCursor(0, 1);
  for (int i = 0; i < strlen(line2); i++) {
    lcd.print(line2[i]);
    delay(40);
  }
lcd.noBlink();
lcd.setCursor(0, 3);
  for (int i = 0; i < strlen(line3); i++) {
    lcd.print(line3[i]);
    delay(50);
  }
// КОНЕЦ ОТРИСОВКИ
//ОЧИЩАЕМ ДИСПЛЕЙ ПЕРЕД СЛЕДУЮЩИМ ЭКРАНОМ
lcd.clear();
///ДЛЯ РЕГУЛИРОВКИ ТЭНА ЧЕРЕЗ GyverTimers И СИМИСТОР 
pinMode(ZERO_PIN, INPUT_PULLUP); 
pinMode(DIMMER_PIN, OUTPUT); 
attachInterrupt(INT_NUM, isr, FALLING);  // прерывание. для самодельной схемы - FALLING (по падению на детекторе)
Timer2.enableISR(); //включили таймер и повесили на него действие
//ЗАДАЕМ PIN MODE
pinMode(R_HEAT, INPUT);
pinMode(R_COOL, INPUT);
pinMode(BTN_PIN, INPUT_PULLUP);
pinMode(PWM_PIN, OUTPUT);
pinMode(PUMP_PIN, OUTPUT);
// КОНЕЦ setup()
}

void loop() {
//###############################
//ПОЛУЧЕНИЕ ТЕМПЕРАТУР С ДАТЧИКОВ
static uint32_t tmr_temp;
if (millis() - tmr_temp >= 1000) {
  tmr_temp = millis();
if (sensor_cube.readTemp()) {
    cube_temp = sensor_cube.getTemp();
    sensor_cube.requestTemp();
}
else {
  sensor_cube.requestTemp();
}
  if (sensor_water.readTemp()) {
    water_temp = sensor_water.getTemp();
    sensor_water.requestTemp();
  }
  else {
    sensor_water.requestTemp();
  }
  if (sensor_out.readTemp()) {
    uo_temp = sensor_out.getTemp();
    sensor_out.requestTemp();
  }
  else {
    sensor_out.requestTemp();
  }
// КОНЕЦ ОПРОСА ДАТЧИКОВ
}

//########################################################
//ВЫБОР РЕЖИМА ПО НАЖАТИЮ КНОПКИ. ОПРОС КНОПКИ РАЗ В 0.9 СЕК
static uint32_t tmr_mode;
if (millis() - tmr_mode >= 550) {
    tmr_mode = millis();  
if (!digitalRead(BTN_PIN)) {
mode = mode + 1;
if (mode > 3) {        //ТУТ ЗАДАЕМ МАКСИМАЛЬНОЕ ЧИСЛО РЕЖИМОВ
  mode = 1;
  }
 }
// ЗАДАЕМ ОПИСАНИЕ РЕЖИМОВ ДЛЯ ОТОБРАЖЕНИЯ НА ДИСПЛЕЕ
if (mode == 1) mode_desc = "POTSTILL1";
if (mode == 2) mode_desc = "POTSTILL2";
if (mode == 3) mode_desc = "MANUAL   ";
// КОНЕЦ ОПРОСА КНОПКИ И ВЫБОРА РЕЖИМА
}

//#############################################
// ЗАДАНИЕ МОЩНОСТИ ТЭНа ПО РЕЖИМАМ
// РЕЖИМ "POTSTILL" ПЕРВОЙ ПЕРЕГОНКИ (АВТО)
static uint32_t tmr_pstill;
if (millis() - tmr_pstill >= 250) {
    tmr_pstill = millis();
if (mode == 1 && cube_temp < 75 ) {
  detachInterrupt(INT_NUM);
  ten_pow = 100;
  digitalWrite(DIMMER_PIN, 1);
 }
if (mode == 1 && cube_temp >= 75) {
    attachInterrupt(INT_NUM, isr, FALLING);
    dimmer = map(cube_temp, 75, 98, 4000, 2500); // МЕНЯЕТСЯ ОТ 60% до 75%
    ten_pow = map(dimmer, 9000, 500, 0, 100);
 }
// РЕЖИМ "POTSTILL 2" ВТОРОЙ ПЕРЕГОНКИ (МОЩНОСТЬ НИЖЕ)
if (mode == 2 && cube_temp < 75 ) {
  detachInterrupt(INT_NUM);
  ten_pow = 100;
  digitalWrite(DIMMER_PIN, 1);
 }
if (mode == 2 && cube_temp >= 75) {
    attachInterrupt(INT_NUM, isr, FALLING);
    dimmer = map(cube_temp, 75, 96, 5000, 3500); // МЕНЯЕТСЯ ОТ 50% ДО 65%
    ten_pow = map(dimmer, 9000, 500, 0, 100);
 }
// РУЧНОЙ
if (mode == 3){
  rheat = analogRead(R_HEAT);
if (rheat < 1000 && rheat >= 100) {
  attachInterrupt(INT_NUM, isr, FALLING);
  dimmer = map(rheat, 0, 1024, 9000, 500);
  ten_pow = map(rheat, 0, 1024, 0, 100);
 }
if (rheat >= 1000) {
  detachInterrupt(INT_NUM);
  ten_pow = 100;
  digitalWrite(DIMMER_PIN, 1);
 }
if (rheat < 100) {
  detachInterrupt(INT_NUM);
  ten_pow = 0;
  digitalWrite(DIMMER_PIN, 0);
 }
}
// КОНЕЦ УПРАВЛЕНИЯ МОЩНОСТЬЮ ТЭНа
} 

//#################################################
// УПРАВЛЕНИЕ ПОМПОЙ (АСИНХРОННО)
static uint32_t tmr_pump;
if (millis() - tmr_pump >= 452) {
    tmr_pump = millis();
if (cube_temp >= P_START_C) {
  digitalWrite(PUMP_PIN, 1);
  pump_state = " ON"; 
 }
else {
  digitalWrite(PUMP_PIN, 0);
  pump_state = "OFF";
 }
// КОНЕЦ УПРАВЛЕНИЯ ПОМПОЙ
}

//##################################################
// УПРАВЛЕНИЕ КУЛЕРАМИ (АСИНХРОННОЕ) 
static uint32_t tmr_air;
if (millis() - tmr_air >= 321) {
    tmr_air = millis();
if (mode != 3 && water_temp >= 10) {
  pwm_pow_map = map(water_temp, 10, 45, 0, 255);
  analogWrite(PWM_PIN, pwm_pow_map);
  pwm_pow = map(pwm_pow_map, 0, 255, 0, 100);  
 }
if (mode !=3 && water_temp < 10) {
  pwm_pow = 0;
 }
if (mode == 3) {
  pwm_pow_map = map(analogRead(R_COOL), 0, 1024, 0, 255);
  analogWrite(PWM_PIN, pwm_pow_map);
  pwm_pow = map(pwm_pow_map, 0, 255, 0, 100);
 }
// КОНЕЦ УПРАВЛЕНИЯ КУЛЕРАМИ
}

//######################################
// ОСТАНОВКИ (СИНХРОННО ДЛЯ УМЕНЬШЕНИЯ ВРЕМЕНИ РЕАКЦИИ)
// POTSTILL_1
if (mode == 1 && cube_temp >= STOP_M1) {
  stop_norm();
}
// POTSTILL_2
if (mode == 2 && cube_temp >= STOP_M2) {
  stop_norm();
}
// АВАРИЙНАЯ ПО КУБУ
if (cube_temp >= FAIL_STOP_C) {
  stop_cube();
}
// АВАРИЙНАЯ ПО ВОДЕ
if (water_temp >= FAIL_STOP_W) {
  stop_water();
}

// ##################################################
// ВЫВОД НА ДИСПЛЕЙ ИНФОРМАЦИИ ПО ТАЙМЕРУ РАЗ В 500мс
static uint32_t tmr;
if (millis() - tmr >= 500) {
    tmr = millis();
// вывод температуры куба
 lcd.setCursor(0,0);
 lcd.print("CUBE:");
 lcd.setCursor(5,0);
 lcd.print(cube_temp);
 lcd.setCursor(10,0);
 lcd.write(223); // значек градуса из таблицы дисплея
 // вывод мощности нагрева в % от максимальной
 lcd.setCursor(12,0);
 lcd.print("TEN:");
 lcd.setCursor(16,0);
 lcd.print("   ");
 lcd.setCursor(16,0);
 lcd.print(ten_pow);
 lcd.setCursor(19,0);
 lcd.print("%");
// вывод температуры воды в контуре охлаждения
 lcd.setCursor(0,1);
 lcd.print("COOL:");
 lcd.setCursor(5,1);
 lcd.print(water_temp);
 lcd.setCursor(10,1);
 lcd.write(223);
// вывод мощности PWM кулеров охлаждения воды
 lcd.setCursor(12,1);
 lcd.print("AIR:");
 lcd.setCursor(16,1);
 lcd.print("   ");
 lcd.setCursor(16,1);
 lcd.print(pwm_pow);
 lcd.setCursor(19,1);
 lcd.print("%");
// вывод температуры узла отбора
 lcd.setCursor(0,2);
 lcd.print("UOUT:");
 lcd.setCursor(5,2);
 lcd.print(uo_temp);
 lcd.setCursor(10,2);
 lcd.write(223);
// вывод статуса помпы
 lcd.setCursor(12,2);
 lcd.print("PUMP:");
 lcd.setCursor(17,2);
 lcd.print(pump_state);
 // вывод режима работы
 lcd.setCursor(0,3);
 lcd.print("MODE:");
 lcd.setCursor(6,3);
 lcd.print(mode_desc); 
// МИГАЕМ ЗВЕЗДОЧКОЙ В УГЛУ. ИНДИКАТОР РАБОТЫ КОНТРОЛЛЕРА
if (star == 1) {
  lcd.setCursor(19,3);
  lcd.print("*");
  star = 0;
}
else {
   lcd.setCursor(19,3);
   lcd.print(" ");
   star = 1; 
}
// КОНЕЦ ВЫВОДА НА ДИСПЛЕЙ ПО ТАЙМЕРУ
 }
//#################
// КОНЕЦ ЦИКЛА LOOP
}

// ДОПОЛНИТЕЛЬНЫЕ ФУНКЦИИ
// ОБРАБОТЧИК ПРЕРЫВАНИЯ ДЕТЕКТОРУ НУЛЯ РЕГУЛЯТОРА ТЭН
void isr() {
  static int lastDim;
  if (lastDim != dimmer) Timer2.setPeriod(lastDim = dimmer);
  else Timer2.restart();
}
// ОБРАБОТЧИК ТАЙМЕРА РЕГУЛЯТОРА ТЭН
ISR(TIMER2_A) {
  digitalWrite(DIMMER_PIN, 1);  // включаем симистор на оставшейся длине полуволны после отсчета таймером
  Timer2.stop();                // останавливаем таймер до следующего прерывания где он будет перезапущен
  digitalWrite(DIMMER_PIN, 0);  // выставляем 0 на оптопаре сразу, но симистор остается открытым до прохождения нуля в напряжении сети.
}
//НОРМАЛЬНЫЙ СТОП
void stop_norm() {
lcd.clear();
while (true) {
    lcd.setCursor(0,1);
    lcd.print("NORMAL STOP");
    detachInterrupt(INT_NUM);    // отцепляем прерывание
    digitalWrite(DIMMER_PIN, 0); // выключаем тиристор принудительно
    digitalWrite(PUMP_PIN, 0);   // выключаем помпу
    analogWrite(PWM_PIN, 0);     // выставляем минимум на кулеры
    delay(1000);                 // повторяем бесконечно, раз в секунду
  }
}
//АВАРИЙНЫЙ СТОП ПО ВОДЕ
void stop_water() {
lcd.clear();
while (true) {
    lcd.setCursor(0,1);
    lcd.print("ERR:WATER OVRHEAT");
    detachInterrupt(INT_NUM);
    digitalWrite(DIMMER_PIN, 0);  //отцепляем прерывание
    digitalWrite(PUMP_PIN, 0);    //выключаем помпу
    analogWrite(PWM_PIN, 0);      //выставляем минимум на кулеры
    delay(1000);                  //повторяем бесконечно, раз в секунду
  }
}
//АВАРИЙНЫЙ СТОП ПО КУБУ
void stop_cube() {
lcd.clear();
while (true) {
    lcd.setCursor(0,1);
    lcd.print("ERR:CUBE OVRHEAT");
    detachInterrupt(INT_NUM);
    digitalWrite(DIMMER_PIN, 0);  //отцепляем прерывание
    digitalWrite(PUMP_PIN, 0);    //выключаем помпу
    analogWrite(PWM_PIN, 0);      //выставляем минимум на кулеры
    delay(1000);                  //повторяем бесконечно, раз в секунду
  }
}
// THE END
