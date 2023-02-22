#include <LiquidCrystal_I2C.h> //библиотека дисплея
#include <GyverTimers.h>  // библиотека таймера
#include <microDS18B20.h> //библиотека для датчиков температуры

//ЗАДАЕМ ПИНЫ:
//ТИРИСТОРНЫЙ РЕГУЛЯТОР
#define ZERO_PIN 2    // пин детектора нуля
#define INT_NUM 0     // соответствующий ему номер прерывания
#define DIMMER_PIN 3  // управляющий пин симистора
//ПОТЕНЦИОМЕРТЫ УПРАВЛЕНИЯ НАГРЕВОМ И КУЛЕРАМИ
#define R_HEAT A6
#define R_COOL A7
//КНОПКИ СМЕНЫ РЕЖИМА РАБОТЫ
#define BTN_PIN 7 //переключение режимов
//PWM НА КУЛЕРЫ
#define PWM_PIN 6
//MOSFET Помпы
#define PUMP_PIN 10
#define COOL_PIN 12
//Пины датчиков температуры
#define TC_PIN 4 //куб
#define TW_PIN 5 //вода
#define TUO_PIN 9 //узел отбора
//КОНСТАНТЫ НАСТРОЙКИ
//температуры остановок
#define NSTOP_TEMP1 98 //на первом режиме potstill
#define NSTOP_TEMP2 95 //на втором режиме rectific
#define ASTOP_TEMP 98.5 //аварийный стоп по перегреву в кубе
#define WSTOP_TEMP 45 //аварийный стоп по воде
//Срабатывание помпы
#define PUMP_TEMP 70
//Таймеры вывода/ввода
#define LCDT 500 //дисплей
#define TEN_POW 200 //Задание мощности ТЭН


//i2C ДИСПЛЕЯ ПОДКЛЮЧЕН НА A4(SDA), A5(SCL)
//ДИСПЛЕЙ 20х4
LiquidCrystal_I2C lcd(0x27, 20, 4);
//ИНИЦИАЛИЗАЦИЯ ДАТЧИКОВ ТЕМПЕРАТУРЫ. MicroDS18B20<PIN> <object_name>;
MicroDS18B20<TC_PIN> sensor1;
MicroDS18B20<TW_PIN> sensor2;
MicroDS18B20<TUO_PIN> sensor3;

//ОПРЕДЕЛЯЕМ ПЕРЕМЕННЫЕ
float cube_temp;     //температура в кубе (идет так же на дисплей)
float water_temp;    //температура воды (идет так же на дисплей)
float uo_temp;       //температура в узле отбора
String pump_state;   //состояние ON/OFF помпы, для отображения на дисплее
int mode = 3;        //режим работы, 1 - первый перегон, 2 - второй перегон, 3 - ручной режим, default = 3
int pwm_pow;         //ШИМ мощность на кулерах (для дисплея)
int pwm_pow_map;     //ШИМ мощность для рассчетов
int ten_pow_map;     //Мощность ТЭН для рассчетов
int ten_pow;         //Мощность ТЭН (для дисплея)
int dimmer;          //переменная диммера, задание мощности ТЭН непосредственно для таймера
                     // задаём значение 500-9300, где 500 максимум мощности, 9300 минимум! (50Гц сеть)
                     // 500-7600 для 60 Гц в сети.
bool star;           //триггер для индикатора работы контроллера
int read_request = 0;

///НАЧАЛЬНЫЕ УСТАНОВКИ

void setup() {
analogWrite(6, 0); //делаем кулеры тише
//ИНИЦИАЛИЗАЦИЯ ДИСПЛЕЯ
lcd.init(); //первичная инициализация
lcd.backlight(); //Подсветка
//РИСУЕМ ЗАСТАВКУ
lcd.blink();
char line1[] = "SAMOGON";
char line2[] = "AUTOMATION V 2.0";
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
//ОЧИЩАЕМ ДИСПЛЕЙ ПЕРЕД СЛЕДУЮЩИМ ЭКРАНОМ
lcd.clear();
///ДЛЯ РЕГУЛИРОВКИ ТЭНА через GyverTimers и симисторный диммер 
pinMode(ZERO_PIN, INPUT_PULLUP);
pinMode(DIMMER_PIN, OUTPUT);
attachInterrupt(INT_NUM, isr, FALLING);  // для самодельной схемы ставь FALLING
Timer2.enableISR();
//задаем PIN MODE там где требуется
pinMode(R_HEAT, INPUT);
pinMode(R_COOL, INPUT);
pinMode(BTN_PIN, INPUT_PULLUP); //подтянут резистором в чипе к VCC

pinMode(PWM_PIN, OUTPUT);
pinMode(PUMP_PIN, OUTPUT);
pinMode(COOL_PIN, OUTPUT);
}

void loop() {

//РАБОТА С ДАТЧИКАМИ ТЕМПЕРАТУРЫ
static uint32_t tmr_temp;
if (millis() - tmr_temp >= 1000) {
  tmr_temp = millis();
if (sensor1.readTemp()) {
    cube_temp = sensor1.getTemp();
    sensor1.requestTemp();
}
else {
  sensor1.requestTemp();
}
  if (sensor2.readTemp()) {
    water_temp = sensor2.getTemp();
    sensor2.requestTemp();
  }
  else {
    sensor2.requestTemp();
  }
  if (sensor3.readTemp()) {
    uo_temp = sensor3.getTemp();
    sensor3.requestTemp();
  }
  else {
    sensor3.requestTemp();
  }
}

//##########################
//ОСНОВНОЙ АЛГОРИТМ КОНТРОЛЯ

//ВЫБОР РЕЖИМА на флаге "mode"
static uint32_t tmr_mode;
if (millis() - tmr_mode >= 1000) {
    tmr_mode = millis();  
if (!digitalRead(BTN_PIN)) {
mode = mode + 1;
if (mode > 3) {
  mode = 1;
  }
 }
}

//полный диапазон значений для dimmer - max=500мкс, min=9300мкс
//считается от точки перехода напряжения через 0 до включения симистора.
//время одной полуволны на 50Гц = 10тыс мкс, Но нужно время на обработку функций и прерываний
//для стабильности возьмем диапазон 9000 - 500, так как были замечены артефакты при минимальной мощности, да и в целом нам тут 100Вт на ТЭН не нужно совсем. 

//ЗАДАНИЕ МОЩНОСТИ НАГРЕВА ТЭНА
//делаем в асинхроне чтобы не дергать attach/detach прерывание слишком часто

static uint32_t tmr_heat;
if (millis() - tmr_heat >= TEN_POW) {
    tmr_heat = millis();  

//РУЧНОЙ РЕЖИМ "MANUAL"
if (mode == 3 && analogRead(R_HEAT) < 1000 && analogRead(R_HEAT) >= 100) {
  attachInterrupt(INT_NUM, isr, FALLING);
  dimmer = map(analogRead(R_HEAT), 0, 1024, 9000, 500);
  ten_pow = map(dimmer, 9000, 500, 0, 100);
 }
if (mode == 3 && analogRead(R_HEAT) >= 1000) {
  detachInterrupt(INT_NUM);
  ten_pow = 100;
  digitalWrite(DIMMER_PIN, 1);
 }
if (mode == 3 && analogRead(R_HEAT) < 100) {
  detachInterrupt(INT_NUM);
  ten_pow = 0;
  digitalWrite(DIMMER_PIN, 0);
 }

//АВТОМАТИЧЕСКИЙ "POTSTILL"
if (mode == 1 && cube_temp >= 75) {
    attachInterrupt(INT_NUM, isr, FALLING);
    dimmer = map(cube_temp, 80, 98, 4000, 2600);
    ten_pow = map(dimmer, 9000, 500, 0, 100);
 }
if (mode == 1 && cube_temp < 75) {
  detachInterrupt(INT_NUM);
  ten_pow = 100;
  digitalWrite(DIMMER_PIN, 1);
}

//ПОЛУАВТОМАТИЧЕСКИЙ "RECTIFIC"
if (mode == 2 && analogRead(R_HEAT) < 1000 && analogRead(R_HEAT) >= 100) {
  attachInterrupt(INT_NUM, isr, FALLING);
  dimmer = map(analogRead(R_HEAT), 0, 1024, 9000, 500);
  ten_pow = map(analogRead(R_HEAT), 0, 1024, 0, 100);
 }
if (mode == 2 && analogRead(R_HEAT) >= 1000) {
  detachInterrupt(INT_NUM);
  ten_pow = 100;
  digitalWrite(DIMMER_PIN, 1);
 }
if (mode == 2 && analogRead(R_HEAT) < 100) {
  detachInterrupt(INT_NUM);
  ten_pow = 0;
  digitalWrite(DIMMER_PIN, 0);
 }
}

//ЗАДАНИЕ МОЩНОСТИ PWM КУЛЕРОВ по температуре воды. 
//Плавное саморегулирование на режимах "POTSTILL" и "RECTIFIC".
if (mode != 3 && water_temp > 1) {
  pwm_pow_map = map(water_temp, 10, 45, 0, 255);
  analogWrite(PWM_PIN, pwm_pow_map);
  pwm_pow = map(pwm_pow_map, 0, 255, 0, 100);  
}
else {
  pwm_pow = 0;  
  }
//Ручное регулирование на режиме 3
if (mode == 3) {
  pwm_pow_map = map(analogRead(R_COOL), 0, 1024, 0, 255);
  analogWrite(PWM_PIN, pwm_pow_map);
  pwm_pow = map(pwm_pow_map, 0, 255, 0, 100);
}

//УПРАВЛЕНИЕ ПОМПОЙ, включение на 70 градусах в кубе
//для ручного включения предусмотрен выключатель подающий 12V аппаратно на помпу.
//для режима 2 можно пересмотреть триггер помпы. она по факту нужна когда температура в узле отбора начнет рости выше 50 
if (cube_temp >= PUMP_TEMP) {
  digitalWrite(PUMP_PIN, 1);
  pump_state = " ON"; 
 }
else {
  digitalWrite(PUMP_PIN, 0);
  pump_state = "OFF";
}

//НОРМАЛЬЫНЕ СТОПЫ ПО РЕЖИМАМ
//На первом режиме примерно на 98 градусах, соответствует отбору до 20% в струе.
//можно выгнать в 0 переключив на manual. 
//Зависимость +/- линейная и получена экспериментальным путем перегонов. 
if (mode == 1 && cube_temp >= NSTOP_TEMP1) {
  stop_norm();
}
//На втором режиме на 95 градусах. Примерно соответствует началу хвостов в колонне.
if (mode == 2 && cube_temp >= NSTOP_TEMP2) {
  stop_norm();
}

//АВАРИЙНЫЕ ОСТАНОВКИ
//по температуре куба 99, нам точно не нужно столько. Не спасет от пробития симистора конечно. 
//На этот случай нужно ставить реле между нагрузкой и симистором на 20-30А не меньше. 

if (cube_temp >= ASTOP_TEMP) { 
  stop_cube();
}
//по температуре охлаждающей жидкости. На 45 это уже сильно мешает процессу
if (water_temp >= WSTOP_TEMP) { 
  stop_water();  
}

///// ВЫВОД НА ДИСПЛЕЙ ИНФОРМАЦИИ ПО ТАЙМЕРУ раз в 500мс, экспериментально выбрано, как наиболее приятное глазу с минимальным лагом 
static uint32_t tmr;
if (millis() - tmr >= LCDT) {
    tmr = millis();

//вывод температуры куба
 lcd.setCursor(0,0);
 lcd.print("CUBE:");
 lcd.setCursor(5,0);
 lcd.print(cube_temp);
 lcd.setCursor(10,0);
 lcd.write(223); //значек градуса из таблицы дисплея
 
 //вывод мощности нагрева в % от максимальной
 lcd.setCursor(12,0);
 lcd.print("TEN:");
 lcd.setCursor(16,0);
 lcd.print("   ");
 lcd.setCursor(16,0);
 lcd.print(ten_pow);
 lcd.setCursor(19,0);
 lcd.print("%");

//вывод температуры воды в контуре охлаждения
 lcd.setCursor(0,1);
 lcd.print("COOL:");
 lcd.setCursor(5,1);
 lcd.print(water_temp);
 lcd.setCursor(10,1);
 lcd.write(223);

//вывод мощности PWM кулеров охлаждения воды
 lcd.setCursor(12,1);
 lcd.print("AIR:");
 lcd.setCursor(16,1);
 lcd.print("   ");
 lcd.setCursor(16,1);
 lcd.print(pwm_pow);
 lcd.setCursor(19,1);
 lcd.print("%");

//вывод температуры узла отбора
 lcd.setCursor(0,2);
 lcd.print(" OUT:");
 lcd.setCursor(5,2);
 lcd.print(uo_temp);
 lcd.setCursor(10,2);
 lcd.write(223);

// вывод статуса помпы
 lcd.setCursor(12,2);
 lcd.print("PUMP:");
 lcd.setCursor(17,2);
 lcd.print(pump_state);

//вывод информации о режиме работы
lcd.setCursor(0,3);
lcd.print("MODE:");
lcd.setCursor(6,3);
if (mode == 3) lcd.print("MANUAL  "); //ручной всё контролируется руками, есть только вкл/откл помпы и автостоп на 99 в кубе
if (mode == 2) lcd.print("RECTIFIC"); //ректификация, охлаждение автоматически, мощность вручную(для вывода на оптимальный режим). 
if (mode == 1) lcd.print("POTSTILL"); //первый перегон браги, охлаждение и мощность автоматически. 

//МИГАЕМ ЗВЕЗДОЧКОЙ В УГЛУ. ИНДИКАТОР РАБОТЫ КОНТРОЛЛЕРА
//нужен на случай если вдруг зависнет контроллер, хотябы на глаз будет понятно
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

//КОНЕЦ ВЫВОДА НА ДИСПЛЕЙ ПО ТАЙМЕРУ
 }

//КОНЕЦ ЦИКЛА loop
}

// Симисторный регулятор. Прерывание детектора нуля
//нужно для корректировки периода и перезапуска таймера
void isr() {
  static int lastDim;
  if (lastDim != dimmer) Timer2.setPeriod(lastDim = dimmer);
  else Timer2.restart();
}
// Симисторный регулятор. Срабатывание таймера после перехода напряжения через 0(таймер запускается в прерывании), считает N микросекунд с прерывания и вызывает функцию ISR().
ISR(TIMER2_A) {
  digitalWrite(DIMMER_PIN, 1);  // включаем симистор на оставшейся после  отсчета таймером полуволне
  Timer2.stop();                // останавливаем таймер до следующего прерывания где он будет перезапущен
  digitalWrite(DIMMER_PIN, 0);  // выставляем 0 на оптопаре сразу, но симистор остается открытым до прохождения нуля в напряжении сети. Особенность работы триака. 
}

///аварийный стоп по кубу
void stop_cube() {
lcd.clear();
while (true) {
    lcd.setCursor(0,1);
    lcd.print("ERR:CUBE OVERHEAT");
    detachInterrupt(INT_NUM);
    digitalWrite(DIMMER_PIN, 0); //выключаем тиристор принудительно
    digitalWrite(PUMP_PIN, 0); //выключаем помпу
    digitalWrite(COOL_PIN, 0); //выключаем кулеры
    analogWrite(PWM_PIN, 0); //выставляем минимум на кулеры
    delay(1000); //повторяем бесконечно, раз в секунду
  }
}

///ЦИКЛ ЛОВУШКА ДЛЯ ОСТАНОВКИ ПО ПЕРЕГРЕВУ ВОДЫ
void stop_water() {
lcd.clear();
while (true) {
    lcd.setCursor(0,1);
    lcd.print("ERR:WATER OVERHEAT");
    detachInterrupt(INT_NUM);
    digitalWrite(DIMMER_PIN, 0); //выключаем тиристор принудительно
    digitalWrite(PUMP_PIN, 0); //выключаем помпу
    analogWrite(PWM_PIN, 0); //выставляем минимум на кулеры
    delay(1000); //повторяем бесконечно, раз в секунду
  }
}

///ЦИКЛ ЛОВУШКА ДЛЯ НОРМАЛЬНОЙ ОСТАНОВКИ
void stop_norm() {
lcd.clear();
while (true) {
    lcd.setCursor(0,1);
    lcd.print("NORM STOP");
    detachInterrupt(INT_NUM); //обесточиваем симистор
    digitalWrite(DIMMER_PIN, 0); //выключаем тиристор принудительно
    digitalWrite(PUMP_PIN, 0); //выключаем помпу
    analogWrite(PWM_PIN, 0); //выставляем минимум на кулеры
    delay(1000); //повторяем бесконечно, раз в секунду
  }
}

// THE END