#include <Wire.h>
#include "RTClib.h"
//#include <avr/sleep.h>
//#include <avr/power.h>
#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include <EEPROM.h>
#include <DHT.h>
#include <SoftwareSerial.h>

#define DEBUG

//датчик температуры
#define DHTTYPE DHT11
#define DHTPIN  8
float hum, tem;

// пины для сенсоров
const byte WaterSensPin11 = 2; //поплавок в 1 бочке (60см)
const byte WaterSensPin12 = 15; //аналоговый датчик в 1 бочке (120см)
const byte WaterSensPin21 = 7; //поплавок в 2 бочке (30см)
const byte SoilSensPin = 16; //датчик влажности почвы
const byte Rel1 = 6; // реле 1 - открывание крана
//const byte Rel2 = 7; // реле 2 - насос

volatile boolean isAlarm = false;
volatile boolean canAlarm = false;
boolean alarmState = false;

uint16_t arrMoisture[10];  // объявляем массив для хранения 10 последних значений влажности почвы
uint32_t valMoisture;      // объявляем переменную для расчёта среднего значения влажности почвы
uint32_t timWatering;      // объявляем переменную для хранения времени начала последнего полива
uint32_t valWaterSens;    // значение датчика в 1 бочке
byte WaterVolume = 0;     // объем воды в бочках
uint32_t MoistureTarget = 0; //порог для начала полива

uint32_t CheckPeriodicity = 3600;  //периодичность проверки влажности почвы в секундах
byte WateringDay = 3;           //день полива - среда
byte WateringTime = 60000;     // время открывания клапана в секундах
boolean isWatered = false;     // было ли полито сегодня

DateTime CurAlarm;
uint8_t CurHr;
uint8_t CurDay;

boolean Rel_1_On = false;

const uint64_t pipe00 = 0xE8E8F0F0E1LL; // приемная труба  
const uint64_t pipe01 = 0xE8E8F0F0A2LL; // передающая труба

struct dataStruct{
  float timestamp;
  byte dir;
  byte message;
  float value;
}myData;

//тип команды (dir)
const byte Set = 1;
const byte Get = 2;
const byte Inf = 3;
//
//команда (message)
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

RTC_DS1307 clock;
DateTime dt;
RF24 radio(9, 10); // пины для NRF
DHT dht(DHTPIN, DHTTYPE); // пины для термодатчика 

//все для модема
String currStr = "";
// Переменная принимает значение True, если текущая строка является сообщением
boolean isStringMessage = false;
boolean NetworkOk=false;
byte ResetPin=5; // аппаратный сброс для модема
SoftwareSerial Serial1(3, 4); // TX, RX GSM модема

//разбор команды от NRF
void ParseCommand(dataStruct command)
{
 switch (command.message) 
 {              
   case mTime:
   {
     switch (command.dir)
     {
     case Set:
        {
          //DateTime adj = DateTime(command.value);
          clock.adjust(command.value);
          //clock.adjust(
          break;
        }
     case Get:
        {
          dt = clock.now();
          sendData(Inf,mTime,dt.unixtime());
          break;
        }
     }   
     break;
     }
   case mTemp:
   {
    sendData(Inf,mTemp,tem);
    break;
    }
   case mHumidity:
   {
    sendData(Inf,mHumidity,hum);
    break;
    }
   case mWateringDay:
   {
     switch (command.dir)
     {
     case Set:
        {
          WateringDay = command.value;
          EEPROM.put(4, WateringDay);
          break;
        }
     case Get:
        {
          sendData(Inf,mWateringDay,WateringDay);
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
          MoistureTarget = command.value;
          EEPROM.put(12, MoistureTarget);
          break;
        }
     case Get:
        {
          sendData(Inf,mMoistureTarget,MoistureTarget);
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
          CheckPeriodicity = command.value;
          EEPROM.put(0, CheckPeriodicity);
          break;
        }
     case Get:
        {
          sendData(Inf,mCheckPeriodicity,CheckPeriodicity);
          break;
        }
     }   
    break;
    }    
  }
  
  
}

void PrintCurrTime(DateTime Dtime)
{
   // Serial.print(Dtime)
    Serial.print(Dtime.day(), DEC); Serial.print("-");
    Serial.print(Dtime.month(), DEC); Serial.print('-');
    Serial.print(Dtime.year(), DEC); Serial.print(' ');
    Serial.print(Dtime.hour(), DEC); Serial.print(':');
    Serial.print(Dtime.minute(), DEC); Serial.print(':');
    Serial.print(Dtime.second(), DEC); Serial.println();
}

void alarmFunction()
{  
  detachInterrupt(0);
  if (canAlarm)
  {
//  Serial.println("*** INT 0 ***");
  isAlarm = true;

  }

}

void sleepNow()         // here we put the arduino to sleep
{
  /*
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);   // sleep mode is set here
    sleep_enable();          // enables the sleep bit in the mcucr register
    attachInterrupt(0, alarmFunction, LOW); // use interrupt 0 (pin 2) and run function
    ADCSRA = 0; // disable ADC
    power_spi_disable();
    power_timer0_disable();
    power_timer1_disable();
    power_timer2_disable();
    power_twi_disable();
    power_adc_disable();
    power_usart0_disable();
    sleep_mode();            // here the device is actually put to sleep!!
                             // THE PROGRAM CONTINUES FROM HERE AFTER WAKING UP
    sleep_disable();         // first thing after waking from sleep:
                             // disable sleep...
  */
  attachInterrupt(0, alarmFunction, LOW); // use interrupt 0 (pin 2) and run function

}

void getFromMemory()
{
  EEPROM.get(0,CheckPeriodicity);
  EEPROM.get(4,WateringDay);
  EEPROM.get(8,WateringTime);
  EEPROM.get(12,MoistureTarget);
}

void setToMemory()
{
  EEPROM.put(0, CheckPeriodicity);
  EEPROM.put(4, WateringDay);
  EEPROM.put(8, WateringTime);
  EEPROM.put(12, MoistureTarget);
}

void sendData(byte direct,byte mess,float val)
{

    Serial.println(F("Now sending"));
    //Serial.println("direct: "+String(direct)+", mess: "+String(mess)+", val: "+String(val));
    Serial.println(String(messTxt[myData.message-1]) + ": " + String(myData.value));
    radio.stopListening();  // First, stop listening so we can talk.
    dt = clock.now();
    myData.timestamp = dt.unixtime();
    myData.dir = direct;
    myData.message = mess;
    myData.value = val;
 //   myData._micros = micros();
    radio.flush_tx();
    if (!radio.write( &myData, sizeof(myData) )){
       Serial.println(F("failed"));
     }
    radio.startListening();  
}

void gettemperature() 
{
//опрос датчика температуры и влажности  
    hum = dht.readHumidity();
    tem = dht.readTemperature(false);
//опрос датчика влажности почвы    
    for (int i = 0; i < 10; i++)
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

  Serial.println(currStr);
  String NetDate = currStr.substring(19,27);
  String NetTime = currStr.substring(28,36);

  clock.adjust(DateTime((String("20" + NetDate.substring(0,2))).toInt(),(NetDate.substring(3,5)).toInt(), (NetDate.substring(6)).toInt(),(NetTime.substring(0,2)).toInt(),(NetTime.substring(3,5)).toInt(),(NetTime.substring(6,8)).toInt())); //1307

#ifdef DEBUG
 // Serial.println(DateTime((String("20" + NetDate.substring(1,3))).toInt() ,(NetDate.substring(4,6)).toInt(), (NetDate.substring(7)).toInt(),(NetTime.substring(1,3)).toInt(),(NetTime.substring(4,6)).toInt(),(NetTime.substring(7,9)).toInt())); //1307
  Serial.println(String(String("20" + NetDate.substring(0,2))).toInt());
  Serial.println(String((NetDate.substring(3,5)).toInt()));
  Serial.println(String((NetDate.substring(6)).toInt()));
  Serial.println(String((NetTime.substring(0,2)).toInt()));
  Serial.println(String((NetTime.substring(3,5)).toInt()));
  Serial.println(String((NetTime.substring(6,8)).toInt()));
#endif
      // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  
  Serial.println(NetDate);
  Serial.println(NetTime);
  
  delay(10000);
}

void readAllSms()
{
  Serial1.print("AT+CMGL=\"ALL\"\r");
  delay(500);
}
//----------------------------------------------------------------
void smsSendAlarm(String text) 
{ 
    String Number = "+79119328708";
    Serial1.println("AT+CMGS=\"" + Number + "\"");//Number - номер телефона в формате 79995551122, без знака +
    delay(1000);
    Serial1.print(text);// text - там хранится сообщение, которое вы передали (smsSendAlarm("Текст, который надо отправить");)
    text = "";// очищаем буфер
    delay(300);
    Serial1.print((char)26);
    delay(1500);
}
//---------------------------------------------------------------
void ParseMessage (String text) 
{ // разбор сообщения от модема
  dataStruct command;
  Serial.println(text);
 /* char* mess;
  text.toCharArray(mess);
  if (sscanf(mess, "%f,%d,%d,%d", &command.timestamp, &command.dir, &command.message,&command.value) == 4) 
  {
    ParseCommand(command); 
  }*/
  
    if (text.startsWith("H Set temp")) {
        //digitalWrite(Rel1, HIGH);
        /*targetTemp = (text.substring(11)).toFloat();
        Serial.println("Set temp for house sensor:" + text.substring(11));*/
    } 
    else if (text.startsWith("H Stop")) {
        /*targetTemp = 0;
        digitalWrite(Rel1, LOW);
        Serial.println("Stop house");*/
    }
    else if (text.startsWith("H Get")) {
        //digitalWrite(greenPin, LOW);
        gettemperature();
//        Serial.println("targetTemp:"+ String(targetTemp)+ " temp: "+String(tem)+" hum: "+String(hum));
        Serial.println("temp: "+String(tem)+" hum: "+String(hum) + " Pochva: " + String(valMoisture));
        smsSendAlarm("temp: "+String(tem)+" hum: "+String(hum) + " Pochva: " + String(valMoisture));
        
    }
    else if (text.startsWith("G Get")) {
        Serial.println("GreenHouse Get");
        gettemperature();
        Serial.println("temp: "+String(tem)+" hum: "+String(hum) + " Pochva: " + String(valMoisture) + " WaterLev: " + String(WaterVolume) );
        smsSendAlarm("temp: "+String(tem)+" hum: "+String(hum) + " Pochva: " + String(valMoisture) + " WaterLev: " + String(WaterVolume));
    }
    else if (text.startsWith("G Open")) {
        digitalWrite(Rel1, HIGH);  // открываем реле краник
        delay(text.substring(7).toInt());
        digitalWrite(Rel1, LOW);  // закрываем реле краник
        Serial.println("Open Time: " + text.substring(7));
    }
    else if (text.startsWith("G CheckTime")) {
        EEPROM.put(0, text.substring(12).toInt());
    }
}
//------------------------------------
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
//---------------------------------------------------------------------
void setup()
{
#ifdef DEBUG
  setToMemory();
#endif
  getFromMemory();
 
  Serial.begin(57600);
  isAlarm = false;

  clock.begin();
  dt = clock.now();
  PrintCurrTime(dt);
  
//подготовка выводов
  pinMode(Rel1, OUTPUT);  //Пин для управления 1 реле
  digitalWrite(Rel1, LOW);
/*  pinMode(Rel2, OUTPUT);  //Пин для управления 2 реле
  digitalWrite(Rel2, LOW);
*/
  pinMode(WaterSensPin11, INPUT_PULLUP); //Пин нижнего поплавка 1 бочки
  pinMode(WaterSensPin12, INPUT);  //Пин верхнего поплавка 1 бочки
  pinMode(WaterSensPin21, INPUT_PULLUP); //Пин нижнего поплавка 2 бочки

  digitalWrite(WaterSensPin11, HIGH); //Пин верхнего поплавка 1 бочки

 //запуск NRF 
 {
  Serial.println("Start NRF");
  radio.begin();
  radio.setPALevel(RF24_PA_MAX);
  radio.setDataRate(RF24_250KBPS);                   // Указываем скорость передачи данных (RF24_250KBPS, RF24_1MBPS, RF24_2MBPS), RF24_1MBPS - 1Мбит/сек
  radio.setAutoAck(false);
  radio.setChannel(10);
  radio.setRetries(15,15);
  radio.openWritingPipe(pipe01);
  radio.openReadingPipe(1,pipe00);
  radio.startListening();
  delay(2000);
 } 
  //запуск датчика температуры
  {
  Serial.println("Start DHT");
  dht.begin();
  gettemperature();
  Serial.println("temp: "+String(tem)+" hum: "+String(hum));
  }
  

//запуск модема
   Serial1.begin(9600);//подключаем порт модема (у меня отлично работает на 9600)
   delay(2000); 

    // Настраиваем приём сообщений с других устройств
    // Между командами даём время на их обработку
    ModemSetup();

    if (!CheckNetwork())
    {
      Serial.println("Reg NotOK, soft Restart");
      Serial1.println("AT+CFUN=1,1\r"); // аппаратный сброс модема
      delay(500);
      ModemSetup();
    }
    else
    {
      Serial.println("Reg OK");
      GetNetworkTime();
      dt = clock.now();
      PrintCurrTime(dt);
    }


/*    
//аппаратный сброс модема
    pinMode(ResetPin, OUTPUT);
    digitalWrite(ResetPin, LOW);
    delay(100);
    digitalWrite(ResetPin, HIGH);
    delay(10000); 
*/    


  
  //clock.adjust(DateTime(F(__DATE__), F(__TIME__))); //1307


  dt = clock.now();
  //CurHr = dt.hour;   //часы
  CurHr = dt.second();
  CurDay = dt.day();

  //CurAlarm = (CurHr + CheckPeriodicity)%24; // аларм в часах кратный 24 часам
  //CurAlarm = (CurHr + CheckPeriodicity) % 60; // аларм в секундах

//отправка значений температуры и влажности на базу  
  sendData(Inf,mTemp,tem);
  delay(5000);
  sendData(Inf,mHumidity,hum);
  
  dt = clock.now();
  
  Serial.println(CheckPeriodicity,DEC);
  CurAlarm = dt + TimeSpan(CheckPeriodicity);
#ifdef DEBUG
{

  Serial.print("CurAlarm: "); 
  Serial.print(CurAlarm.hour(), DEC);
  Serial.print(":");
  Serial.print(CurAlarm.minute(), DEC);
  Serial.print(":");   
  Serial.println(CurAlarm.second(), DEC); 
}
#endif

  
  canAlarm = true;
  
}
//---------------------------------------------------------------------
/*
 * 
 * LOOP
 * 
 * 
 */
void loop()
{
  if(radio.available()){           // Если в буфере NRF имеются принятые данные
        radio.read(&myData, sizeof(myData)); // Читаем данные в массив data и указываем сколько байт читать
        ParseCommand(myData);
     }

 if (Serial1.available())    {    // Если в буфере модема имеются принятые данные
      ReadModemMessage();
    }
    
   dt = clock.now();
   CurHr = dt.second();
   CurDay = dt.dayOfTheWeek();
  //  PrintCurrTime(dt);
//проверка срабатывания аларма
  if ((dt.hour() >= CurAlarm.hour())&& (dt.minute() >= CurAlarm.minute()) && (dt.second() >= CurAlarm.second()) ) 
    {isAlarm=true;}
     else {isAlarm=false;}
  if (isAlarm)
  {
    isAlarm = false;
    canAlarm = !canAlarm;
    PrintCurrTime(dt);
    valMoisture = 0;
    
//опрос датчиков    
    gettemperature(); 
  
#ifdef DEBUG
{
  Serial.print("Temp: "); Serial.println(tem, DEC);
  Serial.print("Humi: "); Serial.println(hum, DEC);
  Serial.print("Pochva: "); Serial.println(valMoisture, DEC);
  Serial.print("WaterVolume "); Serial.println(WaterVolume, DEC);
}
#endif
//отправка значений датчиков на базу    
    sendData(Inf,mTemp,tem);
    sendData(Inf,mHumidity,hum);
    sendData(Inf,mMoistureCurr,valMoisture);
    sendData(Inf,mWaterLevel,WaterVolume);
        
    if (valMoisture > MoistureTarget)        //если влажность почвы >целевой,
    {
      digitalWrite(Rel1, LOW);  // то закрываем краник
    }
    else                        // иначе
    {
      if (((CurDay == WateringDay) || (CurDay > WateringDay)) && (!isWatered) ) //если сегодня день полива, либо следующий день, но полива не было
        { 
          digitalWrite(Rel1, HIGH); //открываем кран
          delay(WateringTime); //на WateringTime секунд
          digitalWrite(Rel1, LOW);  // закрываем кран
          isWatered = true;
        } 
        else if (CurDay<WateringDay) {
          isWatered =false;
          //send alarm tooLowMoisture
          }
    };

     canAlarm = !canAlarm;
     
    Serial.println(CheckPeriodicity,DEC);
    CurAlarm = dt + TimeSpan(CheckPeriodicity); 
#ifdef DEBUG
  {
    Serial.print("CurAlarm: "); 
    Serial.print(CurAlarm.hour(), DEC);
    Serial.print(":");
    Serial.print(CurAlarm.minute(), DEC);
    Serial.print(":");   
    Serial.println(CurAlarm.second(), DEC); 
}
#endif
  
  }
  
   
}

