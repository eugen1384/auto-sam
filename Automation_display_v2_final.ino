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
#define BTN_PIN1 7 //режим первого перегона (выше температура выключения нагрев. до 20% в струе, мощность на ТЭН побольше)
#define BTN_PIN2 8 //режим второго перегона (ниже температура выключения нагрева, до 40% в струе, мощность на ТЭН поменьше)
#define BTN_PIN3 9 //ручной режим без температуры остановки (только аварийный стоп при 99) 
//PWM НА КУЛЕРЫ
#define PWM_PIN 6
//MOSFET Помпы
#define PUMP_PIN 10

//i2C ДИСПЛЕЯ ПОДКЛЮЧЕН НА A4(SDA), A5(SCL)
//ДИСПЛЕЙ 20х4
LiquidCrystal_I2C lcd(0x27, 20, 4);
//ИНИЦИАЛИЗАЦИЯ ДАТЧИКОВ ТЕМПЕРАТУРЫ. MicroDS18B20<PIN> <object_name>;
MicroDS18B20<4> sensor1;
MicroDS18B20<5> sensor2;

//ОПРЕДЕЛЯЕМ ПЕРЕМЕННЫЕ
float cube_temp;     //температура в кубе (идет так же на дисплей)
float water_temp;    //температура воды (идет так же на дисплей)
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
int dim_reg = 0; //флаг регулировки ТЭН-а через прерывание
int read_request = 0;

///НАЧАЛЬНЫЕ УСТАНОВКИ

void setup() {
analogWrite(6, 0); //делаем кулеры тише
//ИНИЦИАЛИЗАЦИЯ ДИСПЛЕЯ
lcd.init(); //первичная инициализация
lcd.backlight(); //ПОдсветка
//РИСУЕМ ЗАСТАВКУ
lcd.blink();
char line1[] = "SAMOGON";
char line2[] = "AUTOMATION V 2.0";
char line3[] = "....................";
lcd.setCursor(0, 0);
  for (int i = 0; i < strlen(line1); i++) {
    lcd.print(line1[i]);
    delay(50);
  }
lcd.setCursor(0, 1);
  for (int i = 0; i < strlen(line2); i++) {
    lcd.print(line2[i]);
    delay(50);
  }
lcd.noBlink();
lcd.setCursor(0, 3);
  for (int i = 0; i < strlen(line3); i++) {
    lcd.print(line3[i]);
    delay(80);
  }
//ОЧИЩАЕМ ДИСПЛЕЙ ПЕРЕД СЛЕДУЮЩИМ ЭКРАНОМ
lcd.clear();
///ДЛЯ РЕГУЛИРОВКИ ТЭНА через GyverTimers и тиристорный диммер 
pinMode(ZERO_PIN, INPUT_PULLUP);
pinMode(DIMMER_PIN, OUTPUT);
attachInterrupt(INT_NUM, isr, FALLING);  // для самодельной схемы ставь FALLING
Timer2.enableISR();
//задаем PIN MODE там где требуется
pinMode(R_HEAT, INPUT);
pinMode(R_COOL, INPUT);
pinMode(BTN_PIN1, INPUT_PULLUP); //подтянут резистором в чипе к VCC
pinMode(BTN_PIN2, INPUT_PULLUP); //подтянут резистором в чипе к VCC
pinMode(BTN_PIN3, INPUT_PULLUP); //подтянут резистором в чипе к VCC
pinMode(PWM_PIN, OUTPUT);
pinMode(PUMP_PIN, OUTPUT);
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
}

//////////////////////////////////////////////
//ОСНОВНОЙ АЛГОРИТМ КОНТРОЛЯ

//ВЫБОР РЕЖИМА на флаге mode
if (!digitalRead(BTN_PIN1)) {
  mode = 1;
}
if (!digitalRead(BTN_PIN2)) {
  mode = 2;
}
if (digitalRead(BTN_PIN3)) {
  mode = 3;
}
//полный диапазон значений для dimmer - max=500мкс, min=9300мкс
//считается от точки перехода напряжения через 0 до включения симистора.
//время одной полуволны на 50Гц = 10тыс мкс, Но нужно время на обработку функций и прерываний
//для стабильности возьмем диапазон 9000 - 500, так как были замечены артефакты при минимальной мощности, да и в целом нам тут 100Вт на ТЭН не нужно совсем. 

//ЗАДАНИЕ МОЩНОСТИ НАГРЕВА ТЭНА, Пока примерно
if (mode == 3 && analogRead(R_HEAT) < 1000) {
  dim_reg = 1;
  dimmer = map(analogRead(R_HEAT), 0, 1024, 9000, 500);
  ten_pow = map(analogRead(R_HEAT), 0, 1024, 0, 100);
  }
if (mode == 3 && analogRead(R_HEAT) >= 1000) {
  dim_reg = 0;
  ten_pow = 100;
  digitalWrite(DIMMER_PIN, 1);
}

//после достижения 75 градусов в кубе включается регулирование диммером
//На режиме 1 экспериментально получена зависимость скорости выхода от температуры в кубе с изменением мощности. 
//старт на 60% при 78 град в кубе, отбор голов и начало отбора тела
//Далее процесс идет с плавным повышением до 75% мощности на 98 град в кубе.
//На втором режиме можно уменьшить до 50% На старте и 65% на финише для более качественного отбора. 
//диапазон 8500мкс умножаем на обратный % (для 7% можности умножаем на 0.3) получаем значение для границ. При этом большее число = меньшая мощность! 

if (mode == 1 && cube_temp >= 75) {
    dim_reg = 1;
    dimmer = map(cube_temp, 75, 98, 4000, 2600); //3400 = 60%, 2125 = 75%
    ten_pow = map(dimmer, 9000, 500, 0, 100);
}
if (mode == 1 && cube_temp < 75) {
  dim_reg = 0; 
  ten_pow = 100;
  digitalWrite(DIMMER_PIN, 1); 
}

if (mode == 2 && cube_temp >= 75) {
    dim_reg = 1;
    dimmer = map(cube_temp, 75, 96, 4500, 2800); //4250 = 50%, 2975 = 65%
    ten_pow = map(dimmer, 9000, 500, 0, 100);
}
if (mode == 2 && cube_temp < 75) {
  dim_reg = 0;
  ten_pow = 100;
  digitalWrite(DIMMER_PIN, 1);
}

//НОРМАЛЬЫНЕ СТОПЫ ПО РЕЖИМАМ
//На первом режиме примерно на 98 градусах, соответствует отбору до 20-25% в струе
if (mode == 1 && cube_temp >= 98) {
  stop_norm();
}
//На втором режиме примерно на 96 градусах, соответствует отбору 40-45% в струе
if (mode == 2 && cube_temp >= 96) {
  stop_norm();
}
//Зависимость +/- линейная и получена экспериментальным путем перегона. 

//ЗАДАНИЕ МОЩНОСТИ PWM КУЛЕРОВ по температуре воды. Плавное саморегулирование на режимах 1 и 2.
if (mode != 3) {
  pwm_pow_map = map(water_temp, 15, 45, 0, 255);
  analogWrite(PWM_PIN, pwm_pow_map);
  pwm_pow = map(pwm_pow_map, 0, 255, 0, 100);  
}
//Ручное регулирование на режиме 3
if (mode == 3) {
  pwm_pow_map = map(analogRead(R_COOL), 0, 1024, 0, 255);
  analogWrite(PWM_PIN, pwm_pow_map);
  pwm_pow = map(pwm_pow_map, 0, 255, 0, 100);
}

//УПРАВЛЕНИЕ ПОМПОЙ, включение на 60 градусах в кубе
//для ручного включения предусмотрен выключатель подающий 5В аппаратно на GATE MOSFET-а.  
if (cube_temp >= 60) {
  digitalWrite(PUMP_PIN, 1);
  pump_state = "ON"; 
 }
else {
  digitalWrite(PUMP_PIN, 0);
  pump_state = "OFF";
}

//АВАРИЙНЫЕ СТОП-ы
if (cube_temp >= 99) { //по температуре куба 99, нам точно не нужно столько
  stop_cube();
}
if (water_temp >= 45) { //по температуре охлаждающей жидкости. На 45 это уже сильно мешает процессу
  stop_water();  
}

///// ВЫВОД НА ДИСПЛЕЙ ИНФОРМАЦИИ ПО ТАЙМЕРУ раз в 500мс, экспериментально выбрано, как наиболее приятное глазу с минимальным лагом 
static uint32_t tmr;
if (millis() - tmr >= 500) {
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

//вывод мощности PWM кулеров охлаждения овды
 lcd.setCursor(12,1);
 lcd.print("AIR:");
 lcd.setCursor(16,1);
 lcd.print("   ");
 lcd.setCursor(16,1);
 lcd.print(pwm_pow);
 lcd.setCursor(19,1);
 lcd.print("%");

// вывод статуса помпы
 lcd.setCursor(12,2);
 lcd.print("PUMP:");
 lcd.setCursor(17,2);
 lcd.print("   ");
 lcd.setCursor(17,2);
 lcd.print(pump_state);

//вывод информации о режиме работы
lcd.setCursor(0,3);
lcd.print("MODE:");
lcd.setCursor(6,3);
if (mode == 3) lcd.print("MANUAL ");
if (mode == 2) lcd.print("SECOND ");
if (mode == 1) lcd.print("FIRST  ");

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
 }
//КОНЕЦ ВЫВОДА НА ДИСПЛЕЙ


//КОНЕЦ ЦИКЛА loop
}

// Тирсторный регулятор. Прерывание детектора нуля
//нужно для корректировки периода и перезапуска таймера
void isr() {
  if (dim_reg != 0) {
  static int lastDim;
  if (lastDim != dimmer) Timer2.setPeriod(lastDim = dimmer);
  else Timer2.restart();
  }
}
// Тиристорный регулятор. Срабатывание таймера после перехода напряжения через 0(таймер запускается в прерывании), считает N микросекунд с прерывания и вызывает функцию ISR().
ISR(TIMER2_A) {
  if (dim_reg != 0) {
  digitalWrite(DIMMER_PIN, 1);  // включаем симистор на оставшейся после  отсчета таймером полуволне
  Timer2.stop();                // останавливаем таймер до следующего прерывания где он будет перезапущен
  digitalWrite(DIMMER_PIN, 0);  // выставляем 0 на оптопаре сразу, но тиристор остается открытым до прохождения нуля в напряжении сети. Особенность работы триака. 
  }
}

///аварийный стоп по кубу
void stop_cube() {
lcd.clear();
while (true) {
    lcd.setCursor(0,1);
    lcd.print("ERROR:CUBE_OVERHEAT");
    detachInterrupt(0);
    digitalWrite(DIMMER_PIN, 0); //выключаем тиристор принудительно
    digitalWrite(PUMP_PIN, 0); //выключаем помпу
    analogWrite(PWM_PIN, 0); //выставляем минимум на кулеры
    delay(1000); //повторяем бесконечно, раз в секунду
  }
}

///ЦИКЛ ЛОВУШКА ДЛЯ ОСТАНОВКИ ПО ПЕРЕГРЕВУ ВОДЫ
void stop_water() {
lcd.clear();
while (true) {
    lcd.setCursor(0,1);
    lcd.print("ERROR:WATER_OVERHEAT");
    detachInterrupt(0);
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
    lcd.print("NORMAL STOP");
    detachInterrupt(0);
    digitalWrite(DIMMER_PIN, 0); //выключаем тиристор принудительно
    digitalWrite(PUMP_PIN, 0); //выключаем помпу
    analogWrite(PWM_PIN, 0); //выставляем минимум на кулеры
    delay(1000); //повторяем бесконечно, раз в секунду
  }
}

// THE END