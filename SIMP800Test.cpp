#include <Arduino.h>
#include <SoftwareSerial.h>

//#include <SIM800.h>
#define PIN_IND_OPEN 10
#define PIN_IND_CLOSE 11
#define PIN_RELAY  4
#define PIN_RING PIN_A0

#define DEBUG
String whiteList = "+79130744802";

SoftwareSerial Sim(9, 8);
//BareBoneSim800 SIM;

bool isRing;
bool isOpen;

String modulPhoneNumber = "79236181237";
//String modulPhoneNumber = "79130744802";
String serverIP = "128.74.235.43";
String serverPort = "80";
String operatorGSM = "megafin";
String incomingNumber = "---";
String response = "";

long openMoment = 0;
int openDelay = 10000;

int responseTimeout = 15000;

void Debug(String str)
{
#ifdef DEBUG
	Serial.println(str);                // Дублируем ответ в монитор порта
#endif
}

String extractNumber(String rawString)
{
	int startIndex = rawString.indexOf("+79");
	if (startIndex > -1)
	{
		return rawString.substring(startIndex, startIndex + 12);
	}
	else
		return "";
}

String waitResponse()
{                   // Функция ожидания ответа и возврата полученного результата
	String _resp = "";                     // Переменная для хранения результата
	long _timeout = millis() + responseTimeout; // Переменная для отслеживания таймаута (10 секунд)
	while (!Sim.available() && millis() < _timeout)
	{
	}; // Ждем ответа 10 секунд, если пришел ответ или наступил таймаут, то...
	if (Sim.available())
	{                     // Если есть, что считывать...
		_resp = Sim.readString();               // ... считываем и запоминаем
	}
	else
	{                                        // Если пришел таймаут, то...
		Serial.println("Timeout...");              // ... оповещаем об этом и...
	}
	return _resp;              // ... возвращаем результат. Пусто, если проблема
}

String sendATCommand(String cmd, bool waiting)
{
	String _resp = "";                     // Переменная для хранения результата
	Debug(cmd);
	Sim.println(cmd);                          // Отправляем команду модулю
	if (waiting)
	{                                // Если необходимо дождаться ответа...
		_resp = waitResponse();           // ... ждем, когда будет передан ответ
		// Если Echo Mode выключен (ATE0), то эти 3 строки можно закомментировать
		if (_resp.startsWith(cmd))
		{                // Убираем из ответа дублирующуюся команду
			_resp = _resp.substring(_resp.indexOf("\r", cmd.length()) + 2);
		}
		Debug(_resp);

	}
	return _resp;                  // Возвращаем результат. Пусто, если проблема
}

void refreshIndicates()
{
	digitalWrite(PIN_IND_OPEN, isOpen ? HIGH : LOW);
	digitalWrite(PIN_IND_CLOSE, isOpen ? LOW : HIGH);
}

void CloseGates()
{
	if (isOpen && millis() - openMoment >= openDelay)
	{

		digitalWrite(PIN_RELAY, HIGH);
		isOpen = false;
		Debug("Close gates");
	}
	refreshIndicates();
}

void OpenGates()
{
	openMoment = millis();
	digitalWrite(PIN_RELAY, LOW);
	isOpen = true;
	Debug("Open gates");
	refreshIndicates();
}

bool checkPhoneOnServer(String phone)
{
	String command = "guests/checkDoor";
	String params = "?guest=" + phone + "&door=" + modulPhoneNumber;
	//https://kem.megafon.ru/
	String resp = sendATCommand(
			"AT+HTTPPARA=\"URL\",\"http://" + serverIP + ":" + serverPort + "/"
					+ command + "\"", true);
	if (resp.indexOf("OK") >= 0)
	{
		resp = sendATCommand("AT+HTTPACTION=0", true);
		if (resp.indexOf("OK") >= 0)
		{
			resp = waitResponse();
		}
		else
			return false;
	}
	else
		return false;
	//String resp = sendATCommand("AT+HTTPPARA=\"URL\",\"http://mysite.ru/?a=" + data + "\"",true);
	Debug(resp);
}

void getGuestInfo(String phone)
{
	String command = "guests/" + phone;
	String params = "";
	//https://kem.megafon.ru/
	//resp = sendATCommand(
	//		"AT+HTTPPARA=\"USERDATA\",\"Authorization: Basic VXNlcjE6cGFzcw=="
	//				+ params + "\"", true);
	// resp = sendATCommand("AT+HTTPPARA=\"REDIR\",\"1\"",true);
	String resp =
			sendATCommand(
					"AT+HTTPPARA=\"USERDATA\",\"Authorization: Basic VXNlcjE6cGFzcw==\"",
					true);
	resp = sendATCommand(
			"AT+HTTPPARA=\"URL\",\"http://" + serverIP + ":" + serverPort + "/"
					+ command + "\"", true);
	//Authorization: Basic VXNlcjE6cGFzcw==
	if (resp.indexOf("OK") >= 0)
	{
		resp = sendATCommand("AT+HTTPACTION=0", true);
		if (resp.indexOf("OK") >= 0)
		{
			resp = waitResponse();
		}
		resp = sendATCommand("AT+HTTPREAD", true);
		resp = waitResponse();
	}

	//String resp = sendATCommand("AT+HTTPPARA=\"URL\",\"http://mysite.ru/?a=" + data + "\"",true);
	Debug(resp);
}

void initGPRS()
{
	int d = 500;
	int ATsCount = 7;

	String ATs[] =
	{  //массив АТ команд
					"AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"", //Установка настроек подключения
					"AT+SAPBR=3,1,\"APN\",\"internet." + operatorGSM + ".ru\"",
					"AT+SAPBR=3,1,\"USER\",\"" + operatorGSM + "\"",
					"AT+SAPBR=3,1,\"PWD\",\"" + operatorGSM + "\"",
					"AT+SAPBR=1,1", //Устанавливаем GPRS соединение
					"AT+HTTPINIT",  //Инициализация http сервиса
					"AT+HTTPPARA=\"CID\",1" //Установка CID параметра для http сессии
			};
	int ATsDelays[] =
	{ 6, 1, 1, 1, 5, 5, 1 }; //массив задержек
	Serial.println("GPRS init start");
	for (int i = 0; i < ATsCount; i++)
	{
		Serial.println(ATs[i]);  //посылаем в монитор порта
		//GSMport.println(ATs[i]);  //посылаем в GSM модуль
		String resp = sendATCommand(ATs[i], true);
		//delay(d * ATsDelays[i]);
		//Serial.println(ReadGSM());  //показываем ответ от GSM модуля
		Debug(resp);
		//delay(d);
	}
	Serial.println("GPRS init complete");

}

void setup()
{
	pinMode(PIN_IND_CLOSE, OUTPUT);
	pinMode(PIN_IND_OPEN, OUTPUT);
	pinMode(PIN_RELAY, OUTPUT);
	//pinMode(PIN_RING, INPUT_PULLUP);
	digitalWrite(PIN_RELAY, HIGH);
	Sim.begin(9600);

#ifdef DEBUG
	Serial.begin(19200);
	while (!Serial)
	{
		; // wait for serial port to connect. Needed for native USB port only
	}
#endif
	isOpen = false;
	refreshIndicates();
	delay(5000);
	sendATCommand("AT", true);
	initGPRS();
}

void loop()
{
	isRing = analogRead(PIN_RING) < 250;
	//Serial.println(analogRead(PIN_RING));
	if (isRing)
	{
		response = waitResponse();
		incomingNumber = extractNumber(response);
		Debug(response);
		//Serial.println(response);
		if (!incomingNumber.equals(""))
		{
			Debug("Number: " + incomingNumber);

			sendATCommand("ATH", true);
			//bool allowed = checkPhoneOnServer(incomingNumber);
			getGuestInfo(incomingNumber);
			//if (!isOpen && allowed)
			//{
			//	OpenGates();
			//}
		}
	}
	else
	{
		CloseGates();
	}

}

//1. вбить в памятиь 7 номеров
//2. запустить сервак согласовать с Диром по стоимости
//3. привязать СД

