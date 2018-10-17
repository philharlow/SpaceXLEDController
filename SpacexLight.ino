#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <EEPROM.h>

#define PIN D2
#define NUMPIXELS 2

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

/* Set these to your desired softAP credentials. They are not configurable at runtime */
const char *softAP_ssid = "LEDController";
const char *softAP_password = "";

/* hostname for mDNS. Should work at least on windows. Try http://esp8266.local */
const char *myHostname = "esp8266";

// DNS server
const byte DNS_PORT = 53;
DNSServer dnsServer;

// Web server
ESP8266WebServer server(80);

/* Soft AP network parameters */
IPAddress apIP(192, 168, 4, 1);
IPAddress netMsk(255, 255, 255, 0);


#define INITED_VALUE 210 // changing will nuke all saved settings

struct SettingsStruct
{
	int inited;
	int RedMin;
	int RedMax;
	int RedChangeRateMin;
	int RedChangeRateMax;
	int GreenMin;
	int GreenMax;
	int GreenChangeRateMin;
	int GreenChangeRateMax;
	int BlueMin;
	int BlueMax;
	int BlueChangeRateMin;
	int BlueChangeRateMax;
	int BrightnessMin;
	int BrightnessMax;
	int BrightnessChangeRateMin;
	int BrightnessChangeRateMax;
};

SettingsStruct defaultSettings =
{
	INITED_VALUE,
	150, //red
	255,
	100,
	1000,
	0, //green
	40,
	100,
	1000,
	0, //blue
	0,
	0,
	0,
	0, //brightness
	255,
	100,
	1000,
};
SettingsStruct settings = defaultSettings;

float rChangeRate = 0;
float gChangeRate = 0;
float bChangeRate = 0;
float aChangeRate = 0;

bool dirty = false;

void saveSettings()
{
	EEPROM.begin(sizeof(settings));
	EEPROM.put(0, settings);
	EEPROM.end();
	Serial.println("Saved settings!");
	dirty = false;
}

void loadSettings()
{
	SettingsStruct newSettings;
	EEPROM.begin(sizeof(newSettings));
	EEPROM.get(0, newSettings);
	EEPROM.end();

	if (newSettings.inited == INITED_VALUE)
	{
		settings = newSettings;
		Serial.print("Loaded settings");
	}
	else
	{
		Serial.println("Initial settings save");
		saveSettings();
	}
}

void handleParam(String paramName, int& value)
{
	if (server.hasArg(paramName))
	{
		int newVal = server.arg(paramName).toInt();
		if (newVal != value)
		{
			value = newVal;
			dirty = true;
		}
	}
	String html = "<tr><td>";
	html += paramName;
	html += ":</td><td><input type='text' name='" + paramName + "' value='";
	html += value;
	html += "'/></td></tr>";
	server.sendContent(html);
}

/** Is this an IP? */
boolean isIp(String str) {
	for (size_t i = 0; i < str.length(); i++) {
		int c = str.charAt(i);
		if (c != '.' && (c < '0' || c > '9')) {
			return false;
		}
	}
	return true;
}

/** IP to String? */
String toStringIp(IPAddress ip) {
	String res = "";
	for (int i = 0; i < 3; i++) {
		res += String((ip >> (8 * i)) & 0xFF) + ".";
	}
	res += String(((ip >> 8 * 3)) & 0xFF);
	return res;
}

/** Redirect to captive portal if we got a request for another domain. Return true in that case so the page handler do not try to handle the request again. */
boolean captivePortal() {
	if (!isIp(server.hostHeader()) && server.hostHeader() != (String(myHostname) + ".local")) {
		Serial.println("Request redirected to captive portal: " + server.hostHeader());
		server.sendHeader("Location", String("http://") + toStringIp(server.client().localIP()), true);
		server.send(302, "text/plain", "");   // Empty content inhibits Content-length header so we have to close the socket ourselves.
		server.client().stop(); // Stop is needed because we sent no content length
		return true;
	}
	return false;
}

void handleRoot()
{
	Serial.println("handleRoot: " + server.hostHeader());
	if (captivePortal()) { // If caprive portal redirect instead of displaying the page.
		return;
	}
	Serial.println("Sending page");
	server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
	server.sendHeader("Pragma", "no-cache");
	server.sendHeader("Expires", "-1");
	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	server.send(200, "text/html", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
	server.sendContent(
		"<meta name='viewport' content='width=device-width'>"
		"<meta name='viewport' content='initial-scale=1.0'>"
		"<html><head></head><style>td { width: 100%; }</style><body>"
		"<h1>LED Control</h1>"
		"<form method='POST' action='/'>"
	);
	server.sendContent("<table><tr style='background-color:#f26060'><td><table style='width:100%'>");
	handleParam("RedMin", settings.RedMin);
	handleParam("RedMax", settings.RedMax);
	handleParam("RedChangeRateMin", settings.RedChangeRateMin);
	handleParam("RedChangeRateMax", settings.RedChangeRateMax);
	server.sendContent("</table></td></tr><tr style='background-color:#5ff190'><td><table style='width:100%'>");
	handleParam("GreenMin", settings.GreenMin);
	handleParam("GreenMax", settings.GreenMax);
	handleParam("GreenChangeRateMin", settings.GreenChangeRateMin);
	handleParam("GreenChangeRateMax", settings.GreenChangeRateMax);
	server.sendContent("</table></td></tr><tr style='background-color:#5f8df0'><td><table style='width:100%'>");
	handleParam("BlueMin", settings.BlueMin);
	handleParam("BlueMax", settings.BlueMax);
	handleParam("BlueChangeRateMin", settings.BlueChangeRateMin);
	handleParam("BlueChangeRateMax", settings.BlueChangeRateMax);
	server.sendContent("</table></td></tr><tr style='background-color:#cccccc'><td><table style='width:100%'>");
	handleParam("BrightnessMin", settings.BrightnessMin);
	handleParam("BrightnessMax", settings.BrightnessMax);
	handleParam("BrightnessChangeRateMin", settings.BrightnessChangeRateMin);
	handleParam("BrightnessChangeRateMax", settings.BrightnessChangeRateMax);
	server.sendContent("</table></td></tr></table><input type='submit' value='Submit'/></form>");
	server.sendContent("");
	server.client().stop(); // Stop is needed because we sent no content length

	if (dirty)
		saveSettings();
}


void setup()
{
	Serial.begin(115200);
	Serial.println("STARTUP");
	strip.begin();
	strip.show(); // Initialize all pixels to 'off'

	loadSettings();

	WiFi.softAPConfig(apIP, apIP, netMsk);
	WiFi.softAP(softAP_ssid, softAP_password);
	delay(500); // Without delay I've seen the IP address blank
	Serial.print("AP IP address: ");
	Serial.println(WiFi.softAPIP());

	/* Setup the DNS server redirecting all the domains to the apIP */
	dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
	dnsServer.start(DNS_PORT, "*", apIP);

	/* Setup web pages: root, wifi config pages, SO captive portal detectors and not found. */
	server.on("/", handleRoot);
	server.onNotFound(handleRoot);
	server.begin(); // Web server start
	Serial.println("HTTP server started");
}

/*int[] colors = {255, 0, 0,
                255, 50, 0,
                200, 50, 0,
                255, 0, 0};
*/

int rPosition, gPosition, bPosition, aPosition;
float rSpeed, gSpeed, bSpeed, aSpeed;

void loop()
{
  //DNS
  dnsServer.processNextRequest();
  //HTTP
  server.handleClient();

  rSpeed = getSpeed(rSpeed, settings.RedChangeRateMin, settings.RedChangeRateMax);
  gSpeed = getSpeed(gSpeed, settings.GreenChangeRateMin, settings.GreenChangeRateMax);
  bSpeed = getSpeed(bSpeed, settings.BlueChangeRateMin, settings.BlueChangeRateMax);
  aSpeed = getSpeed(aSpeed, settings.BrightnessChangeRateMin, settings.BrightnessChangeRateMax);
  rPosition += rSpeed;
  gPosition += gSpeed;
  bPosition += bSpeed;
  aPosition += aSpeed;
  int r = getValue(rPosition, settings.RedMin, settings.RedMax);
  int g = getValue(gPosition, settings.GreenMin, settings.GreenMax);
  int b = getValue(bPosition, settings.BlueMin, settings.BlueMax);
  float a = getValue(aPosition, settings.BrightnessMin, settings.BrightnessMax) / (float) 255;
  //Serial.println(String("r ") + r + String(" - g ") + g + String(" - b ") + b + String(" - a ") + a);
  uint32_t c = strip.Color(r * a, g * a, b * a);
  for (int i=0; i < NUMPIXELS; i++)
    strip.setPixelColor(i, c);

  strip.show();
  delay(22);
}

int getValue(float position, int min, int max)
{
	return min + ((cos(position /1000.f) + 1.f) * 0.5f * (max-min));
}

float getSpeed(float speed, int minVal, int maxVal)
{
  float rand = (random(1000) - 500) / 500.f;
  float newVal = minVal + (rand * (maxVal-minVal));
  return ((9 * speed) + newVal) / 10.f;
}

float clamp(float inV, float minV, float maxV)
{
  return max(min(inV, maxV), minV);
}


