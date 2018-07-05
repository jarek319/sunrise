#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#define LIGHT_PIN 4
#define BUTTON_PIN 0

const char ssid[] = "ssid";  //  your network SSID (name)
const char pass[] = "password";       // your network password
const int sunriseLength = 15; //how many minutes sunrise should take to reach full brightness at wakeTime
const int sunriseUptime = 15; //how many minutes sunrise should stay up at full brightness
uint32_t wakeTime = 1478520000; //UTC Unix time that sunrise should finish
//to get this value, put in the time you want sunrise to occur in the fields in http://www.epochconverter.com/
//make sure the dropdown field is set to Local Time, and don't worry about the date
//copy and paste the Epoch Timestamp value into the variable above

unsigned int localPort = 2390;      // local port to listen for UDP packets
const char* ntpServerName = "time.nist.gov";
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
//IPAddress timeServer(129, 6, 15, 28);
IPAddress timeServerIP; // time.nist.gov NTP server address
WiFiUDP udp; // A UDP instance to let us send and receive packets over UDP
byte packetBuffer[ NTP_PACKET_SIZE ]; //buffer to hold incoming and outgoing packets
uint32_t unixTime = 0;

void getTime() {
  int cb = 0;
  WiFi.hostByName(ntpServerName, timeServerIP); //get a random server from the pool
  sendNTPpacket(timeServerIP); // send an NTP packet to a time server
  delay(10000);// wait to see if a reply is available
  cb = udp.parsePacket();
  if (!cb) {
    Serial.println("no packet yet");
    ESP.deepSleep(10000000);
  }
  Serial.print("packet received, length="); // We've received a packet, read the data from it
  Serial.println(cb);
  udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
  //the timestamp starts at byte 40 of the received packet and is four bytes,
  // or two words, long. First, esxtract the two words:
  unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
  unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
  // combine the four bytes (two words) into a long integer
  // this is NTP time (seconds since Jan 1 1900):
  unsigned long secsSince1900 = highWord << 16 | lowWord;
  Serial.print("Unix time = ");
  const unsigned long seventyYears = 2208988800UL;
  unsigned long epoch = secsSince1900 - seventyYears;
  unixTime = epoch % 86400;
  Serial.println(unixTime);
}

void beginSunrise() {
  uint32_t startTime = unixTime;
  uint32_t endTime = wakeTime + ( sunriseUptime * 60 );
  int brightness = 0;
  Serial.print("Start Time: ");
  Serial.println( startTime );
  Serial.print("Sunrise Length: ");
  Serial.println( ( sunriseLength * 60 ) );
  for ( uint32_t i = 0; i < ( sunriseLength * 60 ); i++ ) {
    unsigned long timeout = millis() + 1000;
    if ( !digitalRead( BUTTON_PIN ) ) ESP.deepSleep( 3600000000 );
    brightness = map( i, 0, ( sunriseLength * 60 ), 0, 965 );
    brightness = constrain( brightness, 0, 965 );
    brightness = 965 - brightness;
    Serial.print("Brightness: ");
    Serial.println( brightness );
    // 0 - ON
    // 965 - OFF
    analogWrite( LIGHT_PIN, brightness );
    while ( millis() < timeout ) yield();
  }
}

void setup() {
  wakeTime = wakeTime % 86400;
  pinMode( LIGHT_PIN, OUTPUT );
  pinMode( BUTTON_PIN, INPUT_PULLUP );
  digitalWrite( LIGHT_PIN, HIGH );
  Serial.begin(115200);
  Serial.println();
  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  WiFi.mode(WIFI_STA);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());
  getTime();
  Serial.print("Unix Time: ");
  Serial.println( unixTime );
  Serial.print("Wake Time: ");
  Serial.println( wakeTime );
  Serial.print("Sunrise Start Time: ");
  Serial.println( ( wakeTime - ( sunriseLength * 60 )) );
  if (( unixTime > ( wakeTime - ( sunriseLength * 60 ))) && ( unixTime < ( wakeTime + ( sunriseUptime * 60 )))) {
    beginSunrise();
    digitalWrite( LIGHT_PIN, HIGH );
  }
  Serial.print("Sleeping...");
  digitalWrite( LIGHT_PIN, HIGH );
  ESP.deepSleep(60000000);
}

void loop() {
}

unsigned long sendNTPpacket(IPAddress & address) { // send an NTP request to the time server at the given address
  Serial.println("sending NTP packet...");
  memset(packetBuffer, 0, NTP_PACKET_SIZE); // Initialize values needed to form NTP request (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  packetBuffer[12]  = 49; // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  udp.beginPacket(address, 123); //all NTP fields have been given values, now you can send a packet requesting a timestamp. NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}
