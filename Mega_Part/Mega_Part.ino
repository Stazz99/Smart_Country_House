

//Sample using LiquidCrystal library
#include <LiquidCrystal.h>
#include <Wire.h>
#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
//#include <DS3231.h>
#include "RTClib.h"
#include <DHT.h>
#include <MenuSystem.h>

#define DEBUG

// Menu variables
MenuSystem ms;
Menu mm("Main Menu");
MenuItem mm_mi1("Level1-Item1(I)");
Menu mu1("Get");
MenuItem mu1_mi1("Time");
MenuItem mu1_mi2("Temp/Hum");
MenuItem mu1_mi3("Greenhouse");
Menu mu2("Set");
MenuItem mu2_mi1("Time");
MenuItem mu2_mi2("TargetTemp");

// select the pins used on the LCD panel
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

// define some values used by the panel and buttons
#define NUM_KEYS 5
#define Key_Pin 0 // LCD key pad analog pin

int lcd_key     = 0;
int old_key     = -1;
int adc_key_in  = 0;

#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5

int adc_key_val[5] ={50,250,450,650,850};
int key=-1;
int oldkey=-1;
int val;
int CursorPos=1;


const uint64_t pipe00 = 0xE8E8F0F0E1LL;
const uint64_t pipe01 = 0xE8E8F0F0A2LL;
uint64_t pipes[4] = {0xE8E8F0F0E1LL,0xE8E8F0F0A2LL,0xE8E8F0F0B3LL,0xE8E8F0F0C4LL};

struct dataStruct{
  float timestamp;
  byte dir;
  char message;
  float value;
}myData;

//команды (dir)

const byte Set = 1;
const byte Get = 2;
const byte Inf = 3;

//
//передаваемые значения (message)

const byte mTime = 1;
const byte mTemp = 2;
const byte mHumidity = 3;
const byte mWateringDay = 4;
const byte mMoistureTarget = 5;
const byte mCheckPeriodicity = 6;
const byte mMoistureCurr = 7;
const byte mWaterLevel = 8;

//
char messTxt[8][10] = {
"Time","Temp","Hum","WDay","MTarg","CheckPer","MCurr","WaterLev"
};
bool radioAvail = false;
RF24 radio(49,53);

// сам символ
//byte tempChar[8] = {0b00110,0b01001,0b01001,0b00110,0b00000,0b00000,0b00000,0b00000};
byte tempChar[8] = {
  0b00110,
  0b01001,
  0b01001,
  0b00110,
  0b00000,
  0b00000,
  0b00000,
  0b00000
};

//датчик температуры
#define DHTTYPE DHT11
#define DHTPIN 10
DHT dht(DHTPIN, DHTTYPE); // пины для термодатчика 
float hum, tem;
float targetTemp = 0; 


//переменные для модема
String currStr = "";
boolean isStringMessage = false; // True, если текущая строка является сообщением
boolean NetworkOk=false;
//
// определяю часы
RTC_DS3231 clock; 
//RTC_DS1307 clock;
DateTime dt;

void on_item1_selected(MenuItem* p_menu_item)
{
  
  lcd.setCursor(0,0);
  lcd.print("Item1 Selected");
  delay(5000); // so we can look the result on the LCD
}

void on_Get_selected(MenuItem* p_menu_item)
{
  
  lcd.setCursor(0,0);
  lcd.print("Get Selected");
  lcd.setCursor(0,1);
  lcd.blink();
  lcd.setCursor(1,1);
  GetNetworkTime();
  lcd.print(ms.get_current_menu()->get_cur_menu_component_num());
 /* Menu const* cp_menu = ms.get_current_menu();
  lcd.print((cp_menu->get_cur_menu_component_num()));
  
  sendData(Get,mTemp,0);  
*/
  delay(5000); // so we can look the result on the LCD
}

void on_Set_selected(MenuItem* p_menu_item)
{
  val=0;
  CursorPos=1;
  lcd.setCursor(0,0);
  lcd.print("Set Selected ");
  lcd.print(ms.get_current_menu()->get_cur_menu_component_num());
//  lcd.setCursor(0,1);
//  lcd.setCursor(1,1);
//  lcd.print("00:00:00");
  lcd.setCursor(1,1);
  lcd.cursor();
  switch (ms.get_current_menu()->get_cur_menu_component_num()))
  {
    case 0:
    {
      lcd.setCursor(1,1);
      lcd.print("00:00:00"); 
      break;
    }
    case 1:
    {
      lcd.print(targetTemp);
      SetValue(11,&targetTemp,-30,30);
      break;
    }
  }
/*
  while (CursorPos>0) {
    switch (read_LCD_buttons())
      {
        case btnUP:
        {
          val++;
          val %= 10;
          lcd.print(val);
          lcd.setCursor(CursorPos,1);          
          Serial.println(val,DEC);
          break;
        }
        case btnDOWN:
        {
          val--;
          val %= 10;
          if (val<0) {val=9;}
          lcd.print(val);
          lcd.setCursor(CursorPos,1);
          Serial.print(val,DEC);
          break;
        }
        case btnRIGHT:
        {
          CursorPos++;
          val=0;
          if (CursorPos>15) {CursorPos=1;}
          lcd.setCursor(CursorPos,1);
          Serial.print(val,DEC);
          break;
        }
        case btnLEFT:
        {
          CursorPos--;
          lcd.setCursor(CursorPos,1);
          Serial.print(val,DEC);
          break;
        }
      }
    }
 */
  
}

void SetValue(int StartCursorPos,float* ParamVal,float ValMin, float ValMax)
{
  CursorPos = StartCursorPos;
  val <-ParamVal;
  
  while (CursorPos>=StartCursorPos) {
    switch (read_LCD_buttons())
      {
        case btnUP:
        {
          val++;
          val %= ValMax+1;
          lcd.print(val);
          lcd.setCursor(CursorPos,1);          
          Serial.println(val,DEC);
          break;
        }
        case btnDOWN:
        {
          val--;
          val %= ValMax+1;
          if (val<ValMin) {val=ValMax;}
          lcd.print(val);
          lcd.setCursor(CursorPos,1);
          Serial.print(val,DEC);
          break;
        }
        case btnRIGHT:
        {
          CursorPos++;
          val=0;
          if (CursorPos>15) {CursorPos=1;}
          lcd.setCursor(CursorPos,1);
          Serial.print(val,DEC);
          break;
        }
        case btnLEFT:
        {
          CursorPos--;
          lcd.setCursor(CursorPos,1);
          Serial.print(val,DEC);
          break;
        }
      }
    }
  
}
//------------------------------
void getFromMemory()
{
  /*
  EEPROM.get(0,CheckPeriodicity);
  EEPROM.get(4,WateringDay);
  EEPROM.get(8,WateringTime);
  EEPROM.get(12,MoistureTarget);
  */
}

void setToMemory()
{
  /*
  EEPROM.put(0, CheckPeriodicity);
  EEPROM.put(4, WateringDay);
  EEPROM.put(8, WateringTime);
  EEPROM.put(12, MoistureTarget);
  */
}

// read the buttons
int read_LCD_buttons_old()
{
 adc_key_in = analogRead(0);      // read the value from the sensor 
 // my buttons when read are centered at these valies: 0, 144, 329, 504, 741
 // we add approx 50 to those values and check to see if we are close
 if (adc_key_in > 1000) return btnNONE; // We make this the 1st option for speed reasons since it will be the most likely result
 // For V1.1 us this threshold
 if (adc_key_in < 50)   return btnRIGHT;  
 if (adc_key_in < 250)  return btnUP; 
 if (adc_key_in < 450)  return btnDOWN; 
 if (adc_key_in < 650)  return btnLEFT; 
 if (adc_key_in < 850)  return btnSELECT;  

 // For V1.0 comment the other threshold and use the one below:
/*
 if (adc_key_in < 50)   return btnRIGHT;  
 if (adc_key_in < 195)  return btnUP; 
 if (adc_key_in < 380)  return btnDOWN; 
 if (adc_key_in < 555)  return btnLEFT; 
 if (adc_key_in < 790)  return btnSELECT;   
*/


 //return btnNONE;  // when all others fail, return this...
}

int read_LCD_buttons()
{
  adc_key_in = analogRead(Key_Pin);      // read the value from the sensor 
  key = get_key(adc_key_in);    // We get the button pressed
  if (key != oldkey)   // if keypress is detected
  {
    delay(10);  // Expected to avoid bouncing pulsations
    adc_key_in = analogRead(Key_Pin);    // Read the value of the pulsation
    key = get_key(adc_key_in);    // We get the button pressed
    oldkey = key;
  if (key == -1) return btnNONE; 
  if (key == 0)  return btnRIGHT;  
  if (key == 1)  return btnUP; 
  if (key == 2)  return btnDOWN; 
  if (key == 3)  return btnLEFT; 
  if (key == 4)  return btnSELECT; 
  } 
  return btnNONE;  // when all others fail, return this...
}

void sendData(byte direct,byte mess,float val)
{
    Serial.println(F("Now sending"));
    Serial.println(String(messTxt[myData.message-1]) + ": " + String(myData.value));
 
    radio.stopListening();  // First, stop listening so we can talk.
    myData.dir = direct;
    myData.message = mess;
    myData.value = val;
//    myData._micros = micros();
    radio.flush_tx();
//    radio.flush_rx();
    if (!radio.write( &myData, sizeof(myData) )){
       Serial.println(F("failed"));
     }
    radio.startListening();  
}

void ParseCommand(dataStruct command)
{
 switch (command.message)               // depending on which button was pushed, we perform an action
 {
   case mTime:
     {
     switch (command.dir)
     {
     case Set:
        {
        //  clock.adjust(command.value);
          break;
        }
     case Get:
        {
        //  dt = clock.now();
        //  sendData(Inf,mTime,dt.unixtime());
          break;
        }
     }   
     break;
     }
    case mTemp:
   {
    //sendData(Inf,mTemp,tem);
    break;
    }
   case mHumidity:
   {
   // sendData(Inf,mHumidity,hum);
    break;
    }
   case mWateringDay:
   {
     switch (command.dir)
     {
     case Set:
        {
   //       WateringDay = command.value;
   //       EEPROM.write(2, WateringDay);
          break;
        }
     case Get:
        {
    //      sendData(Inf,mWateringDay,WateringDay);
          break;
        }
     }   
    break;
    }
   case mMoistureTarget:
   {
     switch (command.dir)
     {
     case Set:
        {
    //      MoistureTarget = command.value;
    //      EEPROM.write(6, MoistureTarget);
          break;
        }
     case Get:
        {
     //     sendData(Inf,mMoistureTarget,MoistureTarget);
          break;
        }
     }   
    break;
    }
   case mCheckPeriodicity:
   {
     switch (command.dir)
     {
     case Set:
        {
    //      CheckPeriodicity = command.value;
    //      EEPROM.write(0, CheckPeriodicity);
          break;
        }
     case Get:
        {
   //       sendData(Inf,mCheckPeriodicity,CheckPeriodicity);
          break;
        }
     }   
    break;
    }    
  }
}

void Balance()
{
    Serial1.print("AT+CUSD=1,");
    Serial1.print('"');
    Serial1.print("#100#");// работает на 100% с МТС
    Serial1.println('"');
}

boolean CheckNetwork()
{
    boolean result,isStringMessage;
    String currStr= "";
    
    Serial1.print("AT+CREG?\r\n");
    delay(1000);

  while (Serial1.available()>0)
  {
    currStr= currStr + Serial1.read();
  }
    result = (currStr.lastIndexOf('1') > 1);
    Serial.println(currStr);
    return result;      
        
}

void GetNetworkTime()
{
   // boolean result,isStringMessage;
    String currStr= "";
    
    Serial1.print("AT+CCLK?\r\n");
    delay(2000);

  while (Serial1.available()>0)
  {
    currStr= currStr + char(Serial1.read());
  }

  String NetDate = currStr.substring(19,27);
  String NetTime = currStr.substring(28,36);
  //clock.adjust(DateTime((String("20" + NetDate.substring(0,2))).toInt(),(NetDate.substring(3,5)).toInt(), (NetDate.substring(6)).toInt(),(NetTime.substring(0,2)).toInt(),(NetTime.substring(3,5)).toInt(),(NetTime.substring(6,8)).toInt())); //1307

#ifdef DEBUG
  Serial.println(NetDate);
  Serial.println(NetTime);
#endif
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(NetDate); // print a simple message
  lcd.setCursor(0,1);
  lcd.print(NetTime); // print a simple message
  delay(10000);
}

void readAllSms()
{
  Serial1.print("AT+CMGL=\"ALL\"\r");
  delay(500);
}

void smsSendAlarm(String text) 
{ // Например, в скетче есть текст, его надо отправить (smsSendAlarm("Текст, который надо отправить");)
    String Number = "+79119328708";
    Serial1.println("AT+CMGS=\"" + Number + "\"");//Number - номер телефона в формате 79995551122, без знака +
    delay(1000);
    Serial1.print(text);// text - там хранится сообщение, которое вы передали (smsSendAlarm("Текст, который надо отправить");)
    text = "";// очищаем буфер
    delay(300);
    Serial1.print((char)26);
    delay(1500);
}

void ParseMessage (String text) 
{ // разбор сообщения от модема
/*  lcd.setCursor(0,0);
  lcd.print(text);
*/
  Serial.println(text);
  
    if (text.startsWith("H Set temp")) {
        //digitalWrite(Rel1, HIGH);
        /*targetTemp = (text.substring(11)).toFloat();
        Serial.println("Set temp for house sensor:" + text.substring(11));*/
        lcd.setCursor(0,0);
        lcd.print(text);
    } 
    else if (text.startsWith("H Stop")) {
        /*targetTemp = 0;
        digitalWrite(Rel1, LOW);
        Serial.println("Stop house");*/
    }
    else if (text.startsWith("H Get")) {
        /*//digitalWrite(greenPin, LOW);
        gettemperature();
        Serial.println("targetTemp:"+ String(targetTemp)+ " temp: "+String(tem)+" hum: "+String(hum));
        smsSendAlarm("temp: "+String(tem)+" hum: "+String(hum));*/
    }
    else if (text.startsWith("G Get")) {
        /*//digitalWrite(greenPin, LOW);
        Serial.println("GreenHouse Get");*/
    }
}
//-----------------------------------
void ReadModemMessage()
{
      char currSymb = Serial1.read();
    //  Serial.print(currSymb);
      if ('\r' == currSymb) 
      {
        Serial.println(currStr);
        if (isStringMessage) 
        {
            //если текущая строка - SMS-сообщение,
            //отреагируем на него соответствующим образом
            ParseMessage(currStr);
            isStringMessage = false;
        } 
        else 
        {
          if (currStr.startsWith("+CMT")) 
          {
                //если текущая строка начинается с "+CMT",
                //то следующая строка является сообщением
            isStringMessage = true;
          }
          else if (currStr.startsWith("+CUSD"))
          {
              //если начинается с "+CUSD", то это баланс
            isStringMessage = true;
           }
        }
        currStr = "";
      } 
      else if ('\n' != currSymb) 
      {
        currStr += String(currSymb);
      }
}

//-------------------------------
void ModemSetup()
{
  Serial1.println("AT+CMGF=1\r"); // устанавливает текстовый режим смс-сообщения    
  delay(1000);
  Serial1.println("AT+IFC=1,1\r"); //устанавливает программный контроль потоком передачи данных
  delay(1000);
  Serial1.print("AT+CPBS=\"SM\"\r\n");//открывает доступ к данным телефонной книги SIM-карты
  delay(1000);
  Serial1.print("AT+CNMI=1,2,2,1,0\r\n");// включает оповещение о новых сообщениях, новые сообщения
  delay(1000);
  Serial1.println("AT+GSMBUSY=1\r"); // запрет всех входящих звонков.
  delay(1000);
  Serial1.println("AT+IPR=9600\r"); //скорость общения модем/ардуина
  delay(1000);
  Serial1.println("AT+CSCS=\"GSM\"\r"); //Кодировка текстового режима
  delay(1000);
  Serial1.println("AT+CMGDA=DEL ALL\r"); // команда удалит все сообщения
  delay(1000);
  Serial1.println("AT+CLTS=1\r"); // после рестарта модем будет читать время сети
  delay(1000);
  Serial1.println("AT+CCLK?\r"); // показать время модема
  delay(500);
}

//------------------------------
void displayMenu() {
  lcd.clear();
  lcd.setCursor(0,0);
  // Display the menu
  Menu const* cp_menu = ms.get_current_menu();

  //lcd.print("Current menu name: ");
  lcd.print(cp_menu->get_name());

  lcd.setCursor(0,1);
  lcd.print(">");
  lcd.setCursor(1,1);
  
  lcd.print(cp_menu->get_selected()->get_name());
}

//----------------------
void keyHandler() {
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  adc_key_in = analogRead(Key_Pin);    // Read the value of the pulsation
  key = get_key(adc_key_in);    // We get the button pressed

  if (key != oldkey)   // if keypress is detected
  {
    delay(50);  // Expected to avoid bouncing pulsations
    adc_key_in = analogRead(Key_Pin);    // Read the value of the pulsation
    key = get_key(adc_key_in);    // We get the button pressed
//    if (key != oldkey) 
    oldkey = key;
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


if (key == 1){    //Up
      ms.prev();
      displayMenu();
      #ifdef DEBUG
      Serial.print("key- ");Serial.println(key);
      #endif
}
if (key == 2){    //Down
      ms.next();
      displayMenu();
#ifdef DEBUG
      Serial.print("key- ");Serial.println(key);
      #endif
}
if (key == 3){    //Left
      ms.back();
      displayMenu();
#ifdef DEBUG
      Serial.print("key- ");Serial.println(key);
      #endif
}
if (key == 4){    //select
      ms.select();
      displayMenu();
#ifdef DEBUG
      Serial.print("key- ");Serial.println(key);
      #endif
}
if (key == 0){    //Right
      //serialPrintHelp();
#ifdef DEBUG
      Serial.print("key- ");Serial.println(key);
      #endif
}
  }
}

//----------------------
// Convert the analog value read in a number of button pressed
int get_key(unsigned int input) {
  int k;
  for (k = 0; k < NUM_KEYS; k++)  {
    if (input < adc_key_val[k])    {
      return k;
    }
  }
  if (k >= NUM_KEYS) k = -1;  // Error in reading.
  return k;
}

//--------------
/*void PrintCurrTime(DateTime Dtime)
{
   // Serial.print(Dtime)
    Serial.print(Dtime.day(), DEC); Serial.print("-");
    Serial.print(Dtime.month(), DEC); Serial.print('-');
    Serial.print(Dtime.year(), DEC); Serial.print(' ');
    Serial.print(Dtime.hour(), DEC); Serial.print(':');
    Serial.print(Dtime.minute(), DEC); Serial.print(':');
    Serial.print(Dtime.second(), DEC); Serial.println(' ');
}
*/
//------------------------
void gettemperature() 
{
//опрос датчика температуры и влажности  
    hum = dht.readHumidity();
    tem = dht.readTemperature(false);
//опрос датчика влажности почвы    
 /*   for (int i = 0; i < 10; i++)
    {
      valMoisture += analogRead(SoilSensPin);
      delay(50);
    }
    valMoisture /= 10; // вычисляем среднее значение влажности почвы

//опрос датчиков в бочках
     WaterVolume = 0;
//опрос датчика во 2 бочке (30см)
    if (!digitalRead(WaterSensPin21))
    { // Если на входе уровень логического «0», то ...
      Serial.println("Sensor 2 Up");// Выводим сообщение о том что сенсор тонет (датчик погрузился в воду)
      WaterVolume = 30;
    }
//опрос датчика в 1 бочке (60см)
    if (!digitalRead(WaterSensPin11))
    { // Если на входе уровень логического «0», то ...
      Serial.println("Sensor 1 up");  
      WaterVolume = 60;
    }
//опрос верхнего датчика в 1 бочке (120см)
    valWaterSens = analogRead(WaterSensPin12);
    if (valWaterSens>10) {WaterVolume = 120;}
    
 Serial.print("WaterVolume "); Serial.println(WaterVolume, DEC);
 */
}

//---------------------------
void setup()
{
#ifdef DEBUG
  setToMemory();
#endif
  getFromMemory();
//запуск часов и вывод текущего времни в порт  
  clock.begin();
  dt = clock.now();
 // PrintCurrTime(dt);
    
  Serial.println("Start DHT");
  dht.begin();
  
  Serial.println("temp: "+String(tem)+" hum: "+String(hum));
  
  
  Serial.begin(57600);
//запуск экрана
  lcd.begin(16, 2);              // set up number of columns and rows
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Setup"); // print a simple message
 // его создание
  lcd.createChar(0, tempChar);
//  lcd.write((byte)0);
/*
//запуск радио
  Serial.println("radio.begin"); 
  if (radio.begin())  // Инициируем работу nRF24L01+
  {                              // Указываем канал приёма данных (от 0 до 127), 5 - значит приём данных осуществляется на частоте 2,405 ГГц (на одном канале может быть только 1 приёмник и до 6 передатчиков)
    radio.setPALevel(RF24_PA_MAX);
    radio.setDataRate(RF24_250KBPS);
    radio.setAutoAck(false);
    radio.setRetries(15,15);
    delay(2000); 
    radio.startListening();
    radio.printDetails();  // Вот эта строка напечатает нам что-то, если все правильно соединили.
    delay(5000);
    radio.stopListening();
    radio.setChannel(10); 
    radio.openWritingPipe(pipe01);
    radio.openReadingPipe(1, pipe00);            // Открываем 0 трубу для приема данных (на ожном канале может быть открыто до 6 разных труб, которые должны отличаться только последним байтом идентификатора)
    radio.startListening();
    radioAvail=true;
    lcd.setCursor(0,1);
    lcd.print(" Radio OK");
    delay(5000);
  }
  
//запуск модема
  {
  Serial1.begin(9600);
  delay(2000);
  Serial1.println("AT+CMGF=1\r"); // устанавливает текстовый режим смс-сообщения    
  delay(1000);
  Serial1.println("AT+IFC=1,1\r"); //устанавливает программный контроль потоком передачи данных
  delay(1000);
  Serial1.print("AT+CPBS=\"SM\"\r\n");//открывает доступ к данным телефонной книги SIM-карты
  delay(1000);
  Serial1.print("AT+CNMI=1,2,2,1,0\r\n");// включает оповещение о новых сообщениях, новые сообщения
  delay(1000);
  Serial1.println("AT+GSMBUSY=1\r"); // запрет всех входящих звонков.
  delay(1000);
  Serial1.println("AT+IPR=9600\r"); //скорость общения модем/ардуина
  delay(1000);
  Serial1.println("AT+CSCS=\"GSM\"\r"); //Кодировка текстового режима
  delay(1000);
  Serial1.println("AT+CMGDA=DEL ALL\r"); // команда удалит все сообщения
  delay(1000);
  Serial1.println("AT+CLTS=1\r"); // после рестарта модем будет читать время сети
  delay(1000);
  Serial1.println("AT+CCLK?\r"); // показать время модема
  delay(500);
  if (Serial1.available())
    {    
      ReadModemMessage();
    }
  delay(500);
  lcd.setCursor(0,1);
  lcd.print("Modem OK");
  delay(4000);
  }
*/
//прорисовка меню
{
  mm.add_item(&mm_mi1, &on_item1_selected);
  mm.add_menu(&mu1);
  mu1.add_item(&mu1_mi1, &on_Get_selected);
  mu1.add_item(&mu1_mi2, &on_Get_selected);
  mu1.add_item(&mu1_mi3, &on_Get_selected);
  mm.add_menu(&mu2);
  mu2.add_item(&mu2_mi1, &on_Set_selected);
  mu2.add_item(&mu2_mi2, &on_Set_selected);
  ms.set_root_menu(&mm);
  
  #ifdef DEBUG
    Serial.println("Menu setted.");
  #endif
 
  displayMenu();
}

}
 
void loop()
{
  lcd.setCursor(12,0);            // move cursor to second line "1" and 9 spaces over
  lcd.print(millis()/1000);      // display seconds elapsed since power-up
// lcd.write((byte)0);
  keyHandler();
/*  lcd.setCursor(0,1);            // move to the begining of the second line
  lcd_key = read_LCD_buttons();  // read the buttons

 switch (lcd_key)               // depending on which button was pushed, we perform an action
 {
   case btnRIGHT:
     {
     lcd.clear();
     lcd.print("RIGHT ");
     sendData(Get,mTemp,0);
     break;
     }
   case btnLEFT:
     {
     lcd.print("LEFT   ");
     sendData(Set,mTime,0);//20170702121212
     break;
     }
   case btnUP:
     {
     lcd.print("UP    ");
     break;
     }
   case btnDOWN:
     {
     lcd.print("DOWN  ");
     break;
     }
   case btnSELECT:
     {
     lcd.print("SELECT");
     break;
     }
     case btnNONE:
     {
  //   lcd.print("NONE  ");
     break;
     }
 //}
  }
*/
 /* 
 //проверка что пришло по NRF
  if (radioAvail) {
    if(radio.available())    {                                // Если в буфере имеются принятые данные
      radio.read(&myData, sizeof(myData));                  // Читаем данные в массив data и указываем сколько байт читать
      lcd.setCursor(0,1); 
      lcd.print(String(messTxt[myData.message-1]) + ": " + String(myData.value));
      //lcd.print(myData.value);       // Выводим показания Trema слайдера на индикатор
        
      Serial.println(String(messTxt[myData.message-1]) + ": " + String(myData.value));
      //delay(1000);
      }
      
  }
  
//проверка модема
  if (Serial1.available()) {    
      ReadModemMessage();
    }
    */
}
