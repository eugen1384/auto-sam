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
#define KL1_PIN 8      // КЛАПАН_ГОЛОВЫ
#define KL2_PIN 12     // КЛАПАН_ТЕЛО
#define TC_PIN 4       // ТЕМПЕРАТУРА КУБ
#define TW_PIN 5       // ТЕМПЕРАТУРА ВОДА
#define TUO_PIN 9      // ТЕМПЕРАТУРА УЗЕЛ ОТБОРА
//КОНСТАНТЫ РЕЖИМОВ
#define STOP_M1 98     // ТЕМПЕРАТУРА ОСТАНОВКИ РЕЖИМА P1(первый перегон)
#define STOP_M2 96     // ТЕМПЕРАТУРА ОСТАНОВКИ РЕЖИМА P2(второй перегон)
#define FAIL_STOP_C 98.60 // ТЕМПЕРАТУРА АВАРИЙНОЙ ОСТАНОВКИ(КУБ)
#define FAIL_STOP_W 45 // ТЕМПЕРАТУРА АВАРИЙНОЙ ОСТАНОВКИ(ВОДА)
#define P_START_UO 45  // ТЕМПЕРАТУРА ВКЛ ПОМПЫ ПО УЗЛУ ОТБОРА 
#define P_START_C  65  // ТЕМПЕРАТУРА ВКЛ ПОМПЫ ПО КУБУ
#define TUO_REF 70     // НИЖНЯЯ ГРАНИЧНАЯ ТЕМПЕРАТУРА УЗЛА ОТБОРА ДЛЯ РЕЖИМОВ
#define MIN_POW 5800   // мощность начала процесса
#define MAX_POW 4800   // мощность в конце процесса 
//ТАЙМЕРЫ КЛАПАНОВ(СКОРОСТЬ ОТБОРА)
#define KL1_PER 20000  // ПЕРИОД ВКЛЮЧЕНИЯ КЛАПАНА ОТБОРА ГОЛОВ (millis)
#define KL2_PER 15000  // ПЕРИОД ВКЛЮЧЕНИЯ КЛАПАНА ОТБОРА ТЕЛА (millis)
#define KL1_OFF 200    // ВРЕМЯ РАБОТЫ КЛАПАНА ОТБОРА ГОЛОВ (millis) 10c/200ms примерно 400мл/час
#define KL2_OFF 300    // ВРЕМЯ РАБОТЫ КЛАПАНА ОТБОРА ТЕЛА (millis)
//СЧЕТЧИКИ ВРЕМЕНИ
#define TSELF 600      // ВРЕМЯ РАБОТЫ КОЛОННЫ НА СЕБЯ (sec) ~10 мин
//ПАРАМЕТРЫ ТЕМПЕРАТУРЫ И ИНТЕНСИВНОСТИ СНИЖЕНИЯ СКОРОСТИ ОТБОРА
#define DECR 50        // ДЕКРЕМЕНТ СНИЖЕНИЯ СКОРОСТИ ОТБОРА
#define DELT 0.15      // ДЕЛЬТА ТЕМПЕРАТУРЫ УЗЛА ОТБОРА
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
float uo_temp_fix;      // ТЕМП. ФИКСАЦИИ ОТБОРА
float uo_temp_fix2;     // ТЕМП. ФИКСАЦИИ ОТБОРА 2
int mode = 5;           // ГЛАВНЫЙ РЕЖИМ РАБОТЫ. ПО УМОЛЧАНИЮ 4(РУЧНОЙ)
int pwm_pow;            // ШИМ КУЛЕРОВ (для дисплея)
int pwm_pow_map;        // ШИМ КУЛЕРОВ ДЛЯ РАСЧЕТОВ
int ten_pow;            // МОЩНОСТЬ ТЭН (для дисплея)
int ten_pow_map;        // МОЩНОСТЬ ТЭН ДЛЯ РАСЧЕТОВ
int dimmer;             // ДИММЕР. ХРАНИТ ВРЕМЯ ВКЛ СИМИСТОРА В мкс ОТ ПРОХОЖДЕНИЯ НУЛЯ СИНУСОИДОЙ
int rheat;              // ЧТЕНИЕ ЗАДАЮЩЕГО R НАГРЕВА
int count_self;         // СЧЕТЧИК ВРЕМЕНИ РАБОТЫ "НА СЕБЯ"
int cnt_self;           // ДЛЯ ДИСПЛЕЯ (МИНУТЫ) 
int count_head;         // СЧЕТЧИК ДЛЯ ВРЕМЕНИ ОТБОРА ГОЛОВ
int cnt_head;           // ДЛЯ ДИСПЛЕЯ (МИНУТЫ) 
int count_body;         // СЧЕТЧИК ВРЕМЕНИ РАБОТЫ ОТБОРА ТЕЛА
int cnt_body;           // ДЛЯ ДИСПЛЕЯ (МИНУТЫ) 
int kl2_off;            // ПЕРЕМЕННАЯ ДЛЯ РЕГУЛИРОВКИ СКОРОСТИ ОТБОРА ТЕЛА
int xflag_count;        // СЧЕТЧИК ПРЕВЫШЕНИЙ ТЕМПЕРАТУРЫ УО
int head_time;          // ЗАДАНИЕ ВРЕМЕНИ ОТБОРА ГОЛОВ
int head_time_map;      // ЗНАЧЕНИЕ В МИН
int head_time_disp;     // ВРЕМЯ ОТБОРА ГОЛОВ ДЛЯ ДИСПЛЕЯ
bool tflag = 0;         // ФЛАГ ФИКСАЦИИ ТЕМПЕРАТУРЫ УО 
bool xflag = 0;         // ФЛАГ ЗАВЫШЕНИЯ ТЕМПЕРАТУРЫ УО
bool star;              //ФЛАГ ИНДИКАТОРА РАБОТЫ
String pump_state;      // ON/OFF ДЛЯ ДИСПЛЕЯ
String mode_desc;       // ОПИСАНИЕ РЕЖИМА ДЛЯ ДИСПЛЕЯ
String kl1_state = "--";       // СТАТУС КЛАПАНА 1
String kl2_state = "--";       // СТАТУС КЛАПАНА 2
String submode;    // ИНДИКАТОР ПОДРЕЖИМА РАБОТЫ РЕКТИФИКАЦИИ

//НАЧАЛЬНЫЕ УСТАНОВКИ
void setup() {
analogWrite(6, 0);      //КУЛЕРЫ НА НОЛЬ
lcd.init();             //INIT ДИСПЛЕЯ
lcd.backlight();        //ПОДСВЕТКА ВКЛ
// ЗАСТАВКА ПРИ СТАРТЕ
lcd.blink();
char line1[] = "SAMOGON";
char line2[] = "AUTOMATION v2.6.1t";
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
    delay(80);
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
pinMode(KL1_PIN, OUTPUT);
pinMode(KL2_PIN, OUTPUT);
}// КОНЕЦ setup()

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
}// КОНЕЦ ОПРОСА ДАТЧИКОВ

//########################################################
//ВЫБОР РЕЖИМА ПО НАЖАТИЮ КНОПКИ. ОПРОС КНОПКИ РАЗ В 0.9 СЕК
static uint32_t tmr_mode;
if (millis() - tmr_mode >= 550) {
    tmr_mode = millis();  
if (!digitalRead(BTN_PIN)) {
mode = mode + 1;
if (mode > 4) {        //ТУТ ЗАДАЕМ МАКСИМАЛЬНОЕ ЧИСЛО РЕЖИМОВ
  mode = 1;
  }
 }
// ЗАДАЕМ ОПИСАНИЕ РЕЖИМОВ ДЛЯ ОТОБРАЖЕНИЯ НА ДИСПЛЕЕ
if (mode == 1) mode_desc = "P1";
if (mode == 2) mode_desc = "P2";
if (mode == 3) mode_desc = "Re";
if (mode == 4) mode_desc = "Mn";
}// КОНЕЦ ОПРОСА КНОПКИ И ВЫБОРА РЕЖИМА

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
  submode = "R";
 }
if (mode == 1 && cube_temp >= 75) {
    attachInterrupt(INT_NUM, isr, FALLING);
    dimmer = map(cube_temp, 75, 98, 4000, 2500); // МЕНЯЕТСЯ ОТ 60% до 75%
    ten_pow = map(dimmer, 9000, 500, 0, 100);
    submode = "P";
 }
// РЕЖИМ "POTSTILL 2" ВТОРОЙ ПЕРЕГОНКИ (МОЩНОСТЬ НИЖЕ)
if (mode == 2 && cube_temp < 75) {
  detachInterrupt(INT_NUM);
  ten_pow = 100;
  digitalWrite(DIMMER_PIN, 1);
  submode = "R";
 }
if (mode == 2 && cube_temp >= 75) {
    attachInterrupt(INT_NUM, isr, FALLING);
    dimmer = map(cube_temp, 75, 96, 5000, 3500); // МЕНЯЕТСЯ ОТ 50% ДО 65%
    ten_pow = map(dimmer, 9000, 500, 0, 100);
    submode = "P";
 }
// РЕЖИМЫ РЕКТИФИКАЦИИ. ПО ТЕМПЕРАТУРЕ В УЗЛЕ ОТБОРА
if ((mode == 3 || mode == 4) && uo_temp < TUO_REF) {
  detachInterrupt(INT_NUM);
  ten_pow = 100;
  digitalWrite(DIMMER_PIN, 1);
}
if ((mode == 3 || mode == 4) && uo_temp >= TUO_REF) {
    attachInterrupt(INT_NUM, isr, FALLING);
    dimmer =  map(cube_temp, 80, 98, MIN_POW, MAX_POW); // МЕНЯЕТСЯ (примерно) ОТ 55% ДО 65%
    ten_pow = map(dimmer, 9000, 500, 0, 100);    
}
if (mode == 5){
  submode = "N";
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
}// КОНЕЦ УПРАВЛЕНИЯ МОЩНОСТЬЮ ТЭНа 

//#########################################
// ОСНОВНАЯ ЛОГИКА РАБОТЫ РЕЖИМ - РЕКТИФИКАЦИЯ
// СЧЕТЧИК ВРЕМЕНИ РАБОТЫ "НА СЕБЯ"
static uint32_t tmr_self;
if (millis() - tmr_self >= 1000) {
    tmr_self = millis();
if (mode == 3 && count_self < TSELF && uo_temp < TUO_REF){
   submode = "R"; //ИНДИКАЦИЯ "РАЗГОН"
}
if (mode == 3 && count_self < TSELF && uo_temp >= TUO_REF) {
   count_self = count_self + 1;
   submode = "S"; //ИНДИКАЦИЯ "РАБОТА НА СЕБЯ"
 }
}
//ЗАДАЕМ ВРЕМЯ ОТБОРА ГОЛОВ ПОТЕНЦИОМЕТРОМ УПРАВЛЕНИЯ КУЛЕРАМИ
if (mode == 3) {
  head_time_map = map(analogRead(R_COOL), 0, 1024, 0, 150); //(150мин) 2.5 ЧАСА МАКСИМУМ
  head_time = head_time_map * 60;
  head_time_disp = head_time_map;
}
//ОЧИЩАЕМ ПОЛЕ ВРЕМЕНИ ОТБОРА ГОЛОВ НА ДРУГИХ РЕЖИМАХ
if (mode != 3) {
  head_time_disp = 0; 
}

//ОТБОР ГОЛОВ. ПОКА СЧЕТКИК НЕ ДОЙДЕТ ДО ЗАДАННОГО ВРЕМЕНИ
if (mode == 3 && uo_temp > TUO_REF && count_self >= TSELF && count_head <= head_time) {
    submode = "H"; //ИНДИКАЦИЯ "ОТБОР ГОЛОВ"
    digitalWrite(KL2_PIN, 0); //закрываем клапан отбора тела(если вдруг вернулись добрать головы) 
//наращиваем по таймеру счетчик
static uint32_t tmr_head;
if (millis() - tmr_head >= 1000) {
    tmr_head = millis();
    count_head = count_head + 1;
    tflag = 0;     //сбрасываем флаг задания эталонной температуры      
 }
// РАБОТА КЛАПАНА ОТБОРА
// millis() – миллисекунды, тип unsigned long, от 1 до 4 294 967 295 мс (~50 суток), разрешение 1 мс. 
// Так что хватит на время перегона точно без сбоев.
static uint32_t tmr_kl1_head;
if (millis() - tmr_kl1_head >= KL1_PER) {
    tmr_kl1_head = millis();
    digitalWrite(KL1_PIN, 1); //открыли клапан 1
    kl1_state = ">>";         //индикация работы клапана
 }
if (millis() - tmr_kl1_head >= KL1_OFF) {  //закрыли клапан если значение времени больше времени открытия KL1_OFF
    digitalWrite(KL1_PIN, 0);
    kl1_state = "--";
}
// так продолжаем отбирать пока значение счетчика count_head не будет >= времени THEAD
}// КОНЕЦ РЕЖИМА ОТБОРА ГОЛОВ 

// ОТБОР "ТЕЛА" ПРОДУКТА
// фиксируем температуру на момент окончания отбора голов в перемнной uo_temp_fix
if (mode == 3 && uo_temp > TUO_REF && count_self >= TSELF && count_head >= head_time && !tflag) {
   uo_temp_fix = uo_temp;          // забрали температуру как эталон
   kl2_off = KL2_OFF;              // задали переменной значение константы для стартовой скорости отбора. константу перезаписывать нельзя
   tflag = 1;                      // флаг выставляем в 1, чтобы больше сюда уже не попадать.
   digitalWrite(KL1_PIN, 0);       // перекрываем клапан отбора голов(в алгоритме работы по таймеру есть недочет - период работы клапана сильно больше счетчика режима). 
   kl1_state = "--"; 
   submode = "X";
}

// И дальше работаем уже после фиксации температуры
if (mode == 3 && uo_temp > TUO_REF && count_self >= TSELF && count_head >= head_time && tflag) {
   submode = "B"; //ОТБОР ТЕЛА
static uint32_t tmr_bgg;
if (millis() - tmr_bgg >= 1000) {
    tmr_bgg = millis();
    count_body = count_body + 1;
}
//РАБОТА КЛАПАНА ОТБОРА 
static uint32_t tmr_kl2_body;
if ((millis() - tmr_kl2_body >= KL2_PER) && (uo_temp < (uo_temp_fix + DELT))) {
    tmr_kl2_body = millis();
    digitalWrite(KL2_PIN, 1); //открыли клапан
    kl2_state = ">>";
    xflag = 0;  //  сбрасываем флаг если температура пришла в норму
 }
if (millis() - tmr_kl2_body >= kl2_off) {  //закрыли клапан если значение времени больше времени открытия
    digitalWrite(KL2_PIN, 0);
    kl2_state = "--";
}
// если темпертара залезла выше uo_temp_fix + DELT, убавляем скорость отбора, ставим флаг завышения, увеличиваем счетчик завышений.
if ((uo_temp >= (uo_temp_fix + DELT)) && !xflag) {
  kl2_off = kl2_off - DECR;
  xflag = 1;
  xflag_count = xflag_count + 1;
}
// нормальная остановка по исчерпанию окна открытия клапана отбора (несколько завышений температуры)
if (kl2_off < 60) {
  stop_norm();
}
// КОНЕЦ РАБОТЫ КЛАПАНА ОТБОРА
// КОНЕЦ РАБОТЫ ПО ОТБОРУ "ТЕЛА"
}

//#################################################
// РЕЖИМ АВАРИЙНОЙ РЕКТИФИКАЦИИ №2
//


//#################################################
// УПРАВЛЕНИЕ ПОМПОЙ НА РАЗНЫХ РЕЖИМАХ (АСИНХРОННО)
static uint32_t tmr_pump;
if (millis() - tmr_pump >= 452) {
    tmr_pump = millis();
// POTSTILLs & MANUAL
if ((mode == 1 || mode == 2 || mode == 4) && cube_temp >= P_START_C) {
  digitalWrite(PUMP_PIN, 1);
  pump_state = " ON"; 
 }
if ((mode == 1 || mode == 2 || mode == 4) && cube_temp < P_START_C) {
  digitalWrite(PUMP_PIN, 0);
  pump_state = "OFF";
 }
// RECTIFICATION
if (mode == 3 && uo_temp >= P_START_UO) {
  digitalWrite(PUMP_PIN, 1);
  pump_state = " ON"; 
 }
if (mode == 3 && uo_temp < P_START_UO) {
  digitalWrite(PUMP_PIN, 0);
  pump_state = "OFF";
 }
}// КОНЕЦ УПРАВЛЕНИЯ ПОМПОЙ

//##################################################
// УПРАВЛЕНИЕ КУЛЕРАМИ (АСИНХРОННОЕ) 
static uint32_t tmr_air;
if (millis() - tmr_air >= 450) {
    tmr_air = millis();
if (mode != 4 && water_temp > 1) {
  pwm_pow_map = map(water_temp, 10, 45, 0, 255);
  analogWrite(PWM_PIN, pwm_pow_map);
  pwm_pow = map(pwm_pow_map, 0, 255, 0, 100);  
 }
if (mode !=3 && water_temp < 10) {
  pwm_pow = 0;
 }
if (mode == 4) {
  pwm_pow_map = map(analogRead(R_COOL), 0, 1024, 0, 255);
  analogWrite(PWM_PIN, pwm_pow_map);
  pwm_pow = map(pwm_pow_map, 0, 255, 0, 100);
 }
}// КОНЕЦ УПРАВЛЕНИЯ КУЛЕРАМИ

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
// ВЫВОД НА ДИСПЛЕЙ ИНФОРМАЦИИ ПО ТАЙМЕРУ РАЗ В 300мс
static uint32_t tmr;
if (millis() - tmr >= 300) {
    tmr = millis();
// вывод температуры куба
lcd.setCursor(0,0);
lcd.print("Tk:");
lcd.setCursor(3,0);
lcd.print(cube_temp);
// вывод температуры воды в контуре охлаждения
lcd.setCursor(0,1);
lcd.print("Tw:");
lcd.setCursor(3,1);
lcd.print(water_temp);
// вывод температуры узла отбора
lcd.setCursor(0,2);
lcd.print("To:");
lcd.setCursor(3,2);
lcd.print(uo_temp);
//вывод зафиксированной температуры узла отбора при ректификации по каждому режиму ректификации
lcd.setCursor(0,3);
lcd.print("Tf:");
lcd.setCursor(3,3);
lcd.print(uo_temp_fix);
// вывод режима работы
lcd.setCursor(9,3);
lcd.print(mode_desc); 
// вывод мощности нагрева (%)
lcd.setCursor(9,0);
lcd.print("H:");
lcd.setCursor(11,0);
lcd.print("   ");
lcd.setCursor(11,0);
lcd.print(ten_pow);
// вывод счетчика на режиме, или мощности ШИМ кулеров для POTSTILL/MANUAL

if (mode == 1 || mode == 2 || mode == 4) {
lcd.setCursor(9,1);
lcd.print("A:");
lcd.setCursor(11,1);
lcd.print("   ");
lcd.setCursor(11,1);
lcd.print(pwm_pow);
}
if (mode ==3 && submode == "R") {
lcd.setCursor(9,1);
lcd.print("R:");
lcd.setCursor(11,1);
lcd.print("---");
}
if (mode ==3 && submode == "S") {
lcd.setCursor(9,1);
lcd.print("S:");
lcd.setCursor(11,1);
lcd.print("   ");
lcd.setCursor(11,1);
cnt_self = count_self / 60;
lcd.print(cnt_self);
}
if (mode ==3 && submode == "H") {
lcd.setCursor(9,1);
lcd.print("S:");
lcd.setCursor(11,1);
lcd.print("   ");
lcd.setCursor(11,1);
cnt_head = count_head / 60;
lcd.print(cnt_head);
}
if (mode ==3 && submode == "B") {
lcd.setCursor(9,1);
lcd.print("S:");
lcd.setCursor(11,1);
lcd.print("   ");
lcd.setCursor(11,1);
cnt_body = count_body / 60;
lcd.print(cnt_body);
}
// вывод статуса помпы
lcd.setCursor(9,2);
lcd.print("P:");
lcd.setCursor(11,2);
lcd.print(pump_state);
//вывод состояния клапанов
lcd.setCursor(15,0);
lcd.print("K1:");
lcd.setCursor(18,0);
lcd.print(kl1_state);
lcd.setCursor(15,1);
lcd.print("K2:");
lcd.setCursor(18,1);
lcd.print(kl2_state);
//счетчик завышений по температуре узла отбора
lcd.setCursor(15,2);
lcd.print("T^:");
lcd.setCursor(18,2);
lcd.print(xflag_count);
//Время отбора голов в минутах
lcd.setCursor(14,3);
lcd.print("Ht:");
lcd.setCursor(17,3);
lcd.print("   ");
lcd.setCursor(17,3);
lcd.print(head_time_disp);
//Подрежим
lcd.setCursor(12,3);
lcd.print(submode);
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
    digitalWrite(KL1_PIN, 0);    // закрываем клапан 1
    digitalWrite(KL2_PIN, 0);    // закрываем клапан 2
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
    digitalWrite(KL1_PIN, 0);     //закрываем клапан 1
    digitalWrite(KL2_PIN, 0);     //закрываем клапан 2
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
    digitalWrite(KL1_PIN, 0);     //закрываем клапан 1
    digitalWrite(KL2_PIN, 0);     //закрываем клапан 2
    delay(1000);                  //повторяем бесконечно, раз в секунду
  }
}