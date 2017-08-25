#include <Wire.h>
#include "RTClib.h"
#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include <EEPROM.h>
#include <DHT.h>
#include <SoftwareSerial.h>

#define DEBUG

//датчик температуры
#define DHTTYPE DHT11   //тип
#define DHTPIN  8       //пин
uint16_t hum, tem;

// пины для сенсоров
#define WaterSensPin11 2 //поплавок в 1 бочке (60см)
#define WaterSensPin12 15 //аналоговый датчик в 1 бочке (120см)
#define WaterSensPin21 7 //поплавок в 2 бочке (30см)
#define SoilSensPin 16 //датчик влажности почвы
#define Rel1 6 // реле 1 - открывание крана
#define Rel2 7 // реле 2 - насос
#define ResetPin=5; // аппаратный сброс для модема

volatile boolean isAlarm = false;
volatile boolean canAlarm = false;
boolean alarmState = false;

uint16_t valMoisture;      // объявляем переменную для расчёта среднего значения влажности почвы
uint16_t timWatering;      // объявляем переменную для хранения времени начала последнего полива
uint16_t valWaterSens;    // значение датчика в 1 бочке
uint16_t WaterVolume = 0;     // объем воды в бочках
uint16_t MoistureTarget = 0; //порог для начала полива

uint16_t CheckPeriodicity = 3600;  //периодичность проверки влажности почвы в секундах
uint16_t WateringDay = 3;           //день полива - среда
uint16_t WateringTime = 60;     // время открывания клапана в секундах
boolean isWatered = false;     // было ли полито сегодня

DateTime CurAlarm;
uint8_t CurHr;
uint8_t CurDay;

boolean Rel_1_On = false;

//---Трубы для према и передачи
const uint64_t pipe00 = 0xE8E8F0F0A2LL; // приемная труба  
const uint64_t pipe01 = 0xE8E8F0F0E1LL; // передающая труба

/*const uint64_t pipe02 = 0xE8E8F0F0D1LL;
const uint64_t pipe03 = 0xE8E8F0F0C3LL;
const uint64_t pipe04 = 0xE8E8F0F0E7LL;
*/
//uint64_t pipes[4] = {0xE8E8F0F0E1LL,0xE8E8F0F0A2LL,0xE8E8F0F0B3LL,0xE8E8F0F0C4LL};

//------------------
//---NRF посылка---
struct dataStruct{
  DateTime timestamp;
  byte addr;
  byte dir;
  byte message;
  float value;
}myData;

//------------------
//---тип команды (dir)
#define Set 1
#define Get 2
#define Inf 3
//---------------------
//---команда (message)
#define mAll 0
#define mTime 1
#define mTemp 2
#define mHumidity 3
#define mWateringDay 4
#define mMoistureTarget 5
#define mCheckPeriodicity 6
#define mMoistureCurr 7
#define mWaterLevel 8
#define mWateringTime 9
//--------------------------------
char messTxt[8][10] = {
"Time","Temp","Hum","WDay","MTarg","CheckPer","MCurr","WaterLev"
};
 int pos;
 
//---Clock-----------------------
//RTC_DS1307 clock;
RTC_DS3231 clock;
DateTime dt;
//------------------------------
//---NRF------------------------
RF24 radio(9, 10); // пины для NRF
DHT dht(DHTPIN, DHTTYPE); // пины для термодатчика 
//---------------------------

//все для модема


SoftwareSerial Serial1(3, 4); // TX, RX GSM модема

//разбор команды от NRF
/*
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
*/

void ParseCommand2(boolean sms)
{
 switch (myData.dir) 
 {
    case Set:
    {
      switch (myData.message)
      {
        case mTime:
        {
          clock.adjust(myData.timestamp);
          break;
        }     
        case mWateringDay:
        {
          WateringDay = myData.value;
          break;
        }
        case mMoistureTarget:
        {     
          MoistureTarget = myData.value;
          break;
        }
        case mCheckPeriodicity:
        {
          CheckPeriodicity = myData.value;
          break;
        }
        case mWateringTime:
        {
          WateringTime = myData.value;
          break;
        }
      }
      setToMemory();
      break;
    }
    case Get:
    {
      switch (myData.message)
      {
        case mAll:
        {
          getSensors();
          AnswerToStream("temp: "+String(tem)+" hum: "+String(hum) + " Pochva: " + String(valMoisture) + " WaterLev: " + String(WaterVolume),sms);
          break;
        }
        case mTime:
        {
          myData.value = 1;
          AnswerToStream(DTimeToString(&clock.now()),sms);
          break;
        }
        case mTemp:
        {
          myData.value = tem;
          break;
        }
        case mHumidity:
        {
          myData.value = hum;
          break;
        }
        case mWateringDay:
        {
          myData.value = WateringDay;
          break;      
        }
        case mMoistureTarget:
        {
          myData.value = MoistureTarget;
          break;      
        }     
        case mCheckPeriodicity:
        {
          myData.value = CheckPeriodicity;
          break;      
        }       
        case mWateringTime:
        {
          myData.value = WateringTime;
          break;      
        }
        case mMoistureCurr:
        {
          myData.value = valMoisture;
          break;      
        }       
        case mWaterLevel:
        {
          myData.value = WaterVolume;
          break;      
        }
      }
      if ((myData.message!=mAll) && (myData.message != mTime))
      {
        AnswerToStream(String(myData.value),sms);
      }
      sendData(Inf);
      break;
    }
 }
}

String DTimeToString(DateTime* Dtime)
{
   return(String(Dtime->year()-2000) + "/" +  String(Dtime->month()) + "/" + String(Dtime->day()) + ","+String(Dtime->hour())+":" + String(Dtime->minute()) + ":"+String(Dtime->second()));
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

void sendData(byte direct)
{
    Serial.println(F("Now sending"));
    //Serial.println("direct: "+String(direct)+", mess: "+String(mess)+", val: "+String(val));
    dt = clock.now();
    myData.timestamp = dt;
    myData.dir = direct;
    Serial.println(String(messTxt[myData.message-1]) + ": " + String(myData.value));
    radio.stopListening();  // First, stop listening so we can talk.
    radio.flush_tx();
    if (!radio.write( &myData, sizeof(myData) )){
       Serial.println(F("failed"));
     }
    radio.startListening();  
    Serial.flush();
}

void getSensors() 
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
      WaterVolume = 30;
    }
//опрос датчика в 1 бочке (60см)
    if (!digitalRead(WaterSensPin11))
    { // Если на входе уровень логического «0», то ...
      WaterVolume = 60;
    }
//опрос верхнего датчика в 1 бочке (120см)
    valWaterSens = analogRead(WaterSensPin12);
    if (valWaterSens>10) {WaterVolume = 120;}
}

boolean CheckNetwork()
{
     String currStr = "";
    
    Serial1.print("AT+CREG?\r\n");
    delay(1000);

  while (Serial1.available()>0)
  {
    currStr= currStr + Serial1.read();
  }
   /* result = (currStr.lastIndexOf('1') > 1);
    return result;     
    */ 
    return (currStr.lastIndexOf('1') > 1);
        
}

void GetNetworkTime()
{
  String currStr = "";
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
{
 // Serial.println(DateTime((String("20" + NetDate.substring(1,3))).toInt() ,(NetDate.substring(4,6)).toInt(), (NetDate.substring(7)).toInt(),(NetTime.substring(1,3)).toInt(),(NetTime.substring(4,6)).toInt(),(NetTime.substring(7,9)).toInt())); //1307
  Serial.println(String(String("20" + NetDate.substring(0,2))).toInt());
  Serial.println(String((NetDate.substring(3,5)).toInt()));
  Serial.println(String((NetDate.substring(6)).toInt()));
  Serial.println(String((NetTime.substring(0,2)).toInt()));
  Serial.println(String((NetTime.substring(3,5)).toInt()));
  Serial.println(String((NetTime.substring(6,8)).toInt()));
}
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
    Serial1.println("AT+CMGDA=DEL ALL\r"); // команда удалит все сообщения
}


//----------------------------------------------------------------
void AnswerToStream(String text,boolean sms) 
{ 
    if (sms)
    {
      String Number = "+79119328708";
      Serial1.println("AT+CMGS=\"" + Number + "\"");//Number - номер телефона в формате 79995551122, без знака +
      delay(1000);
      Serial1.print(text);// text - там хранится сообщение, которое вы передали (smsSendAlarm("Текст, который надо отправить");)
      text = "";// очищаем буфер
      delay(300);
      Serial1.print((char)26);
      delay(1500);
      Serial1.println("AT+CMGDA=DEL ALL\r"); // команда удалит все сообщения
      Serial.println("answer to modem");
    }
    Serial.println(text);
    Serial.println("answer to stream");
    Serial.flush();
}


//---------------------------------------------------------------
void ParseTextMessage (String* text) 
{ // разбор сообщения от модема
//  Serial.println(text);
    if (text->startsWith("H SET TEMP")) {
        /*targetTemp = (text.substring(11)).toFloat();
        Serial.println("Set temp for house sensor:" + text.substring(11));*/
    } 
    else if (text->startsWith("H STOP")) {
        /*targetTemp = 0;
        digitalWrite(Rel1, LOW);
        Serial.println("Stop house");*/
    }
    else if (text->startsWith("H GET")) {
        getSensors();
        Serial.println("temp: "+String(tem)+" hum: "+String(hum) + " Pochva: " + String(valMoisture));
        AnswerToStream("temp: "+String(tem)+" hum: "+String(hum) + " Pochva: " + String(valMoisture) + " WaterLev: " + String(WaterVolume),true);
       
    }
    else if (text->startsWith("G GET")) {
        Serial.println("GreenHouse Get");
        getSensors();
        Serial.println("temp: "+String(tem)+" hum: "+String(hum) + " Pochva: " + String(valMoisture) + " WaterLev: " + String(WaterVolume) );
        AnswerToStream("temp: "+String(tem)+" hum: "+String(hum) + " Pochva: " + String(valMoisture) + " WaterLev: " + String(WaterVolume),true);
    }
    else if (text->startsWith("G OPEN")) {
         dt = clock.now();
        Serial.println(DTimeToString(&dt));
        digitalWrite(Rel1, HIGH);  // открываем реле краник
        delay(text->substring(7).toInt()*1000);
        digitalWrite(Rel1, LOW);  // закрываем реле краник
        Serial.println("Open Time: " + text->substring(7));
        dt = clock.now();
        Serial.println(DTimeToString(&dt));
    }
    else if (text->startsWith("G CheckTime")) {
        EEPROM.put(0, text->substring(12).toInt());
    }
    else
    {
      
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

//------------------------------------------------------------------

void ParseMessage(String* Mess)
{
  String message_data[5]; //timestamp,address,direction,message,value
  for (int i=0;i<5;i++)
  {
    pos = Mess->indexOf(',');
    message_data[i]=Mess->substring(0,pos);
    *Mess = Mess->substring(pos+1); 
  }
  if((message_data[0].substring(0,2)).toInt()>0)
  {
    myData.timestamp = DateTime((String("20" + message_data[0].substring(0,2))).toInt(),(message_data[0].substring(2,4)).toInt(), (message_data[0].substring(4,6)).toInt(),
                              (message_data[0].substring(6,8)).toInt(),(message_data[0].substring(8,10)).toInt(),(message_data[0].substring(10,12)).toInt());
  
  }
  if (!message_data[0].startsWith("G") && !message_data[0].startsWith("H"))
  {
    myData.addr = byte(message_data[1].toInt());
    myData.dir = byte(message_data[2].toInt());
    myData.message = byte(message_data[3].toInt());
    myData.value = message_data[4].toFloat();
  }
  else
  {
    ParseTextMessage(&message_data[0]);
  }
}

 //------
void ParseHeader(String* Mess)
{
  String header_data[5];  // "+CMT:","+79119328708","","17/07/28","00:45:22+12"
  Mess->replace(" ",","); 
  Mess->replace('"','_');
  Mess->replace("_","");
  for (int i;i<5;i++)
  {
    pos = Mess->indexOf(',');
    header_data[i]=Mess->substring(0,pos);
    *Mess = Mess->substring(pos+1); 
  //  Serial.println(header_data[i]);
  }
 
  myData.timestamp = DateTime((String("20" + header_data[3].substring(0,2))).toInt(),(header_data[3].substring(3,5)).toInt(), (header_data[3].substring(6)).toInt(),
                              (header_data[4].substring(0,2)).toInt(),(header_data[4].substring(3,5)).toInt(),(header_data[4].substring(6,8)).toInt());

}
//----------------------

void ReadFromSerial()
{
  String currStr = "";
  char currSymb = "";
  while (Serial.available())
  {
    currStr = Serial.readStringUntil('\r');
    if (currStr.startsWith("+CMT"))
    {
      ParseHeader(&currStr);
    }
    else
    {
      ParseMessage(&currStr);
    }
    currStr = "";
  }
  Serial.flush();
//Serial.println("ReadFromPort2"); 
}

void ReadFromModem()
{
  String currStr = "";
  char currSymb = "";
    while (Serial1.available())
    {
      currSymb = Serial1.read();
      //Serial.print(currSymb);
      if (('\r' == currSymb) & (currStr.length()>1))
      {
        Serial.println("Inside ReadFromPort3");
        currStr.toUpperCase();
        if (currStr.startsWith("+CMT")) 
        {
          ParseHeader(&currStr);  
        }
        else
        {
          ParseMessage(&currStr);
        }
        currStr = "";
      } 
      else if ('\n' != currSymb) 
      {
        currStr += String(currSymb);
      }
    }
    Serial1.println("AT+CMGDA=DEL ALL\r");
}


//---------------------------------------------------------------------
void setup()
{
#ifdef DEBUG
 // setToMemory();
#endif
  getFromMemory();
 
  Serial.begin(57600);
  isAlarm = false;

  clock.begin();
  dt = clock.now();
  
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
  getSensors();
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
     // Serial.println(DTimeToString(dt));
    }

//отправка значений температуры и влажности на базу  
    myData.message=mTemp; myData.value=tem; sendData(Inf);
  delay(5000);
    myData.message=mHumidity; myData.value=hum; sendData(Inf);   
  
  dt = clock.now();
  
 // Serial.println(CheckPeriodicity,DEC);
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
        ParseCommand2(false);
     }

 if (Serial1.available())    {    // Если в буфере модема имеются принятые данные
 //     Serial.println("Serial1");
      ReadFromModem();
      ParseCommand2(true);
    }
    
 if (Serial.available())    {    // Если в буфере UART имеются принятые данные
      //Serial.println("Serial");
      ReadFromSerial();
      ParseCommand2(false);
    }
    
   dt = clock.now();
   CurHr = dt.second();
   CurDay = dt.dayOfTheWeek();
//проверка срабатывания аларма
  if ((dt.day() >= CurAlarm.day())&&(dt.hour() >= CurAlarm.hour())&& (dt.minute() >= CurAlarm.minute()) && (dt.second() >= CurAlarm.second()) ) 
    {isAlarm=true;}
     else {isAlarm=false;}
  if (isAlarm)
  {
    isAlarm = false;
    canAlarm = !canAlarm;
    //PrintCurrTime(&dt);
    Serial.println(DTimeToString(&dt));
    valMoisture = 0;
    
//опрос датчиков    
    getSensors(); 
  
#ifdef DEBUG
{
  Serial.print("Temp: "); Serial.println(tem, DEC);
  Serial.print("Humi: "); Serial.println(hum, DEC);
  Serial.print("Pochva: "); Serial.println(valMoisture, DEC);
  Serial.print("WaterVolume "); Serial.println(WaterVolume, DEC);
}
#endif
//отправка значений датчиков на базу    
    myData.message=mTemp; myData.value=tem; sendData(Inf);
    myData.message=mHumidity; myData.value=hum; sendData(Inf);   
    myData.message=mMoistureCurr; myData.value=valMoisture; sendData(Inf);
    myData.message=mWaterLevel; myData.value=WaterVolume; sendData(Inf);   
       
    if (valMoisture > MoistureTarget)        //если влажность почвы >целевой,
    {
      digitalWrite(Rel1, LOW);  // то закрываем краник
    }
    else                        // иначе
    {
      if (((CurDay == WateringDay) || (CurDay > WateringDay)) && (!isWatered) ) //если сегодня день полива, либо следующий день, но полива не было
        { 
          digitalWrite(Rel1, HIGH); //открываем кран
          delay(WateringTime*1000); //на WateringTime секунд
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

