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
	Serial.println(str);                // ��������� ����� � ������� �����
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
{                   // ������� �������� ������ � �������� ����������� ����������
	String _resp = "";                     // ���������� ��� �������� ����������
	long _timeout = millis() + responseTimeout; // ���������� ��� ������������ �������� (10 ������)
	while (!Sim.available() && millis() < _timeout)
	{
	}; // ���� ������ 10 ������, ���� ������ ����� ��� �������� �������, ��...
	if (Sim.available())
	{                     // ���� ����, ��� ���������...
		_resp = Sim.readString();               // ... ��������� � ����������
	}
	else
	{                                        // ���� ������ �������, ��...
		Serial.println("Timeout...");              // ... ��������� �� ���� �...
	}
	return _resp;              // ... ���������� ���������. �����, ���� ��������
}

String sendATCommand(String cmd, bool waiting)
{
	String _resp = "";                     // ���������� ��� �������� ����������
	Debug(cmd);
	Sim.println(cmd);                          // ���������� ������� ������
	if (waiting)
	{                                // ���� ���������� ��������� ������...
		_resp = waitResponse();           // ... ����, ����� ����� ������� �����
		// ���� Echo Mode �������� (ATE0), �� ��� 3 ������ ����� ����������������
		if (_resp.startsWith(cmd))
		{                // ������� �� ������ ������������� �������
			_resp = _resp.substring(_resp.indexOf("\r", cmd.length()) + 2);
		}
		Debug(_resp);

	}
	return _resp;                  // ���������� ���������. �����, ���� ��������
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
	{  //������ �� ������
					"AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"", //��������� �������� �����������
					"AT+SAPBR=3,1,\"APN\",\"internet." + operatorGSM + ".ru\"",
					"AT+SAPBR=3,1,\"USER\",\"" + operatorGSM + "\"",
					"AT+SAPBR=3,1,\"PWD\",\"" + operatorGSM + "\"",
					"AT+SAPBR=1,1", //������������� GPRS ����������
					"AT+HTTPINIT",  //������������� http �������
					"AT+HTTPPARA=\"CID\",1" //��������� CID ��������� ��� http ������
			};
	int ATsDelays[] =
	{ 6, 1, 1, 1, 5, 5, 1 }; //������ ��������
	Serial.println("GPRS init start");
	for (int i = 0; i < ATsCount; i++)
	{
		Serial.println(ATs[i]);  //�������� � ������� �����
		//GSMport.println(ATs[i]);  //�������� � GSM ������
		String resp = sendATCommand(ATs[i], true);
		//delay(d * ATsDelays[i]);
		//Serial.println(ReadGSM());  //���������� ����� �� GSM ������
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

//1. ����� � ������� 7 �������
//2. ��������� ������ ����������� � ����� �� ���������
//3. ��������� ��

