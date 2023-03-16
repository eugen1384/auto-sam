// БИБЛИОТЕКИ
#include <LiquidCrystal_I2C.h>   // БИБЛИОТЕКА LCD ДИСПЛЕЯ
#include <GyverTimers.h>        // ТАЙМЕРЫ
#include <microDS18B20.h>       // ТЕМПЕРАТУРА DS18B20
#include <EncButton.h>
#include <EEPROM.h>
#define INT_NUM 0               // НОМЕР ПРЕРЫВАНИЯ ДЕТЕКТОРА НУЛЯ
// ПИНЫ
#define ZERO_PIN 2              // ПИН ДЕТЕКТОРА НУЛЯ
#define DIMMER_PIN 3            // УПРАВЛЕНИЕ СИМИСТОРА
#define PWM_PIN 6               // ВЫХОД ДЛЯ ШИМ КУЛЕРОВ
#define PUMP_PIN 10             // MOSFET ПОМПЫ
#define KL1_PIN 8               // MOSFET КЛАПАН ОТБОРА "ГОЛОВ"
#define KL2_PIN 12              // MOSFET КЛАПАН ОТБОРА "ТЕЛА"
#define TC_PIN 11 //4                // ДАТЧИК ТЕМПЕРАТУРЫ КУБ
#define TW_PIN 5                // ДАТЧИК ТЕМПЕРАТУРЫ ВОДА
#define TUO_PIN 9               // ДАТЧИК ТЕМПЕРАТУРЫ УЗЕЛ ОТБОРА
#define BTN_PIN 4               // КНОПКА ЭНКОДЕРА
#define S1_PIN A1               // ЭНКОДЕР СИГНАЛ
#define S2_PIN A2               // ЭНКОДЕР СИГНАЛ
// КОНСТАНТЫ
#define STOP_M1 98              // ТЕМПЕРАТУРА ОСТАНОВКИ РЕЖИМА P1(первый перегон)
#define STOP_M2 96              // ТЕМПЕРАТУРА ОСТАНОВКИ РЕЖИМА P2(второй перегон)
#define FAIL_STOP_C 98.50       // ТЕМПЕРАТУРА АВАРИЙНОЙ ОСТАНОВКИ(КУБ)
#define FAIL_STOP_D 45          // ТЕМПЕРАТУРА АВАРИЙНОЙ ОСТАНОВКИ(ВОДА)
#define P_START_UO 45           // ТЕМПЕРАТУРА ВКЛ ПОМПЫ ПО УЗЛУ ОТБОРА 
#define P_START_C  65           // ТЕМПЕРАТУРА ВКЛ ПОМПЫ ПО КУБУ
#define TUO_REF 73              // НИЖНЯЯ ГРАНИЧНАЯ ТЕМПЕРАТУРА УЗЛА ОТБОРА ДЛЯ РЕЖИМОВ
#define KL1_PER 10000           // ПЕРИОД РАБОТЫ КЛАПАНА ОТБОРА ГОЛОВ (millis)
#define KL2_PER 10000           // ПЕРИОД РАБОТЫ КЛАПАНА ОТБОРА ТЕЛА (millis)
LiquidCrystal_I2C lcd(0x27, 20, 4);      // i2C ДИСПЛЕЯ ПОДКЛЮЧЕН НА A4(SDA), A5(SCL). ДИСПЛЕЙ 20x4
EncButton<EB_TICK, S1_PIN, S2_PIN, BTN_PIN> enc; // ОБЪЯВЛЯЕМ ЭНКОДЕР С КНОПКОЙ
// ИНИЦИАЛИЗАЦИЯ ДАТЧИКОВ ТЕМПЕРАТУРЫ. MicroDS18B20<PIN> <object_name>;
MicroDS18B20<TC_PIN> sensor_cube;
MicroDS18B20<TW_PIN> sensor_defl;
MicroDS18B20<TUO_PIN> sensor_out;
// ОПРЕДЕЛЯЕМ ПЕРЕМЕННЫЕ
float cube_temp;        // ТЕМП. В КУБЕ (идет на дисплей)
float defl_temp;        // ТЕМП. ДЕФЛЕГМАТОРА (идет на дисплей)
float uo_temp;          // ТЕМП. УЗЛА ОТБОРА (идет на дисплей) 
float uo_temp_fix;      // ТЕМП. ФИКСАЦИИ ОТБОРА
float delt;             // ДЕЛЬТА ЗАЛЕТА ПО ТЕМПЕРАТУРЕ
int mode = 4;           // ГЛАВНЫЙ РЕЖИМ РАБОТЫ. ПО УМОЛЧАНИЮ 4(MANUAL)
int pwm_pow;            // ШИМ КУЛЕРОВ (для дисплея)
int pwm_pow_map;        // ШИМ КУЛЕРОВ ДЛЯ РАСЧЕТОВ
int ten_pow;            // МОЩНОСТЬ ТЭН (для дисплея)
int ten_pow_map;        // МОЩНОСТЬ ТЭН ДЛЯ РАСЧЕТОВ
int dimmer;             // ДИММЕР. ХРАНИТ ВРЕМЯ ВКЛ СИМИСТОРА В мкс ОТ ПРОХОЖДЕНИЯ НУЛЯ СИНУСОИДОЙ
int count_self;         // СЧЕТЧИК ВРЕМЕНИ РАБОТЫ "НА СЕБЯ"
int cnt_self;           // ДЛЯ ДИСПЛЕЯ (МИНУТЫ) 
int count_head;         // СЧЕТЧИК ДЛЯ ВРЕМЕНИ ОТБОРА ГОЛОВ
int cnt_head;           // ДЛЯ ДИСПЛЕЯ (МИНУТЫ) 
int count_body;         // СЧЕТЧИК ВРЕМЕНИ РАБОТЫ ОТБОРА ТЕЛА
int cnt_body;           // ДЛЯ ДИСПЛЕЯ (МИНУТЫ) 
int xflag_count;        // СЧЕТЧИК ПРЕВЫШЕНИЙ ТЕМПЕРАТУРЫ УО
int head_time;          // ВРЕМЕНЯ ОТБОРА ГОЛОВ
int self_time;          // РАБОТА НА СЕБЯ
int k1time;             // % ВРЕМЕНИ ОТКРЫТИЯ КЛАПАНА 1
int k2time;             // % ВРЕМЕНИ ОТКРЫТИЯ КЛАПАНА 2
int decr;               // ДЕКРЕМЕНТ СНИЖЕНИЯ СКОРОСТИ ОТБОРА %
int repwrst;            // % МОЩНОСТИ ТЭН В НАЧАЛЕ РЕКТИФИКАЦИИ
int repwrnd;            // % МОЩНОСТИ ТЭН В КОНЦЕ РЕКТИФИКАЦИИ  
int p1pwrst;            // % МОЩНОСТИ ТЭН ПЕРЕГОН 1 НАЧАЛО
int p1pwrnd;            // % МОЩНОСТИ ТЭН ПЕРЕГОН 1 КОНЕЦ
int p2pwrst;            // % МОЩНОСТИ ТЭН ПЕРЕГОН 2 НАЧАЛО
int p2pwrnd;            // % МОЩНОСТИ ТЭН ПЕРЕГОН 2 КОНЕЦ
int mnpwr;              // % МОЩНОСТИ НА РУЧНОМ РЕЖИМЕ 
int ptr;                // УКАЗАТЕЛЬ В МЕНЮ УСТАНОВОК
bool is_set = 0;        // ФЛАГ УСТАНОВКИ ЗНАЧЕНИЯ ПЕРМЕННОЙ
bool in_menu = 0;       // ФЛАГ ПОПАДАНИЯ В МЕНЮ УСТАНОВОК
bool tflag = 0;         // ФЛАГ ФИКСАЦИИ ТЕМПЕРАТУРЫ УО 
bool xflag = 0;         // ФЛАГ ЗАВЫШЕНИЯ ТЕМПЕРАТУРЫ УО
String pump_state;      // ON/OFF ДЛЯ ДИСПЛЕЯ
String mode_desc;       // ОПИСАНИЕ РЕЖИМА ДЛЯ ДИСПЛЕЯ
String kl1_state = "-"; // СТАТУС КЛАПАНА 1
String kl2_state = "-"; // СТАТУС КЛАПАНА 2
String submode;         // ИНДИКАТОР ПОДРЕЖИМА РАБОТЫ РЕКТИФИКАЦИИ
/// МАССИВ СТРОК МЕНЮ УСТАНОВОК. РАЗБИТ ПО ЭКРАНАМ(СТОЛБЦЫ)
String menu_settings[4][4] = 
{
{"SELF TIME:    ","DELTA:        ","P1 PWR START: ","MODE:         "},
{"HEAD TIME:    ","DECREMENT:    ","P1 PWR END:   ","MANUAL PWR:   "},
{"K1 TIME:      ","RE PWR START: ","P2 PWR START: ","SAVE TO EEPROM"},
{"K2 TIME:      ","RE PWR END:   ","P2 PWR END:   ","EXIT TO MAIN  "}
};

// НАЧАЛЬНЫЕ УСТАНОВКИ
void setup() {
analogWrite(6, 0);      // КУЛЕРЫ НА НОЛЬ
lcd.init();             // INIT ДИСПЛЕЯ
lcd.backlight();        // ПОДСВЕТКА ВКЛ
// ЗАСТАВКА ПРИ СТАРТЕ
lcd.blink();
char line1[] = "SAMOGON";
char line2[] = "AUTOMATION v3.0";
char line3[] = "....................";
lcd.setCursor(0, 0);
  for (int i = 0; i < strlen(line1); i++) { lcd.print(line1[i]); delay(40); }
lcd.setCursor(0, 1);
  for (int i = 0; i < strlen(line2); i++) { lcd.print(line2[i]); delay(40); }
lcd.noBlink();
lcd.setCursor(0, 3);
  for (int i = 0; i < strlen(line3); i++) { lcd.print(line3[i]); delay(80); }
// ДЛЯ РЕГУЛИРОВКИ ТЭНА ЧЕРЕЗ GyverTimers И СИМИСТОР 
pinMode(ZERO_PIN, INPUT_PULLUP);
pinMode(DIMMER_PIN, OUTPUT); 
attachInterrupt(INT_NUM, isr, FALLING);  // Прерывание. Для самодельной схемы - FALLING (по падению на детекторе)
Timer2.enableISR(); // Включили таймер и повесили на него действие
// PIN MODE
pinMode(BTN_PIN, INPUT_PULLUP);
pinMode(PWM_PIN, OUTPUT);
pinMode(PUMP_PIN, OUTPUT);
pinMode(KL1_PIN, OUTPUT);
pinMode(KL2_PIN, OUTPUT);
// ЧИТАЕМ ЗНАЧЕНИЯ НАСТРОЕК ИЗ EEPROM. float = 4 байта, int = 2 байта. 
EEPROM.get(0,  delt);      
EEPROM.get(4,  self_time);
EEPROM.get(6,  head_time);
EEPROM.get(8,  k1time);
EEPROM.get(10, k2time);
EEPROM.get(12, decr);
EEPROM.get(14, repwrst);
EEPROM.get(16, repwrnd);
EEPROM.get(18, p1pwrst);
EEPROM.get(20, p1pwrnd);
EEPROM.get(22, p2pwrst);
EEPROM.get(24, p2pwrnd);
EEPROM.get(26, mnpwr);
lcd.clear();
}// КОНЕЦ setup()

void loop() {
enc.tick();        // ОПРОС ЭНКОДЕРА
if (enc.right()) { // ОБРАБОТКА ПОВОРОТОВ ЭНКОДЕРА (ВПРАВО), ПРИРАЩЕНИЕ ПЕРЕМЕННЫХ
  if (!is_set && in_menu){ ptr = constrain(ptr + 1, 0, 15); }
  if (is_set && ptr == 0 && in_menu) { self_time = constrain(self_time + 1, 0, 120); }
  if (is_set && ptr == 1 && in_menu) { head_time = constrain(head_time + 1, 0, 120); }
  if (is_set && ptr == 2 && in_menu) { k1time = constrain(k1time + 1, 0, 100); }
  if (is_set && ptr == 3 && in_menu) { k2time = constrain(k2time + 1, 0, 100); }
  if (is_set && ptr == 4 && in_menu) { delt = constrain(delt + 0.01, 0, 0.5); }
  if (is_set && ptr == 5 && in_menu) { decr = constrain(decr + 1, 0, 100); }
  if (is_set && ptr == 6 && in_menu) { repwrst = constrain(repwrst + 1, 0, 100); }
  if (is_set && ptr == 7 && in_menu) { repwrnd = constrain(repwrnd + 1, 0, 100); }
  if (is_set && ptr == 8 && in_menu) { p1pwrst = constrain(p1pwrst + 1, 0, 100); }
  if (is_set && ptr == 9 && in_menu) { p1pwrnd = constrain(p1pwrnd + 1, 0, 100); }
  if (is_set && ptr == 10 && in_menu) { p2pwrst = constrain(p2pwrst + 1, 0, 100); }
  if (is_set && ptr == 11 && in_menu) { p2pwrnd = constrain(p2pwrnd + 1, 0, 100); }
  if (is_set && ptr == 12 && in_menu) { mode = constrain(mode + 1, 1, 4); }
  if (is_set && ptr == 13 && in_menu) { mnpwr = constrain(mnpwr + 1, 0, 100); }
}
if (enc.left()) { // ОБРАБОТКА ПОВОРОТОВ ЭНКОДЕРА (ВЛЕВО)
  if (!is_set && in_menu){ ptr = constrain(ptr - 1, 0, 15); }
  if (is_set && ptr == 0 && in_menu) { self_time = constrain(self_time - 1, 0, 120); }
  if (is_set && ptr == 1 && in_menu) { head_time = constrain(head_time - 1, 0, 120); }
  if (is_set && ptr == 2 && in_menu) { k1time = constrain(k1time - 1, 0, 100); }
  if (is_set && ptr == 3 && in_menu) { k2time = constrain(k2time - 1, 0, 100); }
  if (is_set && ptr == 4 && in_menu) { delt = constrain(delt - 0.01, 0, 0.5); }
  if (is_set && ptr == 5 && in_menu) { decr = constrain(decr - 1, 0, 100); }
  if (is_set && ptr == 6 && in_menu) { repwrst = constrain(repwrst - 1, 0, 100); }
  if (is_set && ptr == 7 && in_menu) { repwrnd = constrain(repwrnd - 1, 0, 100); }  
  if (is_set && ptr == 8 && in_menu) { p1pwrst = constrain(p1pwrst - 1, 0, 100); }
  if (is_set && ptr == 9 && in_menu) { p1pwrnd = constrain(p1pwrnd - 1, 0, 100); }
  if (is_set && ptr == 10 && in_menu) { p2pwrst = constrain(p2pwrst - 1, 0, 100); }
  if (is_set && ptr == 11 && in_menu) { p2pwrnd = constrain(p2pwrnd - 1, 0, 100); }
  if (is_set && ptr == 12 && in_menu) { mode = constrain(mode - 1, 1, 4); }
  if (is_set && ptr == 13 && in_menu) { mnpwr = constrain(mnpwr - 1, 0, 100); }
}
// ОБРАБОТЧИК НАЖАТИЯ КНОПКИ ЭНКОДЕРА
if (enc.press()) { // УСТАНОВКА ПАРАМЕТРА
  if (in_menu) { is_set = !is_set; }
  if (!in_menu) { in_menu = 1; } // ВХОД В МЕНЮ УСТАНОВОК
  if (is_set && ptr == 15 && in_menu) { in_menu = 0; is_set = 0; } // ВЫХОД ИЗ МЕНЮ НА 14 ПУНКТЕ
  if (is_set && ptr == 14 && in_menu) { eeprom_write(); } // ЗАПИСЬ ВСЕХ ЗНАЧИМЫХ ПЕРЕМЕННЫХ В EEPROM 
}
// ОСТАНОВКИ
if (mode == 1 && cube_temp >= STOP_M1) { stop_norm(); } // ПО ТЕМП. В КУБЕ на P1
if (mode == 2 && cube_temp >= STOP_M2) { stop_norm(); } // ПО ТЕМП. В КУБЕ на P2
if (cube_temp >= FAIL_STOP_C) { stop_cube(); } // ПО ТЕМП. КУБА
if (defl_temp >= FAIL_STOP_D) { stop_defl(); } // ПО ТЕМП. ДЕФЛЕГМАТОР
// ОПИСАНИЕ РЕЖИМОВ ДЛЯ ДИСПЛЕЯ
if (mode == 1) { mode_desc = "P1 ";}
if (mode == 2) { mode_desc = "P2 ";}    
if (mode == 3) { mode_desc = "RE ";}
if (mode == 4) { mode_desc = "MN ";}
// #######
// ПОЛУЧЕНИЕ ТЕМПЕРАТУР С ДАТЧИКОВ
static uint32_t tmr_temp;
if (millis() - tmr_temp >= 1000) {
  tmr_temp = millis();
if (sensor_cube.readTemp()) { cube_temp = sensor_cube.getTemp(); sensor_cube.requestTemp(); }
else { sensor_cube.requestTemp(); }
if (sensor_defl.readTemp()) { defl_temp = sensor_defl.getTemp(); sensor_defl.requestTemp(); }
else { sensor_defl.requestTemp(); }
if (sensor_out.readTemp()) { uo_temp = sensor_out.getTemp(); sensor_out.requestTemp(); }
else { sensor_out.requestTemp(); }
}// КОНЕЦ ОПРОСА ДАТЧИКОВ
// ####### 
// ЗАДАНИЕ МОЩНОСТИ ТЭНа ПО РЕЖИМАМ
// РЕЖИМ "POTSTILL" ПЕРВОЙ ПЕРЕГОНКИ
static uint32_t tmr_pstill;
if (millis() - tmr_pstill >= 210) { tmr_pstill = millis();
if (mode == 1 && cube_temp < 75 ) {
  detachInterrupt(INT_NUM);
  ten_pow = 100;
  digitalWrite(DIMMER_PIN, 1);
  submode = "R"; }
if (mode == 1 && cube_temp >= 75) {
  attachInterrupt(INT_NUM, isr, FALLING);
  dimmer = map(cube_temp, 75, 98, map(p1pwrst, 0, 100, 9000, 500), map(p1pwrnd, 0, 100, 9000, 500));
  ten_pow = map(dimmer, 9000, 500, 0, 100);
  submode = "P"; }
// РЕЖИМ "POTSTILL 2" ВТОРОЙ ПЕРЕГОНКИ (МОЩНОСТЬ НИЖЕ)
if (mode == 2 && cube_temp < 75) {
  detachInterrupt(INT_NUM);
  ten_pow = 100;
  digitalWrite(DIMMER_PIN, 1);
  submode = "R"; }
if (mode == 2 && cube_temp >= 75) {
  attachInterrupt(INT_NUM, isr, FALLING);
  dimmer = map(cube_temp, 75, 96, map(p2pwrst, 0, 100, 9000, 500), map(p2pwrnd, 0, 100, 9000, 500));
  ten_pow = map(dimmer, 9000, 500, 0, 100);
  submode = "P"; }
// РЕЖИМ РЕКТИФИКАЦИИ. СТАРТ РЕГУЛИРОВКИ ПО ДОСТИЖЕНИЮ ТЕМПЕРАТУРЫ TUO_REF В УЗЛЕ ОТБОРА. САМА РЕГУЛИРОВКА ПО ТЕМПЕРАТУРЕ В КУБЕ. 
if (mode == 3 && uo_temp < TUO_REF) {
  detachInterrupt(INT_NUM);
  ten_pow = 100;
  digitalWrite(DIMMER_PIN, 1); }
if (mode == 3 && uo_temp >= TUO_REF) {
  attachInterrupt(INT_NUM, isr, FALLING);
  dimmer =  map(cube_temp, 80, 98, map(repwrst, 0, 100, 9000, 500), map(repwrnd, 0, 100, 9000, 500));
  ten_pow = map(dimmer, 9000, 500, 0, 100); }    
// РУЧНОЙ РЕЖИМ
if (mode == 4){
   submode = "N";
if (mnpwr < 100 && mnpwr > 0) {
   attachInterrupt(INT_NUM, isr, FALLING);
   dimmer = map(mnpwr, 0, 100, 9000, 500);
   ten_pow = mnpwr; }
if (mnpwr == 100) {
  detachInterrupt(INT_NUM);
  ten_pow = mnpwr;
  digitalWrite(DIMMER_PIN, 1); }
if (mnpwr < 1) {
  detachInterrupt(INT_NUM);
  ten_pow = mnpwr;
  digitalWrite(DIMMER_PIN, 0); }
 }//КОНЕУ РУЧНОГО РЕЖИМА
}// КОНЕЦ УПРАВЛЕНИЯ МОЩНОСТЬЮ ТЭНа 
// #######
// ОСНОВНАЯ ЛОГИКА РАБОТЫ РЕЖИМ - "РЕКТИФИКАЦИЯ"
// СЧЕТЧИК ВРЕМЕНИ РАБОТЫ "НА СЕБЯ"
static uint32_t tmr_self;
if (millis() - tmr_self >= 1000) { tmr_self = millis();
if (mode == 3 && count_self < (self_time * 60) && uo_temp < TUO_REF) { submode = "R"; }//ИНДИКАЦИЯ "РАЗГОН"
if (mode == 3 && count_self < (self_time * 60) && uo_temp >= TUO_REF) { count_self = count_self + 1; submode = "S"; }
}//КОНЕЦ РАБОТЫ СЧЕТЧИКА
// #######
// ОТБОР ГОЛОВ. ПОКА СЧЕТКИК НЕ ДОЙДЕТ ДО ЗАДАННОГО ВРЕМЕНИ
if (mode == 3 && uo_temp > TUO_REF && count_self >= (self_time * 60) && count_head <= (head_time * 60)) {
    submode = "H";            // ИНДИКАЦИЯ "ОТБОР ГОЛОВ"
    digitalWrite(KL2_PIN, 0); // Закрываем клапан отбора тела(если вдруг вернулись добрать головы в середине цикла работы клапана 2) 
static uint32_t tmr_head;
if (millis() - tmr_head >= 1000) { tmr_head = millis();
    count_head = count_head + 1;   // Наращиваем по таймеру счетчик
    tflag = 0; }                   // Сбрасываем флаг задания эталонной температуры      
static uint32_t tmr_kl1_head; 
if (millis() - tmr_kl1_head >= KL1_PER) { // РАБОТА КЛАПАНА ОТБОРА
    tmr_kl1_head = millis();
    digitalWrite(KL1_PIN, 1);      // Открыли клапан 1
    kl1_state = ">"; }              // Индикация работы клапана
if (millis() - tmr_kl1_head >= ((KL1_PER / 100) * k1time)) {  // Закрыли клапан 1 по времени kl1time в % от периода
    digitalWrite(KL1_PIN, 0);
    kl1_state = "-"; }
}// КОНЕЦ РЕЖИМА ОТБОРА ГОЛОВ 
// #######
// ОТБОР "ТЕЛА" ПРОДУКТА
// Фиксируем температуру на момент окончания отбора голов в перемнной uo_temp_fix
if (mode == 3 && uo_temp > TUO_REF && count_self >= (self_time * 60) && count_head > (head_time * 60) && !tflag) {
   uo_temp_fix = uo_temp;          // Забрали температуру как эталон
   tflag = 1;                      // Флаг выставляем в 1, чтобы больше сюда уже не попадать(за исключением случая с возвратом к отбору голов).
   digitalWrite(KL1_PIN, 0);       // Перекрываем клапан отбора голов принудительно, не зависимо от таймеров.  
   kl1_state = "-"; }
// Дальше работаем уже после фиксации температуры
if (mode == 3 && uo_temp > TUO_REF && count_self >= (self_time * 60) && count_head > (head_time * 60) && tflag) {
   submode = "B"; // ИНДИКАЦИЯ "ОТБОР ТЕЛА"
static uint32_t tmr_body; // Счетчик времени отбора, для дисплея 
if (millis() - tmr_body >= 1000) { tmr_body = millis(); count_body = count_body + 1; }
static uint32_t tmr_kl2_body; // РАБОТА КЛАПАНА ОТБОРА
if ((millis() - tmr_kl2_body >= KL2_PER) && (uo_temp < (uo_temp_fix + delt))) {
    tmr_kl2_body = millis();
    digitalWrite(KL2_PIN, 1); // Открыли клапан
    kl2_state = ">";
    xflag = 0; } // Сбрасываем флаг завышения если температура пришла в норму после завышения
if (millis() - tmr_kl2_body >= ((KL2_PER / 100) * k2time)) {  // Закрыли клапан 2 по времени k2time в % от периода
    digitalWrite(KL2_PIN, 0);
    kl2_state = "-"; }
// если темпертара зашла выше uo_temp_fix + delt, убавляем скорость отбора, ставим флаг завышения, увеличиваем счетчик завышений.
if ((uo_temp >= (uo_temp_fix + delt)) && !xflag) {
  k2time = k2time - decr;
  xflag = 1;
  xflag_count = xflag_count + 1; }
// нормальная остановка по исчерпанию окна открытия клапана отбора (несколько завышений температуры в зависимости от декремента)
if (k2time < 1) { stop_norm(); }
// КОНЕЦ РАБОТЫ КЛАПАНА ОТБОРА
} // КОНЕЦ РАБОТЫ ПО ОТБОРУ "ТЕЛА"
// #######
// УПРАВЛЕНИЕ ПОМПОЙ НА РАЗНЫХ РЕЖИМАХ
static uint32_t tmr_pump;
if (millis() - tmr_pump >= 500) { tmr_pump = millis();
// POTSTILLs & MANUAL
if ((mode == 1 || mode == 2 || mode == 4) && cube_temp >= P_START_C) { digitalWrite(PUMP_PIN, 1); pump_state = " ON"; }
if ((mode == 1 || mode == 2 || mode == 4) && cube_temp < P_START_C) { digitalWrite(PUMP_PIN, 0); pump_state = "OFF"; }
// RECTIFICATION
if (mode == 3 && uo_temp >= P_START_UO) { digitalWrite(PUMP_PIN, 1); pump_state = " ON"; }
if (mode == 3 && uo_temp < P_START_UO) { digitalWrite(PUMP_PIN, 0); pump_state = "OFF"; }
}// КОНЕЦ УПРАВЛЕНИЯ ПОМПОЙ
// #######
// УПРАВЛЕНИЕ КУЛЕРАМИ
static uint32_t tmr_air;
if (millis() - tmr_air >= 450) { tmr_air = millis();
if (defl_temp > 1) {
  pwm_pow_map = map(defl_temp, 10, 45, 0, 255);
  analogWrite(PWM_PIN, pwm_pow_map);
  pwm_pow = map(pwm_pow_map, 0, 255, 0, 100); }
} // КОНЕЦ УПРАВЛЕНИЯ КУЛЕРАМИ
// #######
// ВЫВОД НА ДИСПЛЕЙ ИНФОРМАЦИИ ПО ТАЙМЕРУ РАЗ В 300мс. ОСНОВНОЙ ЭКРАН
static uint32_t tmr;
if (millis() - tmr >= 300 && !in_menu) { tmr = millis();
// Вывод температуры куба
lcd.setCursor(0,0); lcd.print("Tk:");
lcd.setCursor(3,0); lcd.print(cube_temp);
// Вывод температуры воды в контуре охлаждения
lcd.setCursor(0,1); lcd.print("Td:");
lcd.setCursor(3,1); lcd.print(defl_temp);
// Вывод температуры узла отбора
lcd.setCursor(0,2); lcd.print("To:");
lcd.setCursor(3,2); lcd.print(uo_temp);
// Вывод зафиксированной температуры узла отбора при ректификации по каждому режиму ректификации
lcd.setCursor(0,3); lcd.print("Tf:");
lcd.setCursor(3,3); lcd.print(uo_temp_fix);
// Вывод режима работы
lcd.setCursor(16,3); lcd.print(mode_desc); 
// Вывод мощности нагрева (%)
lcd.setCursor(8,0); lcd.print(" H:");
lcd.setCursor(11,0); lcd.print("   ");
lcd.setCursor(11,0); lcd.print(ten_pow);
// Вывод счетчика на режиме, или мощности ШИМ кулеров для POTSTILL/MANUAL
if (mode == 1 || mode == 2 || mode == 4) {
lcd.setCursor(8,1); lcd.print(" A:");
lcd.setCursor(11,1); lcd.print("   ");
lcd.setCursor(11,1); lcd.print(pwm_pow); }
if (mode ==3 && submode == "R") {
lcd.setCursor(8,1); lcd.print(" R:");
lcd.setCursor(11,1); lcd.print("---"); }
if (mode ==3 && submode == "S") {
lcd.setCursor(8,1); lcd.print(" S:");
lcd.setCursor(11,1); lcd.print("   ");
lcd.setCursor(11,1); cnt_self = count_self / 60; lcd.print(cnt_self); }
if (mode ==3 && submode == "H") {
lcd.setCursor(8,1); lcd.print(" H:");
lcd.setCursor(11,1); lcd.print("   ");
lcd.setCursor(11,1); cnt_head = count_head / 60; lcd.print(cnt_head); }
if (mode ==3 && submode == "B") {
lcd.setCursor(8,1); lcd.print(" B:");
lcd.setCursor(11,1); lcd.print("   ");
lcd.setCursor(11,1); cnt_body = count_body / 60; lcd.print(cnt_body); }
// Вывод статуса помпы
lcd.setCursor(8,2); lcd.print(" P:");
lcd.setCursor(11,2); lcd.print(pump_state);
// Вывод состояния клапанов
lcd.setCursor(15,0); lcd.print(" K1:");
lcd.setCursor(19,0); lcd.print(kl1_state);
lcd.setCursor(15,1); lcd.print(" K2:");
lcd.setCursor(19,1); lcd.print(kl2_state);
// Счетчик завышений по температуре узла отбора
lcd.setCursor(14,2); lcd.print("  T^:");
lcd.setCursor(19,2); lcd.print(xflag_count);
// Время отбора голов в минутах
lcd.setCursor(8,3); lcd.print(" Ht:");
lcd.setCursor(12,3); lcd.print("   ");
lcd.setCursor(12,3); lcd.print(head_time);
// Подрежим
lcd.setCursor(19,3); lcd.print(submode);
}// КОНЕЦ ВЫВОДА НА ДИСПЛЕЙ ПО ТАЙМЕРУ
// #######
// ВЫВОД НА ДИСПЛЕЙ МЕНЮ УСТАНОВОК
static uint32_t tmr_menu;
if (millis() - tmr_menu >= 300 && in_menu){ tmr_menu = millis();
if (ptr < 4) {
mprint(0);
pprint(ptr);
lcd.setCursor(15,0); lcd.print("    m");
lcd.setCursor(15,0); lcd.print(self_time);
lcd.setCursor(15,1); lcd.print("    m");
lcd.setCursor(15,1); lcd.print(head_time);
lcd.setCursor(15,2); lcd.print("    %");
lcd.setCursor(15,2); lcd.print(k1time);
lcd.setCursor(15,3); lcd.print("    %");
lcd.setCursor(15,3); lcd.print(k2time);
}
if (ptr > 3 && ptr < 8) {
mprint(1);
pprint(ptr - 4);
lcd.setCursor(15,0); lcd.print("    ");
lcd.setCursor(19,0); lcd.write(223);
lcd.setCursor(15,0); lcd.print(delt);
lcd.setCursor(15,1); lcd.print("    %");
lcd.setCursor(15,1); lcd.print(decr);
lcd.setCursor(15,2); lcd.print("    %");
lcd.setCursor(15,2); lcd.print(repwrst);
lcd.setCursor(15,3); lcd.print("    %");
lcd.setCursor(15,3); lcd.print(repwrnd);
}
if (ptr > 7 && ptr < 12) {
mprint(2);
pprint(ptr - 8);
lcd.setCursor(15,0); lcd.print("    %");
lcd.setCursor(15,0); lcd.print(p1pwrst);
lcd.setCursor(15,1); lcd.print("    %");
lcd.setCursor(15,1); lcd.print(p1pwrnd);
lcd.setCursor(15,2); lcd.print("    %");
lcd.setCursor(15,2); lcd.print(p2pwrst);
lcd.setCursor(15,3); lcd.print("    %");
lcd.setCursor(15,3); lcd.print(p2pwrnd);
}
if (ptr > 11 && ptr < 16) {
mprint(3);
pprint(ptr - 12);
lcd.setCursor(15,0); lcd.print("     ");
lcd.setCursor(15,0); lcd.print(mode_desc);
lcd.setCursor(15,1); lcd.print("    %");
lcd.setCursor(15,1); lcd.print(mnpwr);
lcd.setCursor(15,2); lcd.print("     ");
lcd.setCursor(15,3); lcd.print("     ");
}
}// КОНЕЦ ВЫВОДА МЕНЮ

}// КОНЕЦ ЦИКЛА LOOP
// #######
// ФУНКЦИИ
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
// ВЫВОД МЕНЮ УСТАНОВОК
void mprint(int mcol) {
  lcd.setCursor(1,0); lcd.print(menu_settings[0][mcol]);
  lcd.setCursor(1,1); lcd.print(menu_settings[1][mcol]);
  lcd.setCursor(1,2); lcd.print(menu_settings[2][mcol]);
  lcd.setCursor(1,3); lcd.print(menu_settings[3][mcol]);
}
// ВЫВОД УКАЗАТЕЛЯ
void pprint(int snum){
if (!is_set) {
  lcd.setCursor(0,0); lcd.print(" ");
  lcd.setCursor(0,1); lcd.print(" ");
  lcd.setCursor(0,2); lcd.print(" ");
  lcd.setCursor(0,3); lcd.print(" ");
  lcd.setCursor(0,snum); lcd.print(">");
}
else {
  lcd.setCursor(0,0); lcd.print(" ");
  lcd.setCursor(0,1); lcd.print(" ");
  lcd.setCursor(0,2); lcd.print(" ");
  lcd.setCursor(0,3); lcd.print(" ");
  lcd.setCursor(0,snum); lcd.print("*");
   }
}
// ЗАПИСЬ ПЕРЕМННЫХ В EEPROM
void eeprom_write() { 
EEPROM.put(0, 0.0);        //если ранее в EEPROM ничегоне писалось (пустой контроллер, будет nan если 2 байта по 255), инициализируем ячейку под float.
EEPROM.put(4, self_time);  // int будут -1 везде, но это поддается заданию из меню. 
EEPROM.put(6, head_time);
EEPROM.put(8, k1time);
EEPROM.put(10, k2time);
EEPROM.put(12, decr);
EEPROM.put(14, repwrst);
EEPROM.put(16, repwrnd);
EEPROM.put(18, p1pwrst);
EEPROM.put(20, p1pwrnd);
EEPROM.put(22, p2pwrst);
EEPROM.put(24, p2pwrnd);
EEPROM.put(26, mnpwr);
EEPROM.put(0, delt);
}
// НОРМАЛЬНЫЙ СТОП
void stop_norm() {
lcd.clear();
while (true) {
    lcd.setCursor(0,3);
    lcd.print("# NORMAL STOP #");
    detachInterrupt(INT_NUM);    // отцепляем прерывание
    digitalWrite(DIMMER_PIN, 0); // выключаем тиристор принудительно
    digitalWrite(PUMP_PIN, 0);   // выключаем помпу
    analogWrite(PWM_PIN, 0);     // выставляем минимум на кулеры
    digitalWrite(KL1_PIN, 0);    // закрываем клапан 1
    digitalWrite(KL2_PIN, 0);    // закрываем клапан 2
    disp_stats();                //показываем статистику
    delay(1000);                 // повторяем бесконечно, раз в секунду
  }
}
// АВАРИЙНЫЙ СТОП ПО ВОДЕ
void stop_defl() {
lcd.clear();
while (true) {
    lcd.setCursor(0,3);
    lcd.print("# ERR COOLING #");
    detachInterrupt(INT_NUM);
    digitalWrite(DIMMER_PIN, 0);  //отцепляем прерывание
    digitalWrite(PUMP_PIN, 0);    //выключаем помпу
    analogWrite(PWM_PIN, 0);      //выставляем минимум на кулеры
    digitalWrite(KL1_PIN, 0);     //закрываем клапан 1
    digitalWrite(KL2_PIN, 0);     //закрываем клапан 2
    disp_stats();                 //показываем статистику
    delay(1000);                  //повторяем бесконечно, раз в секунду
  }
}
// АВАРИЙНЫЙ СТОП ПО КУБУ
void stop_cube() {
lcd.clear();
while (true) {
    lcd.setCursor(0,3);
    lcd.print("# ERR CUBE #");
    detachInterrupt(INT_NUM);
    digitalWrite(DIMMER_PIN, 0);  //отцепляем прерывание
    digitalWrite(PUMP_PIN, 0);    //выключаем помпу
    analogWrite(PWM_PIN, 0);      //выставляем минимум на кулеры
    digitalWrite(KL1_PIN, 0);     //закрываем клапан 1
    digitalWrite(KL2_PIN, 0);     //закрываем клапан 2
    disp_stats();                 //показываем статистику 
    delay(1000);                  //повторяем бесконечно, раз в секунду
  }
}
// ВЫВОД ОТЧЕТА
void disp_stats() {
// Выводим на экран отчет с температурами, временем и т.д.
// Температуры
lcd.setCursor(0,0); lcd.print("Tk:");
lcd.setCursor(3,0); lcd.print(cube_temp);
lcd.setCursor(0,1); lcd.print("Td:");
lcd.setCursor(3,1); lcd.print(defl_temp);
lcd.setCursor(0,2); lcd.print("To:");
lcd.setCursor(3,2); lcd.print(uo_temp);
// Счетчики времени
lcd.setCursor(9,0); lcd.print("Ht:");
lcd.setCursor(12,0); lcd.print(cnt_head);
lcd.setCursor(9,1); lcd.print("Bt:");
lcd.setCursor(12,1); lcd.print(cnt_body);
// Зафиксированная в УО температура
lcd.setCursor(9,2); lcd.print("Tf:");
lcd.setCursor(12,2); lcd.print(uo_temp_fix);
// Режим/подрежим работы
lcd.setCursor(16,3); lcd.print(mode_desc);
lcd.setCursor(19,3); lcd.print(submode);
}